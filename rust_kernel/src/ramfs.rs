use crate::heap::kmalloc;
use core::ptr::null_mut;

const MAX_NAME: usize = 32;
const MAX_CHILDREN: usize = 16;
const MAX_OPEN_FILES: usize = 16;

#[derive(PartialEq, Eq, Copy, Clone)]
pub enum NodeType { File, Directory }

pub struct FSNode {
    pub name: [u8; MAX_NAME],
    pub node_type: NodeType,
    pub size: usize,
    pub data: *mut u8,
    pub parent: *mut FSNode,
    pub children: [*mut FSNode; MAX_CHILDREN],
    pub child_count: usize,
}

#[derive(Clone, Copy)]
pub struct FileDescriptor {
    pub node: *mut FSNode,
    pub offset: usize,
    pub used: bool,
}

static mut ROOT: *mut FSNode = null_mut();
static mut FDS: [FileDescriptor; MAX_OPEN_FILES] = [FileDescriptor { node: null_mut(), offset: 0, used: false }; MAX_OPEN_FILES];

pub unsafe fn fs_init() {
    ROOT = fs_create_node(b"/", NodeType::Directory);
}

pub unsafe fn fs_create_node(name: &[u8], ty: NodeType) -> *mut FSNode {
    let node = kmalloc(core::mem::size_of::<FSNode>()) as *mut FSNode;
    if node.is_null() { return null_mut(); }
    (*node).name = [0; MAX_NAME];
    for (i, c) in name.iter().enumerate().take(MAX_NAME-1) { (*node).name[i] = *c; }
    (*node).node_type = ty;
    (*node).size = 0;
    (*node).data = null_mut();
    (*node).parent = null_mut();
    (*node).children = [null_mut(); MAX_CHILDREN];
    (*node).child_count = 0;
    node
}

pub unsafe fn fs_add_child(parent: *mut FSNode, child: *mut FSNode) {
    if parent.is_null() || child.is_null() { return; }
    if (*parent).child_count < MAX_CHILDREN {
        (*parent).children[(*parent).child_count] = child;
        (*parent).child_count += 1;
        (*child).parent = parent;
    }
}

pub unsafe fn fs_find_child(parent: *mut FSNode, name: &[u8]) -> *mut FSNode {
    if parent.is_null() { return null_mut(); }
    for i in 0..(*parent).child_count {
        let child = (*parent).children[i];
        let cname = &(*child).name;
        if cname.iter().zip(name.iter()).all(|(a,b)| *a == *b) && name.len() <= cname.len() { return child; }
    }
    null_mut()
}

pub unsafe fn fs_touch(path: &str) -> *mut FSNode {
    let name = path.as_bytes();
    let file = fs_create_node(name, NodeType::File);
    if file.is_null() { return null_mut(); }
    (*file).data = kmalloc(1024); // 1KiB per file
    fs_add_child(ROOT, file);
    file
}

pub unsafe fn fs_open(node: *mut FSNode) -> i32 {
    for i in 0..MAX_OPEN_FILES {
        if !FDS[i].used {
            FDS[i].node = node;
            FDS[i].offset = 0;
            FDS[i].used = true;
            return i as i32;
        }
    }
    -1
}

pub unsafe fn fs_read(fd: i32, buf: *mut u8, size: usize) -> isize {
    if fd < 0 || fd as usize >= MAX_OPEN_FILES { return -1; }
    let desc = &mut FDS[fd as usize];
    if !desc.used { return -1; }
    let node = desc.node;
    if node.is_null() || (*node).node_type != NodeType::File { return -1; }
    let remaining = if desc.offset + size > (*node).size { (*node).size - desc.offset } else { size };
    core::ptr::copy((*node).data.add(desc.offset), buf, remaining);
    desc.offset += remaining;
    remaining as isize
}

pub unsafe fn fs_write(fd: i32, buf: *const u8, size: usize) -> isize {
    if fd < 0 || fd as usize >= MAX_OPEN_FILES { return -1; }
    let desc = &mut FDS[fd as usize];
    if !desc.used { return -1; }
    let node = desc.node;
    if node.is_null() || (*node).node_type != NodeType::File { return -1; }
    core::ptr::copy(buf, (*node).data.add(desc.offset), size);
    desc.offset += size;
    if desc.offset > (*node).size { (*node).size = desc.offset; }
    size as isize
}

pub unsafe fn fs_close(fd: i32) {
    if fd < 0 || fd as usize >= MAX_OPEN_FILES { return; }
    FDS[fd as usize].used = false;
}

pub unsafe fn fs_root() -> *mut FSNode { ROOT }

