#include <kernel/app_loader.h>

#include <kernel/debug.h>
#include <kernel/framebuffer.h>
#include <kernel/graphics.h>
#include <kernel/heap.h>
#include <kernel/keyboard.h>
#include <kernel/process.h>
#include <kernel/scheduler.h>
#include <kernel/syscalls.h>
#include <kernel/vfs.h>
#include <utils.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace
{
// Minimal ELF32 definitions (sufficient for relocatable objects)
struct Elf32_Ehdr
{
    unsigned char e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};

struct Elf32_Shdr
{
    uint32_t sh_name;
    uint32_t sh_type;
    uint32_t sh_flags;
    uint32_t sh_addr;
    uint32_t sh_offset;
    uint32_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint32_t sh_addralign;
    uint32_t sh_entsize;
};

struct Elf32_Sym
{
    uint32_t st_name;
    uint32_t st_value;
    uint32_t st_size;
    unsigned char st_info;
    unsigned char st_other;
    uint16_t st_shndx;
};

struct Elf32_Rel
{
    uint32_t r_offset;
    uint32_t r_info;
};

constexpr uint16_t ET_REL = 1;
constexpr uint16_t EM_386 = 3;

constexpr uint32_t SHT_NULL = 0;
constexpr uint32_t SHT_PROGBITS = 1;
constexpr uint32_t SHT_SYMTAB = 2;
constexpr uint32_t SHT_STRTAB = 3;
constexpr uint32_t SHT_RELA = 4;
constexpr uint32_t SHT_NOBITS = 8;
constexpr uint32_t SHT_REL = 9;

constexpr uint32_t SHF_WRITE = 0x1;
constexpr uint32_t SHF_ALLOC = 0x2;
constexpr uint32_t SHF_EXECINSTR = 0x4;

constexpr uint16_t SHN_UNDEF = 0;
constexpr uint16_t SHN_ABS = 0xFFF1;

#define ELF32_ST_BIND(i) ((i) >> 4)
#define ELF32_ST_TYPE(i) ((i)&0xF)
#define ELF32_R_SYM(info) ((info) >> 8)
#define ELF32_R_TYPE(info) ((uint8_t)(info))

constexpr uint8_t STB_LOCAL = 0;
constexpr uint8_t STB_GLOBAL = 1;

constexpr uint8_t STT_NOTYPE = 0;
constexpr uint8_t STT_OBJECT = 1;
constexpr uint8_t STT_FUNC = 2;

constexpr uint8_t R_386_NONE = 0;
constexpr uint8_t R_386_32 = 1;
constexpr uint8_t R_386_PC32 = 2;

struct KernelSymbol
{
    const char* name;
    void* address;
};

static KernelSymbol g_kernel_symbols[] = {
    {"_Z11kb_to_ascii14keyboard_event", (void*)&kb_to_ascii},
    {"_Z11sys_getcharv", (void*)&sys_getchar},
    {"_Z17sys_console_writePKcm", (void*)&sys_console_write},
    {"_Z8sys_openPKc", (void*)&sys_open},
    {"_Z8sys_readiPhm", (void*)&sys_read},
    {"_Z9sys_writeiPKhm", (void*)&sys_write},
    {"_Z9sys_closei", (void*)&sys_close},
    {"_Z5uitoajPci", (void*)&uitoa},
};

static void* resolve_kernel_symbol(const char* name)
{
    for (size_t i = 0; i < sizeof(g_kernel_symbols) / sizeof(g_kernel_symbols[0]); ++i)
    {
        if (strcmp(g_kernel_symbols[i].name, name) == 0)
        {
            return g_kernel_symbols[i].address;
        }
    }
    return nullptr;
}

static uint8_t* allocate_aligned(size_t size, uint32_t alignment)
{
    if (alignment == 0)
    {
        alignment = sizeof(void*);
    }
    if (alignment & (alignment - 1))
    {
        // Ensure power of two
        uint32_t pow2 = 1;
        while (pow2 < alignment)
        {
            pow2 <<= 1;
        }
        alignment = pow2;
    }
    size_t total = size + alignment;
    uint8_t* raw = static_cast<uint8_t*>(kmalloc(total));
    if (!raw)
    {
        return nullptr;
    }
    uintptr_t addr = reinterpret_cast<uintptr_t>(raw);
    uintptr_t aligned = (addr + (alignment - 1)) & ~(static_cast<uintptr_t>(alignment - 1));
    return reinterpret_cast<uint8_t*>(aligned);
}

static bool is_valid_elf(const Elf32_Ehdr& header)
{
    return header.e_ident[0] == 0x7F && header.e_ident[1] == 'E' && header.e_ident[2] == 'L' &&
           header.e_ident[3] == 'F';
}

} // namespace

Process* app_load_and_start(const char* path,
                            const char* process_name,
                            const AppLoadParams* params,
                            const char* init_argument)
{
    if (!path || !process_name || !params || !params->entry_symbol)
    {
        error("[app] invalid parameters");
        return nullptr;
    }

    AppLoadParams config = *params;
    if (config.stack_size == 0)
    {
        config.stack_size = 8192;
    }

    vfs_dirent_t info;
    if (vfs_stat(path, &info) != VFS_SUCCESS)
    {
        error("[app] failed to stat '%s'", path);
        return nullptr;
    }

    if (info.size == 0)
    {
        error("[app] file '%s' is empty", path);
        return nullptr;
    }

    uint8_t* file_data = static_cast<uint8_t*>(kmalloc(info.size));
    if (!file_data)
    {
        error("[app] out of memory reading '%s'", path);
        return nullptr;
    }

    vfs_file_t file;
    if (vfs_open(path, &file) != VFS_SUCCESS)
    {
        error("[app] failed to open '%s'", path);
        kfree(file_data);
        return nullptr;
    }

    size_t total_read = 0;
    while (total_read < info.size)
    {
        int chunk = vfs_read(&file, file_data + total_read, info.size - total_read);
        if (chunk <= 0)
        {
            error("[app] read error while loading '%s'", path);
            vfs_close(&file);
            kfree(file_data);
            return nullptr;
        }
        total_read += static_cast<size_t>(chunk);
    }
    vfs_close(&file);

    if (total_read < sizeof(Elf32_Ehdr))
    {
        error("[app] invalid ELF header in '%s'", path);
        kfree(file_data);
        return nullptr;
    }

    const Elf32_Ehdr& ehdr = *reinterpret_cast<const Elf32_Ehdr*>(file_data);
    if (!is_valid_elf(ehdr) || ehdr.e_type != ET_REL || ehdr.e_machine != EM_386)
    {
        error("[app] unsupported ELF object '%s'", path);
        kfree(file_data);
        return nullptr;
    }

    if (ehdr.e_shoff == 0 || ehdr.e_shentsize != sizeof(Elf32_Shdr))
    {
        error("[app] malformed section table in '%s'", path);
        kfree(file_data);
        return nullptr;
    }

    const Elf32_Shdr* sh_table =
        reinterpret_cast<const Elf32_Shdr*>(file_data + ehdr.e_shoff);
    uint16_t section_count = ehdr.e_shnum;

    uint8_t** section_memory =
        static_cast<uint8_t**>(kmalloc(sizeof(uint8_t*) * section_count));
    if (!section_memory)
    {
        error("[app] out of memory allocating section table");
        kfree(file_data);
        return nullptr;
    }
    memset(section_memory, 0, sizeof(uint8_t*) * section_count);

    const char* shstr_base = nullptr;
    if (ehdr.e_shstrndx < section_count)
    {
        const Elf32_Shdr& shstr = sh_table[ehdr.e_shstrndx];
        shstr_base = reinterpret_cast<const char*>(file_data + shstr.sh_offset);
    }

    // Allocate sections that need to reside in memory
    for (uint16_t i = 0; i < section_count; ++i)
    {
        const Elf32_Shdr& sh = sh_table[i];
        if (!(sh.sh_flags & SHF_ALLOC) || sh.sh_type == SHT_NULL)
        {
            continue;
        }

        uint8_t* dest = allocate_aligned(sh.sh_size, sh.sh_addralign);
        if (!dest)
        {
            error("[app] failed to allocate section %u (size=%u)", i, sh.sh_size);
            kfree(section_memory);
            kfree(file_data);
            return nullptr;
        }

        if (sh.sh_type == SHT_NOBITS)
        {
            memset(dest, 0, sh.sh_size);
        }
        else
        {
            memcpy(dest, file_data + sh.sh_offset, sh.sh_size);
        }
        section_memory[i] = dest;
    }

    const Elf32_Sym* symtab = nullptr;
    size_t sym_count = 0;
    const char* sym_strtab = nullptr;

    for (uint16_t i = 0; i < section_count; ++i)
    {
        const Elf32_Shdr& sh = sh_table[i];
        if (sh.sh_type == SHT_SYMTAB)
        {
            symtab = reinterpret_cast<const Elf32_Sym*>(file_data + sh.sh_offset);
            sym_count = sh.sh_size / sizeof(Elf32_Sym);
            if (sh.sh_link < section_count)
            {
                const Elf32_Shdr& str = sh_table[sh.sh_link];
                sym_strtab = reinterpret_cast<const char*>(file_data + str.sh_offset);
            }
            break;
        }
    }

    if (!symtab || !sym_strtab)
    {
        error("[app] missing symbol table in '%s'", path);
        kfree(section_memory);
        kfree(file_data);
        return nullptr;
    }

    // Apply relocations
    for (uint16_t i = 0; i < section_count; ++i)
    {
        const Elf32_Shdr& rel_sh = sh_table[i];
        if (rel_sh.sh_type != SHT_REL)
        {
            continue;
        }

        if (rel_sh.sh_info >= section_count)
        {
            continue;
        }

        const Elf32_Shdr& target_sh = sh_table[rel_sh.sh_info];
        if (!(target_sh.sh_flags & SHF_ALLOC))
        {
            // Skip relocations for non-loaded sections (e.g., debug info)
            continue;
        }

        uint8_t* target_base = section_memory[rel_sh.sh_info];
        if (!target_base)
        {
            error("[app] relocation target section %u not allocated", rel_sh.sh_info);
            kfree(section_memory);
            kfree(file_data);
            return nullptr;
        }

        if (rel_sh.sh_link >= section_count)
        {
            error("[app] relocation %u refers to invalid symtab", i);
            kfree(section_memory);
            kfree(file_data);
            return nullptr;
        }

        const Elf32_Shdr& rel_symtab_sh = sh_table[rel_sh.sh_link];
        const Elf32_Sym* rel_symtab =
            reinterpret_cast<const Elf32_Sym*>(file_data + rel_symtab_sh.sh_offset);
        const Elf32_Shdr& rel_strtab_sh = sh_table[rel_symtab_sh.sh_link];
        const char* rel_strtab =
            reinterpret_cast<const char*>(file_data + rel_strtab_sh.sh_offset);

        size_t rel_count = rel_sh.sh_size / sizeof(Elf32_Rel);
        const Elf32_Rel* rel_entries =
            reinterpret_cast<const Elf32_Rel*>(file_data + rel_sh.sh_offset);

        for (size_t r = 0; r < rel_count; ++r)
        {
            const Elf32_Rel& rel = rel_entries[r];
            uint32_t type = ELF32_R_TYPE(rel.r_info);
            uint32_t sym_index = ELF32_R_SYM(rel.r_info);

            const Elf32_Sym& sym = rel_symtab[sym_index];
            uint32_t location_addr =
                reinterpret_cast<uint32_t>(target_base + rel.r_offset);
            uint32_t addend = *reinterpret_cast<uint32_t*>(target_base + rel.r_offset);

            uint32_t symbol_value = 0;
            if (sym.st_shndx == SHN_UNDEF)
            {
                const char* name = rel_strtab + sym.st_name;
                void* resolved = resolve_kernel_symbol(name);
                if (!resolved)
                {
                    error("[app] unresolved symbol '%s'", name);
                    kfree(section_memory);
                    kfree(file_data);
                    return nullptr;
                }
                symbol_value = reinterpret_cast<uint32_t>(resolved);
            }
            else if (sym.st_shndx == SHN_ABS)
            {
                symbol_value = sym.st_value;
            }
            else if (sym.st_shndx < section_count)
            {
                uint8_t* base = section_memory[sym.st_shndx];
                if (!base)
                {
                    // Non-allocated section: treat as located in the file buffer
                    base = const_cast<uint8_t*>(file_data + sh_table[sym.st_shndx].sh_offset);
                }
                symbol_value = reinterpret_cast<uint32_t>(base + sym.st_value);
            }
            else
            {
                error("[app] invalid symbol section index %u", sym.st_shndx);
                kfree(section_memory);
                kfree(file_data);
                return nullptr;
            }

            switch (type)
            {
            case R_386_NONE:
                break;
            case R_386_32:
                *reinterpret_cast<uint32_t*>(target_base + rel.r_offset) =
                    symbol_value + addend;
                break;
            case R_386_PC32:
                *reinterpret_cast<uint32_t*>(target_base + rel.r_offset) =
                    symbol_value + addend - location_addr;
                break;
            default:
                error("[app] unsupported relocation type %u", type);
                kfree(section_memory);
                kfree(file_data);
                return nullptr;
            }
        }
    }

    // Locate entry and init symbols
    void (*entry_func)() = nullptr;
    void (*init_func)(const char*) = nullptr;

    for (size_t i = 0; i < sym_count; ++i)
    {
        const Elf32_Sym& sym = symtab[i];
        if (sym.st_name == 0)
        {
            continue;
        }

        const char* name = sym_strtab + sym.st_name;
        uint32_t value = 0;

        if (sym.st_shndx == SHN_UNDEF)
        {
            continue;
        }
        else if (sym.st_shndx == SHN_ABS)
        {
            value = sym.st_value;
        }
        else if (sym.st_shndx < section_count)
        {
            uint8_t* base = section_memory[sym.st_shndx];
            if (!base)
            {
                base = const_cast<uint8_t*>(file_data + sh_table[sym.st_shndx].sh_offset);
            }
            value = reinterpret_cast<uint32_t>(base + sym.st_value);
        }
        else
        {
            continue;
        }

        if (strcmp(name, config.entry_symbol) == 0)
        {
            entry_func = reinterpret_cast<void (*)()>(value);
        }
        else if (config.init_symbol && strcmp(name, config.init_symbol) == 0)
        {
            init_func = reinterpret_cast<void (*)(const char*)>(value);
        }
    }

    if (!entry_func)
    {
        error("[app] entry symbol '%s' not found in '%s'", config.entry_symbol, path);
        kfree(section_memory);
        kfree(file_data);
        return nullptr;
    }

    // Run static constructors if present
    if (shstr_base)
    {
        for (uint16_t i = 0; i < section_count; ++i)
        {
            const Elf32_Shdr& sh = sh_table[i];
            if (!(sh.sh_flags & SHF_ALLOC))
            {
                continue;
            }
            const char* sec_name = shstr_base + sh.sh_name;
            if (strcmp(sec_name, ".ctors") == 0 && section_memory[i])
            {
                size_t count = sh.sh_size / sizeof(void*);
                void (**ctors)() = reinterpret_cast<void (**)()>(section_memory[i]);
                for (size_t c = 0; c < count; ++c)
                {
                    if (ctors[c])
                    {
                        ctors[c]();
                    }
                }
            }
        }
    }

    if (init_func && init_argument)
    {
        init_func(init_argument);
    }

    kfree(section_memory);
    kfree(file_data);

    Process* proc = k_start_process(process_name, entry_func, 0, config.stack_size);
    if (!proc)
    {
        error("[app] failed to start process '%s'", process_name);
        return nullptr;
    }

    return proc;
}
