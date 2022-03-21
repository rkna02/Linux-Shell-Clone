#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>

#define MAXLINE 1024
#define MAXJOBS 1024

void cmd_jobs(const char**);
void insert_jobs(const char**, pid_t);

char **environ;

// TODO: you will need some data structure(s) to keep track of jobs
// Job object consisting job id, pid, and name of command
typedef struct {
    int id;
    pid_t pid;
    char *name;
    //bool present;
} job;

job jobs[1024];  // array of pointers to job objects
job curr;
int job_id = 0;  // job id reference numbers

void handle_sigchld(int sig) {
    // TODO
}

void handle_sigtstp(int sig) {
    // TODO
}

void handle_sigint(int sig) {
    // TODO
}

void handle_sigquit(int sig) {
    // TODO
}

void install_signal_handlers() {
    // TODO
}

void spawn(const char **toks, bool bg) { // bg is true iff command ended with &

    pid_t p1 = fork();  // fork results in two concurrent processes

    if (p1 == -1) {
        fprintf(stderr, "ERROR: cannot run %s\n", toks[0]);
        exit(0);
    }

    if (p1 == 0) {

        int success = execvp(toks[0], toks);
        if (success == -1) {
            fprintf(stderr, "ERROR: cannot run %s\n", toks[0]);
            exit(0);
        } 

    } else {
        job_id = job_id + 1;
        insert_jobs(toks, p1);
        printf("[%i] (%ld)  %s\n", job_id, (long) p1, jobs[job_id - 1].name);
        if (bg) {
            return;
        } else {
            pid_t p2 = waitpid(p1, NULL, 0);
        }
    }
}

void insert_jobs(const char **toks, pid_t pid) {
    curr.id = job_id;
    curr.pid = pid;
    char *copy;
    copy = malloc((strlen(toks[0]) + 1));
    strcpy(copy, toks[0]);
    curr.name = copy;
    jobs[job_id - 1] = curr;  
}

void cmd_jobs(const char **toks) {
    // TODO
    for (int i = 0; i < job_id; i++) {
        printf("[%i] (%ld)  running  %s\n", jobs[i].id, (long) jobs[i].pid, jobs[i].name);
    }
    
}

void cmd_fg(const char **toks) {
    // TODO
}

void cmd_bg(const char **toks) {
    // TODO
}

void cmd_slay(const char **toks) {
    // TODO
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
    if (strcmp(toks[0], "quit") == 0) {
        cmd_quit(toks);
    // TODO: other commands
    } else if (strcmp(toks[0], "jobs") == 0) {
        if (toks[1] != NULL) {
            fprintf(stderr, "ERROR: jobs takes no arguments\n");
        } else {
            cmd_jobs(toks);
        }
    } else {
        spawn(toks, bg);
        //cmd_jobs(toks);
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