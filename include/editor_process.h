#ifndef EDITOR_PROCESS_H
#define EDITOR_PROCESS_H
#include <kernel/ramfs.h>
#ifdef __cplusplus
extern "C" {
#endif
extern const char* editor_filename_global;
extern FSNode* editor_dir_global;
void editor_entry();
#ifdef __cplusplus
}
#endif
#endif // EDITOR_PROCESS_H
