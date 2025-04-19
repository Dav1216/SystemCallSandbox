#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main()
{
    // allowed system call
    write(1, "HELLO WORD\n", 11);
    // simulated attack
    syscall(300, 0, 0, 0, 0, 0, 0);

    exit(0);
}