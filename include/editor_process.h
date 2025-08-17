#ifndef EDITOR_PROCESS_H
#define EDITOR_PROCESS_H
#include <kernel/ramfs.h>
#ifdef __cplusplus
extern "C" {
#endif
// Set editor parameters before starting the process
void editor_set_params(const char* filename, FSNode* dir);
// Editor process entry point
void editor_entry();
#ifdef __cplusplus
}
#endif
#endif // EDITOR_PROCESS_H
