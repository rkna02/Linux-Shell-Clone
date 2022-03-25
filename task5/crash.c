#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>

#define MAXLINE 1024
#define MAXJOBS 1024

void cmd_jobs(const char**);
void insert_jobs(const char**, pid_t, bool);

char **environ;

// TODO: you will need some data structure(s) to keep track of jobs
// Job object consisting job id, pid, and name of command
typedef struct {
    char *id_s;
    int id;
    char *pid_s;
    pid_t pid;
    char *name;
    bool terminated;
    bool suspended; 
} job;

job jobs[1024];  // array of pointers to job objects
job curr;
int job_id = 0;  // job id reference numbers
int foreground_job = -1;

void handle_sigchld(int sig) {
    sigset_t mask;  
    sigfillset(&mask);  
    pid_t sig_pid;
    int status;
    
    while ((sig_pid = waitpid(-1, &status, WNOHANG|WUNTRACED)) > 0) {   
        if (WIFSTOPPED(status)) {
            int exit_status = WSTOPSIG(status);  
            if (exit_status == 19 || exit_status == 20) {
                for (int i = 0; i < job_id; i++) {
                    if (jobs[i].pid == sig_pid) {
                        sigprocmask(SIG_BLOCK, &mask, NULL);
                        jobs[foreground_job - 1].suspended = true;  // mark suspended (not terminated)
                        write(STDOUT_FILENO, "[", sizeof("["));
                        write(STDOUT_FILENO, jobs[i].id_s, sizeof(jobs[i].id_s));
                        write(STDOUT_FILENO, "] (", sizeof("] ("));
                        write(STDOUT_FILENO, jobs[i].pid_s, sizeof(jobs[i].pid_s));
                        write(STDOUT_FILENO, ")  suspended  ", sizeof(")  suspended  "));
                        write(STDOUT_FILENO, jobs[i].name, sizeof(jobs[i].name));
                        write(STDOUT_FILENO, "\n", sizeof("\n"));
                        sigprocmask(SIG_UNBLOCK, &mask, NULL);
                        break;
                    }
                }
            } 
        } 
        
        if (WIFSIGNALED(status)) {
            int exit_status = WTERMSIG(status);    
            if (exit_status == 2 || exit_status == 3 || exit_status == SIGKILL) {  
                for (int i = 0; i < job_id; i++) {
                    if (jobs[i].pid == sig_pid) {
                        sigprocmask(SIG_BLOCK, &mask, NULL);
                        jobs[i].terminated = true;  // mark terminated
                        write(STDOUT_FILENO, "[", sizeof("["));
                        write(STDOUT_FILENO, jobs[i].id_s, sizeof(jobs[i].id_s));
                        write(STDOUT_FILENO, "] (", sizeof("] ("));
                        write(STDOUT_FILENO, jobs[i].pid_s, sizeof(jobs[i].pid_s));
                        write(STDOUT_FILENO, ")  killed  ", sizeof(")  killed  "));
                        write(STDOUT_FILENO, jobs[i].name, sizeof(jobs[i].name));
                        write(STDOUT_FILENO, "\n", sizeof("\n"));
                        sigprocmask(SIG_UNBLOCK, &mask, NULL);  
                        break;
                    }  
                }
            } 
        } 

        if (WIFEXITED(status)) {
            for (int i = 0; i < job_id; i++) {
                if (jobs[i].pid == sig_pid) {
                    sigprocmask(SIG_BLOCK, &mask, NULL);
                    jobs[i].terminated = true;
                    sigprocmask(SIG_UNBLOCK, &mask, NULL);
                    break;
                }
            }
        }
    }
    
}

void handle_sigtstp(int sig) {
    //sigset_t mask;  
    //sigfillset(&mask);  
    //lsigprocmask(SIG_BLOCK, &mask, NULL);
    if (foreground_job != -1) {
        if (jobs[foreground_job - 1].terminated == 0 && jobs[foreground_job - 1].suspended == 0) {
            kill(jobs[foreground_job - 1].pid, SIGTSTP);
            //sigprocmask(SIG_BLOCK, &mask, NULL);
        } else {
            return;
        }
    } else {
        return;
    }
}

void handle_sigint(int sig) {
    if (foreground_job != -1) {
        if (jobs[foreground_job - 1].terminated == 0) {
            kill(jobs[foreground_job - 1].pid, SIGINT);
        } else {
            return;
        }
    } else {
        return;
    }
}

void handle_sigquit(int sig) {
    if (foreground_job != -1) {
        if (jobs[foreground_job - 1].terminated == 0) {
            kill(jobs[foreground_job - 1].pid, SIGQUIT);
            return;
        } else {
            _exit(0);
        }
    } else {
        _exit(0);
    }
}

void install_signal_handlers() {
    // install SIGCHLD
    struct sigaction chld;
    chld.sa_handler = handle_sigchld;
    chld.sa_flags = SA_RESTART;
    sigemptyset(&chld.sa_mask);
    sigaction(SIGCHLD, &chld, NULL);

    // install SIGINT
    struct sigaction intt;
    intt.sa_handler = handle_sigint;
    intt.sa_flags = SA_RESTART;
    sigemptyset(&intt.sa_mask);
    sigaction(SIGINT, &intt, NULL);

    // install SIGQUIT
    struct sigaction quit;
    quit.sa_handler = handle_sigquit;
    quit.sa_flags = SA_RESTART;
    sigfillset(&quit.sa_mask);
    sigaction(SIGQUIT, &quit, NULL);

    // install SIGTSTP
    struct sigaction tstp;
    tstp.sa_handler = handle_sigtstp;
    tstp.sa_flags = SA_RESTART;
    sigfillset(&tstp.sa_mask);
    sigaction(SIGTSTP, &tstp, NULL);
}

void spawn(const char **toks, bool bg) { // bg is true iff command ended with &
    sigset_t mask;  // sigset_t represent a signal set
    sigfillset(&mask);  // all recongnized signals are excluded
    sigprocmask(SIG_BLOCK, &mask, NULL);
    pid_t p1 = fork();  // fork results in two concurrent processes

    if (p1 == -1) {
        fprintf(stderr, "ERROR: cannot run %s\n", toks[0]);
        sigprocmask(SIG_UNBLOCK, &mask, NULL);
        exit(0);
    }
    if (p1 == 0) {
        sigprocmask(SIG_UNBLOCK, &mask, NULL);
        setpgid(0, 0);
        int success = execvp(toks[0], toks);
        if (success == -1) {
            fprintf(stderr, "ERROR: cannot run %s\n", toks[0]);
        }
        exit(0);
    } else {
        // update job status
        job_id = job_id + 1;
        insert_jobs(toks, p1, bg);

        // print job message if the command is not "kill"
        if (strcmp(jobs[job_id - 1].name, "kill") != 0) {
            printf("[%i] (%ld)  %s\n", job_id, (long) p1, jobs[job_id - 1].name);
        }

        sigprocmask(SIG_UNBLOCK, &mask, NULL);

        if (bg) {
            return;
        }
        if (!bg) {
            while (jobs[foreground_job - 1].terminated == false && jobs[foreground_job - 1].suspended == false) {
                sleep(0.01);
            }
        }
    }

}

void insert_jobs(const char **toks, pid_t pid, bool bg) {

    // job id
    jobs[job_id - 1].id = job_id;

    // job id (string version)
    int id_length = 0;  // length of job id
    int id_temp = job_id;  // temporary id used to count number of digits
    while(id_temp!=0)  {  
        id_temp = id_temp/10;  
        id_length++;  
    }  
    id_length++;
    jobs[job_id - 1].id_s = malloc(id_length * sizeof(char*));
    sprintf(jobs[job_id - 1].id_s, "%d", job_id);

    // pid 
    jobs[job_id - 1].pid = pid;

    // pid (string version)
    int pid_length = 0;
    int pid_temp = (int) pid;
    while(pid_temp!=0)  {  
        pid_temp = pid_temp/10;  
        pid_length++;  
    }  
    pid_length++;  // account for ending string char
    jobs[job_id - 1].pid_s = malloc(pid_length * sizeof(char*));
    sprintf(jobs[job_id - 1].pid_s, "%ld", (long) pid);

    // name 
    char *name_copy;
    name_copy = malloc((strlen(toks[0]) + 1));
    strcpy(name_copy, toks[0]);
    jobs[job_id - 1].name = name_copy;

    // terminated
    jobs[job_id - 1].terminated = false;

    // suspended
    jobs[job_id - 1].suspended = false;

    // update foreground job if appropriate 
    if (!bg) {
        foreground_job = jobs[job_id - 1].id;
    }

}

void cmd_jobs(const char **toks) {
    for (int i = 0; i < job_id; i++) {
        if (jobs[i].terminated == false) {
            if (jobs[i].suspended == false) {
                printf("[%d] (%ld)  running  %s\n", jobs[i].id, (long) jobs[i].pid, jobs[i].name);
            } else {
                printf("[%d] (%ld)  suspended  %s\n", jobs[i].id, (long) jobs[i].pid, jobs[i].name);
            }
        }
    }
}

void cmd_fg(const char **toks) {
    sigset_t mask;  
    sigfillset(&mask);  
    char *remaining = NULL;

    if (toks[1][0] == (char)'%') {
        strtol(toks[1] + 1, &remaining, 10);
        if (strcmp((toks[1] + strlen(toks[1])), remaining) != 0) {
            fprintf(stderr, "ERROR: bad argument for fg: %s\n", toks[1]);
        } else {
            for (int i = 0; i < job_id; i++) {
                if (strcmp(jobs[i].id_s, toks[1] + 1) == 0) {
                    sigprocmask(SIG_BLOCK, &mask, NULL);
                    if (jobs[i].suspended == true) {
                        foreground_job = jobs[i].id;
                        jobs[i].suspended = false;
                        kill(jobs[i].pid, SIGCONT);
                    } else {
                        foreground_job = jobs[i].id;
                    }
                    sigprocmask(SIG_UNBLOCK, &mask, NULL);
                    while (jobs[foreground_job - 1].terminated == false && jobs[foreground_job - 1].suspended == false) {
                        sleep(0.1);
                    }
                    return;
                }
            }
            fprintf(stderr, "ERROR: no job %s\n", toks[1]);
        }
    } else {
        strtol(toks[1], &remaining, 10);
        if (strcmp((toks[1] + strlen(toks[1])), remaining) != 0) {
            fprintf(stderr, "ERROR: bad argument for fg: %s\n", toks[1]);
        } else {
            for (int i = 0; i < job_id; i++) {
                if (strcmp(jobs[i].pid_s, toks[1]) == 0) {
                    sigprocmask(SIG_BLOCK, &mask, NULL);
                    if (jobs[i].suspended == true) {
                        foreground_job = jobs[i].id;
                        jobs[i].suspended = false;
                        kill(jobs[i].pid, SIGCONT);
                    } else {
                        foreground_job = jobs[i].id;
                    }
                    sigprocmask(SIG_UNBLOCK, &mask, NULL);
                    while (jobs[foreground_job - 1].terminated == false && jobs[foreground_job - 1].suspended == false) {
                        sleep(0.1);
                    }
                    return;
                }
            }
            fprintf(stderr, "ERROR: no PID %ld\n", (long) toks[1]);
        }
    }
}

void cmd_bg(const char **toks) {
    sigset_t mask;  
    sigfillset(&mask);  
    char *remaining = NULL;

    if (toks[1][0] == (char)'%') {
        strtol(toks[1] + 1, &remaining, 10);
        if (strcmp((toks[1] + strlen(toks[1])), remaining) != 0) {
            fprintf(stderr, "ERROR: bad argument for fg: %s\n", toks[1]);
        } else {
            for (int i = 0; i < job_id; i++) {
                if (strcmp(jobs[i].id_s, toks[1] + 1) == 0) {
                    sigprocmask(SIG_BLOCK, &mask, NULL);
                    if (jobs[i].suspended == true) {
                        kill(jobs[i].pid, SIGCONT);
                        jobs[i].suspended = false;
                        if (foreground_job == jobs[i].id) {
                            foreground_job = -1;
                        }
                    } 
                    sigprocmask(SIG_UNBLOCK, &mask, NULL);
                    return;
                }
            }
            fprintf(stderr, "ERROR: no job %s\n", toks[1]);
        }
    } else {
        strtol(toks[1], &remaining, 10);
        if (strcmp((toks[1] + strlen(toks[1])), remaining) != 0) {
            fprintf(stderr, "ERROR: bad argument for fg: %s\n", toks[1]);
        } else {
            for (int i = 0; i < job_id; i++) {
                if (strcmp(jobs[i].pid_s, toks[1]) == 0) {
                    sigprocmask(SIG_BLOCK, &mask, NULL);
                    if (jobs[i].suspended == true) {
                        kill(jobs[i].pid, SIGCONT);
                        jobs[i].suspended = false;
                        if (foreground_job == jobs[i].id) {
                            foreground_job = -1;
                        }
                    }
                    sigprocmask(SIG_UNBLOCK, &mask, NULL);
                    return;
                }
            }
            fprintf(stderr, "ERROR: no PID %ld\n", (long) toks[1]);
        }
    }
}

void cmd_slay(const char **toks) {
    sigset_t mask;  // sigset_t represent a signal set
    sigfillset(&mask);  // all recongnized signals are excluded
    char *remaining = NULL;

    if (toks[1][0] == (char)'%') {
        strtol(toks[1] + 1, &remaining, 10);
        if (strcmp((toks[1] + strlen(toks[1])), remaining) != 0) {
            fprintf(stderr, "ERROR: bad argument for fg: %s\n", toks[1]);
        } else {
            for (int i = 0; i < job_id; i++) {
                if (strcmp(jobs[i].id_s, toks[1] + 1) == 0) {
                    kill(jobs[i].pid, SIGKILL);
                    return;
                }
            }
            fprintf(stderr, "ERROR: no job %s\n", toks[1]);
        }
    } else {
        strtol(toks[1], &remaining, 10);
        if (strcmp((toks[1] + strlen(toks[1])), remaining) != 0) {
            fprintf(stderr, "ERROR: bad argument for fg: %s\n", toks[1]);
        } else {
            for (int i = 0; i < job_id; i++) {
                if (strcmp(jobs[i].pid_s, toks[1]) == 0) {
                    kill(jobs[i].pid, SIGKILL);
                    return;
                }
            }
            fprintf(stderr, "ERROR: no PID %ld\n", (long) toks[1]);
        }
    }

}

void cmd_quit(const char **toks) {
    if (toks[1] != NULL) {
        fprintf(stderr, "ERROR: quit takes no arguments\n");
    } else {
        exit(0);
    }
}

void eval(const char **toks, bool bg) { // bg is true iff command ended with &
    assert(toks);
    if (*toks == NULL) return;

    // quit
    if (strcmp(toks[0], "quit") == 0) {
        cmd_quit(toks);

    // jobs
    } else if (strcmp(toks[0], "jobs") == 0) {
        if (toks[1] != NULL) {
            fprintf(stderr, "ERROR: jobs takes no arguments\n");
        } else {
            cmd_jobs(toks);
        }

    // slay, fg, bg
    } else if (strcmp(toks[0], "slay") == 0) {
        if (toks[1] == NULL || toks[2] != NULL) {
            fprintf(stderr, "ERROR: slay takes exactly one argument\n");
        } else {
            cmd_slay(toks);
        }
    } else if (strcmp(toks[0], "fg") == 0) {
        if (toks[1] == NULL || toks[2] != NULL) {
            fprintf(stderr, "ERROR: fg takes exactly one argument\n");
        } else {
            cmd_fg(toks);
        }
    } else if (strcmp(toks[0], "bg") == 0) {
        if (toks[1] == NULL || toks[2] != NULL) {
            fprintf(stderr, "ERROR: bg takes exactly one argument\n");
        } else {
            cmd_bg(toks);
        }
    } else {
        spawn(toks, bg);
    }
}

// you don't need to touch this unless you are submitting the bonus task
void parse_and_eval(char *s) {
    assert(s);
    const char *toks[MAXLINE+1];
    
    while (*s != '\0') {
        bool end = false;
        bool bg = false;
        int t = 0;

        while (*s != '\0' && !end) {
            while (*s == ' ' || *s == '\t' || *s == '\n') ++s;  
            if (*s != '&' && *s != ';' && *s != '\0') toks[t++] = s;
            while (strchr("&; \t\n", *s) == NULL) ++s;
            switch (*s) {
            case '&':
                bg = true;
                end = true;
                break;
            case ';':
                end = true;
                break;
            }
            if (*s) *s++ = '\0';
        }
        toks[t] = NULL;
        eval(toks, bg);
    }
}

// you don't need to touch this unless you are submitting the bonus task
void prompt() {
    printf("crash> ");
    fflush(stdout);
}

// you don't need to touch this unless you are submitting the bonus task
int repl() {
    char *line = NULL;
    size_t len = 0;
    while (prompt(), getline(&line, &len, stdin) != -1) {
        parse_and_eval(line);
    }

    if (line != NULL) free(line);
    if (ferror(stdin)) {
        perror("ERROR");
        return 1;
    }
    return 0;
}

// you don't need to touch this unless you want to add debugging options
int main(int argc, char **argv) {
    install_signal_handlers();
    return repl();
}