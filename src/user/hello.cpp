#include <stdio.h>
#include <process.h>
#include <sys/scheduler.h>

extern "C" void hello_entry()
{
    int pid = scheduler_getpid();
    printf("[hello] Greetings from user app! (pid=%d)\n", pid);
    process_exit(0);

    while (1)
    {
        asm volatile("hlt");
    }
}
