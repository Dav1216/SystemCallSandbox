#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/prctl.h>
#include <ucontext.h>     /* <- brings in the REG_* constants we need */

#ifndef POLICY_FILE
#define POLICY_FILE "policy_file.txt"
#endif

#define MAX_SYSCALL_NR 332

/* prctl selector byte (must be global) */
unsigned char selector = SYSCALL_DISPATCH_FILTER_ALLOW;

/* dispatcher symbols — still referenced by prctl later */
extern char syscall_dispatcher_start;
extern char syscall_dispatcher_end;

/* lookup table of allowed syscalls */
static bool allowed[MAX_SYSCALL_NR + 1];

/* naked helper to replay the trapped syscall */
__attribute__((naked))
long enter_syscall(long nr,long a1,long a2,long a3,long a4,long a5,long a6)
{
    asm volatile(
        "movq %rdi, %rax \n\t"
        "movq %rsi, %rdi \n\t"
        "movq %rdx, %rsi \n\t"
        "movq %rcx, %rdx \n\t"
        "movq %r8,  %r10 \n\t"
        "movq %r9,  %r8  \n\t"
        "movq 8(%rsp), %r9 \n\t"
        "syscall        \n\t"
        "ret            \n\t");
}

/* -------------------------------------------------- */
/*                   policy loading                   */
/* -------------------------------------------------- */
static void load_policy(const char *file)
{
    FILE *fp = fopen(file, "r");
    if (!fp) { perror("open policy file"); exit(EXIT_FAILURE); }

    int n;
    while (fscanf(fp, "%d", &n) == 1)
        if (0 <= n && n <= MAX_SYSCALL_NR)
            allowed[n] = true;
    fclose(fp);

    /* Syscalls the process must always be allowed to make */
    allowed[15]  = true;   /* rt_sigreturn */
    allowed[231] = true;   /* exit_group   */
}

/* -------------------------------------------------- */
/*                     SIGSYS handler                 */
/* -------------------------------------------------- */
static void sigsys_handler(int sig, siginfo_t *si, void *vctx)
{
    ucontext_t *uc = (ucontext_t *)vctx;
    long nr = uc->uc_mcontext.gregs[REG_RAX];

    selector = SYSCALL_DISPATCH_FILTER_ALLOW;   /* temporarily unblock */

    if (allowed[si->si_syscall]) {
        long args[6] = {
            uc->uc_mcontext.gregs[REG_RDI],
            uc->uc_mcontext.gregs[REG_RSI],
            uc->uc_mcontext.gregs[REG_RDX],
            uc->uc_mcontext.gregs[REG_R10],
            uc->uc_mcontext.gregs[REG_R8],
            uc->uc_mcontext.gregs[REG_R9],
        };
        uc->uc_mcontext.gregs[REG_RAX] =
            enter_syscall(nr,args[0],args[1],args[2],args[3],args[4],args[5]);
    } else {
        fprintf(stderr,"Blocked syscall %ld – terminating\n",nr);
        _exit(EXIT_FAILURE);
    }

    selector = SYSCALL_DISPATCH_FILTER_BLOCK;

    /* ---- tiny dispatcher stub ---- */
    asm volatile(
        "movq $0xf, %%rax       \n"
        "leaveq                 \n"
        "add  $0x8, %%rsp       \n"
        ".globl syscall_dispatcher_start \n"
        "syscall_dispatcher_start:       \n"
        "    syscall            \n"
        "    nop                \n"
        ".globl syscall_dispatcher_end   \n"
        "syscall_dispatcher_end:         \n"
        : : : "rsp","rax");
}

/* -------------------------------------------------- */
/*                  initialisation                    */
/* -------------------------------------------------- */
static void sandbox_init(void)
{
    load_policy(POLICY_FILE);

    struct sigaction sa = {0};
    sa.sa_sigaction = sigsys_handler;
    sa.sa_flags     = SA_SIGINFO | SA_RESTART;
    if (sigaction(SIGSYS,&sa,NULL) < 0) {
        perror("sigaction"); exit(EXIT_FAILURE);
    }

    unsigned long start = (unsigned long)&syscall_dispatcher_start;
    unsigned long len   = (unsigned long)&syscall_dispatcher_end - start;

    if (prctl(PR_SET_SYSCALL_USER_DISPATCH,
              PR_SYS_DISPATCH_ON,
              start, len, &selector) < 0)
    {
        perror("prctl PR_SET_SYSCALL_USER_DISPATCH"); exit(EXIT_FAILURE);
    }

    selector = SYSCALL_DISPATCH_FILTER_BLOCK;
}

__attribute__((constructor))
static void ctor(void) { sandbox_init(); }
