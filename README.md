## Launch Buddies

A program that launches two other programs in order. I use it to run [BCC tools](https://github.com/iovisor/bcc/tree/master/tools) on short-lived processes, most of which accept a `-p` argument but do not support launching commands like `perf` does.

### How it works

The first program gets launched with `fork()`. After `fork()` returns, the child process performs a blocking read on a pipe created by the parent.

The second program also gets launched with `fork()` and immediately calls `exec()`. The parent process sleeps for 5 seconds and then sends data on a pipe to the first process. Once the first process reads data from the pipe, it calls `exec()`.

The parent waits for its children. After the first child exits, the parent will send a signal (currently `SIGINT`) to the second process to kill that one.

### Compiling

Make sure you have `gcc` and `make` installed. Then run:

```
$ make
```

### Alternatives

If you use `bpftrace`, you can just filter by `comm` (process name) and launch your program in another window.

```
$ sudo bpftrace -e 'kprobe:vfs_read /comm=="ls"/ { printf("vs_read called from %d\n", pid); }'
```

Sometimes programs will offer a `-n` where you can filter by process name.

### Using

The general use is:

```
$ lb [cmd1] [cmd2] [pid-arg-flag]
```

Here's how you could use it to launch `ls` and `profile-bpfcc`.

```
$ sudo lb "ls -lah" "profile-bpfcc" "-p "
Child launched with pid 483635
Profiler launched with pid 483636
Parent sleeping while profiler launches...
launching profile-bpfcc -F 999 -p 483635
Sampling at 999 Hertz of PID 483635 by user + kernel stack... Hit Ctrl-C to end.
Received ?, launching child...
total 88K
drwxrwxr-x  3 petermalmgren petermalmgren 4.0K Nov 20 18:07 .
drwxr-xr-x 24 petermalmgren petermalmgren 4.0K Nov 20 17:32 ..
drwxrwxr-x  8 petermalmgren petermalmgren 4.0K Nov 20 17:35 .git
-rw-rw-r--  1 petermalmgren petermalmgren  430 Nov 20 17:35 .gitignore
-rwxrwxr-x  1 petermalmgren petermalmgren  18K Nov 20 17:48 lb
-rw-rw-r--  1 petermalmgren petermalmgren 5.2K Nov 20 17:30 lb.c
-rw-rw-r--  1 petermalmgren petermalmgren  35K Nov 20 17:35 LICENSE
-rw-rw-r--  1 petermalmgren petermalmgren   77 Nov 20 17:48 Makefile
-rw-rw-r--  1 petermalmgren petermalmgren 1.2K Nov 20 18:01 README.md
Child finished, killing profiler...

    b'apparmor_file_free_security'
    b'apparmor_file_free_security'
    b'security_file_free'
    b'__fput'
    b'____fput'
    b'task_work_run'
    b'exit_to_usermode_loop'
    b'__prepare_exit_to_usermode'
    b'__syscall_return_slowpath'
    b'do_syscall_64'
    b'entry_SYSCALL_64_after_hwframe'
    [unknown]
    -                ls (483635)
        1

Profiler finished, exiting...
```
