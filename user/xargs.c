#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"


#define NULL 0


/// @brief Search the first char which is not c.
char const* str_nchr(char const* s, char const c) {
    for(; *s && *s == c; ++s);
    if(*s)
        return s;
    return NULL;
}


int main(int argc, char** argv) {
    if(argc < 2) {
        printf("xargs: too few params\n");
        exit();
    }

    char buf[128];
    char* fwd_args[MAXARG];

    while(1) {
        // init buffers
        int fwd_cnt = argc - 1;
        memset(fwd_args, NULL, sizeof(fwd_args));
        memmove(fwd_args, argv + 1, fwd_cnt * sizeof(char*) / sizeof(char));

        gets(buf, 128);
        // if hit ctrl-d
        if(strlen(buf) == 0)
            break;

        for(char *p = buf, prev_isspace = 1; *p; ++p) {
            if(*p == ' ' || *p == '\t') {
                *p = '\0';
                prev_isspace = 1;
                continue;
            }
            if(*p == '\n' || *p == '\r') {
                *p = '\0';
                break;
            }
            if(prev_isspace) {
                fwd_args[fwd_cnt++] = p;
                prev_isspace = 0;
            }
        }

        if(fork() == 0) {
            exec(fwd_args[0], fwd_args);
            exit();
        }
        wait();
    }
    exit();
}
