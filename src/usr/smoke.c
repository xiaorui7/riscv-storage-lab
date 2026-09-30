#include "syscall.h"

/* Rebuilt from source: proves ELF entry, user mode, print syscall and exit. */
void main(void) {
    _print("[PASS] user_exec_print\n");
    _print("[DONE] smoke 1\n");
    _exit();
}
