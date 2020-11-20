/**
 * COPYRIGHT: Copyright (c) Peter Malmgren 2020
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * (http://www.gnu.org/copyleft/gpl.html)
 *
 * 14-Nov-2020  Peter Malmgren  Created this.
 **/
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

void err_exit(char *reason) {
    fprintf(stderr, "%s: %s\n", reason, strerror(errno));
    exit(1);
}

void err_exit_child(char *reason) {
    fprintf(stderr, "%s: %s\n", reason, strerror(errno));
    _exit(1);
}

char* pid_itoa(pid_t target_pid) {
    int length = snprintf(NULL, 0, "%d", target_pid);
    char* s = malloc(length + 1);
    snprintf(s, length + 1, "%d", target_pid);
    return s;
}

int count_delims(char *s, char delimiter) {
    int num_delims = 0;
    char *c = s;

    while (*c++) {
        if (*c == delimiter)
            num_delims++;
    }

    return num_delims;
}

char** split(char *str, char delimiter) {
    int num_delims = count_delims(str, delimiter);
    int length = num_delims + 2;

    char **split_prog = malloc(sizeof(char*) * length);

    char *cp = strdup(str);
    char delimiters[1];
    delimiters[0] = delimiter;

    int i = 1;
    char *token;
    token = strtok(cp, delimiters);
    split_prog[0] = token;

    while (token != NULL) {
        token = strtok(NULL, delimiters);
        split_prog[i] = token;
        i++;
    }

    split_prog[i] = NULL;

    return split_prog;
}

void launch_target(char *argv, int read_pipe_fd) {
    char buf[1];
    if (read(read_pipe_fd, buf, 1) == -1)
        err_exit_child("read");

    if (close(read_pipe_fd) == -1)
        err_exit_child("close");

    fprintf(stderr, "Received %s, launching child...\n", buf);

    char **target = split(argv, ' ');

    execvp(target[0], target);

    err_exit("execlp");
}

void launch_profiler(char *argv, pid_t target_pid, char *pid_arg) {
    char *pid = pid_itoa(target_pid);

    // argv + length of pid_arg + length of pid + "\0"
    int new_len = strlen(argv) + strlen(pid_arg) + strlen(pid) + 2;
    char *target_str = malloc(sizeof(char) * new_len);

    int c = snprintf(target_str, new_len, "%s %s%s", argv, pid_arg, pid);

    fprintf(stderr, "launching %s\n", target_str);

    char **target = split(target_str, ' ');

    execvp(target[0], target);

    err_exit("execlp");
}

int
main (int argc, char **argv)
{
    if (argc != 3 && argc != 4) {
        fprintf(stderr, "Usage: ebpf-bench target \"bpf-script\" \"--pid={}\"\n");
        exit(1);
    }

    int sync_pipe[2];
    if (pipe(sync_pipe) == -1)
        err_exit("pipe");
    
    pid_t target_process;
    pid_t profile_process;

    target_process = fork();
    switch (target_process) {
        case -1:
            err_exit("fork");
            break;
        case 0:
            if (close(sync_pipe[1]) == -1)
                err_exit_child("close");
            launch_target(argv[1], sync_pipe[0]);
            break;
        default:
            fprintf(stderr, "Child launched with pid %d\n", target_process);
    }

    profile_process = fork();
    switch (profile_process) {
        case -1:
            err_exit("fork");
            break;
        case 0:
            if (close(sync_pipe[0]) == -1)
                err_exit_child("close");
            if (close(sync_pipe[1]) == -1)
                err_exit_child("close");
            char *pid_arg = argc == 4 ? argv[3] : "";
            launch_profiler(argv[2], target_process, pid_arg);
            break;
        default:
            fprintf(stderr, "Profiler launched with pid %d\n", profile_process);
            break;
    }

    if (close(sync_pipe[0]) == -1)
        err_exit("close");
    
    fprintf(stderr, "Parent sleeping while profiler launches...\n");
    // TODO: Replace with a signal handler
    sleep(5);

    // tell target to run execlp and start
    if (write(sync_pipe[1], "?", 1) != 1)
        err_exit("write");
    
    int child_finished = 0;
    int profiler_finished = 0;

    while (child_finished == 0 || profiler_finished == 0) {
        pid_t finished = waitpid(-1, NULL, 0);

        if (finished == target_process) {
            child_finished = 1;
            fprintf(stderr, "Child finished, killing profiler...\n");
            if (kill(profile_process, SIGINT) == -1)
                err_exit("kill");
        }

        if (finished == profile_process) {
            fprintf(stderr, "Profiler finished, exiting...\n");
            profiler_finished = 1;
        }
    }
}
