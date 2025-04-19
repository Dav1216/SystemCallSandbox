> ⚠️ Requires **Linux kernel 5.11+** for `PR_SET_SYSCALL_USER_DISPATCH`.

# 🛡️ Syscall User Dispatch (SUD) Enforcer

A minimal sandbox that uses **Syscall User Dispatch** to restrict Linux x86-64 programs to only *allowed* system calls. Designed for tracing and enforcing syscall behavior at runtime — without using `seccomp`.

## 🚀 Quick Start

```bash
make          # Build everything (tracer, dummy, sandbox, attacker)
make trace    # Trace dummy and log syscalls to policy_file.txt
make run      # Run attacker with sandbox preloaded
make clean    # Clean up generated files
