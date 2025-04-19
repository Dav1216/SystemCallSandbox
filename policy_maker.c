#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>
#include <unistd.h>
#include <sys/ptrace.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <signal.h>

#define MAX_SYSCALL_NR 332
#define POLICY_FILENAME "policy_file.txt"

/* Simple error‐reporting macro */
#define FATAL(msg) do { perror(msg); exit(EXIT_FAILURE); } while (0)

/* Print usage and exit */
static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s [--runs N] executable\n", prog);
    exit(EXIT_FAILURE);
}

/* Parse command‐line, set *out_exec and *out_runs */
static void parse_args(int argc, char *argv[], char **out_exec, int *out_runs) {
    *out_runs = 5;
    if (argc < 2) {
        usage(argv[0]);
    }

    /* If the last-but-one arg is "--runs", parse its value */
    if (argc >= 4 && strcmp(argv[argc-2], "--runs") == 0) {
        *out_runs = atoi(argv[argc-1]);
        if (*out_runs <= 0) {
            fprintf(stderr, "Invalid run count: %s\n", argv[argc-1]);
            exit(EXIT_FAILURE);
        }
        *out_exec = argv[1];
    } else {
        *out_exec = argv[1];
    }
}

/* Trace one child run, logging any new syscalls into 'seen' and to 'outf' */
static void trace_one(const char *exec_path, bool seen[], FILE *outf) {
    pid_t child = fork();
    if (child < 0) {
        FATAL("fork");
    }

    if (child == 0) {
        /* Child: enable tracing and exec target */
        if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) < 0) {
            FATAL("ptrace TRACEME");
        }
        execlp(exec_path, exec_path, NULL);
        FATAL("execlp");
    }

    /* Parent */
    int status;
    struct user_regs_struct regs;

    /* Wait for initial stop */
    if (waitpid(child, &status, 0) < 0) {
        FATAL("waitpid");
    }

    /* Drive the child until it exits */
    while (WIFSTOPPED(status)) {
        /* Get registers */
        if (ptrace(PTRACE_GETREGS, child, NULL, &regs) < 0) {
            FATAL("ptrace GETREGS");
        }

        long nr = regs.orig_rax;
        if (nr >= 0 && nr <= MAX_SYSCALL_NR && !seen[nr]) {
            printf("Observed syscall %ld\n", nr);
            fprintf(outf, "%ld\n", nr);
            seen[nr] = true;
        }

        /* Let the syscall (entry or exit) complete */
        if (ptrace(PTRACE_SYSCALL, child, NULL, NULL) < 0) {
            FATAL("ptrace SYSCALL");
        }
        if (waitpid(child, &status, 0) < 0) {
            FATAL("waitpid");
        }
    }

    if (WIFEXITED(status)) {
        printf("Child exited with %d\n", WEXITSTATUS(status));
    }
}

int main(int argc, char *argv[]) {
    char *executable;
    int runs;
    parse_args(argc, argv, &executable, &runs);

    bool seen[MAX_SYSCALL_NR + 1] = { false };
    FILE *outf = fopen(POLICY_FILENAME, "w");
    if (!outf) {
        FATAL("fopen policy file");
    }

    for (int i = 0; i < runs; i++) {
        trace_one(executable, seen, outf);
    }

    fclose(outf);
    return 0;
}
