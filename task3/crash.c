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
void insert_jobs(const char**, pid_t);

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
} job;

job jobs[1024];  // array of pointers to job objects
job curr;
int job_id = 0;  // job id reference numbers

void handle_sigchld(int sig) {
    pid_t sig_pid;
    sigset_t mask;  // sigset_t represent a signal set
    sigemptyset(&mask);  // all recongnized signals are excluded
    sigaddset(&mask, SIGCHLD);  // adds the SIGCHLD signal into the set to include it
    while ((sig_pid = waitpid(-1, 0, WNOHANG)) != -1) {
        for (int i = 0; i < job_id; i++) {
            if (jobs[i].pid == sig_pid) {
                sigprocmask(SIG_BLOCK, &mask, NULL);
                jobs[i].terminated = true;
                sigprocmask(SIG_UNBLOCK, &mask, NULL);
            }
        }
    }
}

void handle_sigtstp(int sig) {
    // TODO
}

void handle_sigint(int sig) {
    /*
    int length = (int) strlen(jobs[0].id_s) + (int) strlen(jobs[0].pid_s) + (int) strlen(jobs[0].name);
    length = length + 54;
    printf("%ld", (long) jobs[0].pid_s);
    char output[length];
    strcpy(output, "[");
    strcat(output, jobs[0].id_s);
    strcat(output, "] (");
    strcat(output, jobs[0].pid_s);
    strcat(output, ")  killed  ");
    strcat(output, jobs[0].name);
    strcat(output, "\n");
    

    write(STDOUT_FILENO, output, (size_t) strlen(output));
    */
    write(1, "(", 2);
    write(1, jobs[0].id_s, 5);
    write(1, ")", 2);
    write(1, jobs[0].pid_s, 10);
    //printf("[%i] (%ld)  killed  %s\n", job_id, (long) jobs[job_id - 1].pid, jobs[job_id - 1].name);
}

void handle_sigquit(int sig) {
    // TODO
}

void install_signal_handlers() {
    // install SIGCHLD
    struct sigaction chld;
    chld.sa_handler = handle_sigchld;
    chld.sa_flags = SA_RESTART;
    sigfillset(&chld.sa_mask);
    sigaction(SIGCHLD, &chld, NULL);

    // install SIGINT
    struct sigaction intt;
    intt.sa_handler = handle_sigint;
    intt.sa_flags = SA_RESTART;
    sigfillset(&intt.sa_mask);
    sigaction(SIGINT, &intt, NULL);
}

void spawn(const char **toks, bool bg) { // bg is true iff command ended with &
    sigset_t mask;  // sigset_t represent a signal set
    sigfillset(&mask);  // all recongnized signals are excluded
    sigprocmask(SIG_BLOCK, &mask, NULL);
    pid_t p1 = fork();  // fork results in two concurrent processes

    if (p1 == -1) {
        sigprocmask(SIG_UNBLOCK, &mask, NULL);
        fprintf(stderr, "ERROR: cannot run %s\n", toks[0]);
        exit(0);
    }

    if (p1 == 0) {
        sigprocmask(SIG_UNBLOCK, &mask, NULL);
        int success = execvp(toks[0], toks);
        if (success == -1) {
            fprintf(stderr, "ERROR: cannot run %s\n", toks[0]);
            exit(0);
        } 
    } else {
        job_id = job_id + 1;
        insert_jobs(toks, p1);
        printf("[%i] (%ld)  %s\n", job_id, (long) p1, jobs[job_id - 1].name);
        sigprocmask(SIG_UNBLOCK, &mask, NULL);
        if (bg) {
            return;
        } else {
            while (jobs[0].terminated == false) {
                sleep(1);
            }
        }
    }
}

void insert_jobs(const char **toks, pid_t pid) {

    // job id
    curr.id = job_id;

    // job id (string version)
    char id_copy[5];
    sprintf(id_copy, "%d", job_id);
    curr.id_s = id_copy;

    // pid 
    curr.pid = pid;

    // pid (string version)
    int pid_length = 0;
    long pid_temp = (long) pid;
    while(pid_temp!=0)  {  
        pid_temp = pid_temp/10;  
        pid_length++;  
    }  
    pid_length++;  // account for ending string char
    char pid_copy[pid_length];
    sprintf(pid_copy, "%ld", (long) pid);
    curr.pid_s = pid_copy;

    printf("%s", curr.pid_s);

    // name 
    char *name_copy;
    name_copy = malloc((strlen(toks[0]) + 1));
    strcpy(name_copy, toks[0]);
    curr.name = name_copy;

    jobs[job_id - 1] = curr;  
}

void cmd_jobs(const char **toks) {
    for (int i = 0; i < job_id; i++) {
        if (jobs[i].terminated == false) {
            printf("[%d] (%ld)  running  %s\n", jobs[i].id, (long) jobs[i].pid, jobs[i].name);
        }
    }
}

void cmd_fg(const char **toks) {
    // TODO
}

void cmd_bg(const char **toks) {
    // TODO
}

void cmd_slay(const char **toks) {

    if (toks[1][0] == (char)'%') {
        //if (remaining != (toks[1] + (strlen(toks[1])) - 1)) {
        //    fprintf(stderr, "ERROR: bad argument for slay: %s\n", toks[1]);
        //} else {
            for (int i = 0; i < job_id; i++) {
                if (strcmp(jobs[i].id_s, toks[1] + 1) == 0) {
                    jobs[i].terminated = true;
                    kill(jobs[i].pid, SIGKILL);
                    printf("[%d] (%ld)  killed  %s\n", jobs[i].id, (long) jobs[i].pid, jobs[i].name);
                    return;
                }
            }
            fprintf(stderr, "ERROR: no job %s\n", toks[1]);
        //}
    } else {
        //if(strlen(strtol(toks[1], &remaining, 10)) != strlen(toks[1])) {
        //    fprintf(stderr, "ERROR: bad argument for slay: %s\n", toks[1]);
        //} else {
            for (int i = 0; i < job_id; i++) {
                if (strcmp(jobs[i].pid_s, toks[1]) == 0) {
                    jobs[i].terminated = true;
                    kill(jobs[i].pid, SIGKILL);
                    printf("[%d] (%ld)  killed  %s\n", jobs[i].id, (long) jobs[i].pid, jobs[i].name);
                    return;
                }
            }
            fprintf(stderr, "ERROR: no PID %ld\n", (long) toks[1]);
        //}
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
        if (toks[1] == NULL || toks[3] != NULL) {
            fprintf(stderr, "ERROR: slay takes exactly one argument\n");
        } else {
            cmd_slay(toks);
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