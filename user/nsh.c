#include "kernel/fcntl.h"
#include "kernel/param.h"
#include "kernel/types.h"
#include "user/user.h"

#include <stdbool.h>


#ifndef NULL
#define NULL 0
#endif

#define MAXLINELEN 128


typedef struct command {
    /// @brief path to the file to be read from
    char* i_file;
    /// @brief path to the file to be written to
    char* o_file;
    char* args[MAXARG];
} command;


void terminate(const char* s) {
    fprintf(2, "failed: %s\n", s);
    exit(-1);
}


int safe_fork() {
    int pid = fork();
    if(pid == -1)
        terminate("error on fork()");
    return pid;
}


int readline(char* buf, int max) {
    printf("@ ");
    gets(buf, max);
    return strlen(buf);
}


/// @param buf will be modified in place to help tokenization.
/// @param toks out param. An array of tokens.
/// @param max maximum number of tokens.
void tokenize(char* buf, char* toks[], int max) {
    int tok_cnt = 0;
    bool prev_isspace = true;
    for(char* p = buf; *p; ++p) {
        if(*p == ' ' || *p == '\t' || *p == '\v') {
            *p = '\0';
            prev_isspace = true;
            continue;
        }
        if(*p == '\n' || *p == '\r') {
            *p = '\0';
            break;
        }
        if(prev_isspace) {
            if(tok_cnt == max - 1)
                terminate("too many args");
            toks[tok_cnt++] = p;
            prev_isspace = false;
        }
    }
    toks[tok_cnt] = NULL;
}


/// @brief split tokens by |
int split_pipe(char* toks[], struct command cmd_buf[]) {
    int arg_cnt = 0;
    bool has_r = false;
    while(*toks) {
        if(!has_r && strcmp(*toks, "|") == 0) {
            has_r = true;
            (cmd_buf++)->args[arg_cnt] = NULL;
            arg_cnt = 0;
            toks++;
            continue;
        }
        cmd_buf->args[arg_cnt++] = *(toks++);
    }
    cmd_buf->args[arg_cnt] = NULL;
    return has_r ? 2 : 1;
}


/// @param cmd ptr to the command to be checked
void checkout_redirect(struct command* cmd) {
    cmd->i_file = NULL;
    cmd->o_file = NULL;
    for(char** arg = cmd->args; *arg; arg++) {
        if(strcmp(*arg, "<") == 0) {
            *arg = NULL;
            char* file = *(++arg);
            if(!file)
                terminate("syntax error");
            cmd->i_file = file;
        }
        else if(strcmp(*arg, ">") == 0) {
            *arg = NULL;
            char* file = *(++arg);
            if(!file)
                terminate("syntax error");
            cmd->o_file = file;
        }
    }
}


void redirect_file(const struct command* cmd) {
    if(cmd->i_file) {
        close(0);
        if(open(cmd->i_file, O_RDONLY) == -1)
            terminate("error on open()");
    }
    if(cmd->o_file) {
        close(1);
        if(open(cmd->o_file, O_WRONLY | O_CREATE) == -1)
            terminate("error on open()");
    }
}


/// @remark must be called in a sub process.
void exec_single(struct command* cmd) {
    redirect_file(cmd);
    exec(cmd->args[0], cmd->args);
    terminate("error on exec()");
}


/// @remark must be called in a sub process.
void exec_pipe(struct command* left, struct command* right) {
    int fd[2];
    if(pipe(fd) == -1)
        terminate("error on pipe()");
    if(safe_fork() == 0) {
        redirect_file(left);
        close(1);
        dup(fd[1]);
        close(fd[1]);
        close(fd[0]);
        exec(left->args[0], left->args);
        terminate("error on exec()");
    }
    else {
        wait(NULL);
        redirect_file(right);
        close(0);
        dup(fd[0]);
        close(fd[1]);
        close(fd[0]);
        exec(right->args[0], right->args);
        terminate("error on exec()");
    }
}


int main() {
    char buf[MAXLINELEN];
    char* toks[MAXARG];
    struct command cmd_buf[2], *const left = cmd_buf, *const right = cmd_buf + 1;
    while(readline(buf, MAXLINELEN) > 0) {
        if(safe_fork() == 0) {
            tokenize(buf, toks, MAXARG);
            memset(cmd_buf, 0, sizeof(cmd_buf));
            const int cmd_num = split_pipe(toks, cmd_buf);
            if(cmd_num == 2) {
                checkout_redirect(left);
                checkout_redirect(right);
                exec_pipe(left, right);
            }
            else {
                checkout_redirect(left);
                exec_single(left);
            }
            exit(0);
        }
        wait(NULL);
    }
    printf("\n");
    exit(0);
}
