#include "kernel/types.h"
#include "user/user.h"


/// @param id Either 0 or 1. The file descriptor to redirect to.
/// @param fd Array of length 2. The file descriptors to be redirected.
void redirect(int id, int* fd) {
    close(id);
    dup(fd[id]);
    close(fd[1]);
    close(fd[0]);
}


void sieve() {
    int prime;
    if(!read(0, &prime, sizeof(prime)))
        return;
    printf("prime %d\n", prime);
    int fd[2];
    pipe(fd);
    if(fork()) {
        redirect(0, fd);
        sieve();
    }
    else {
        redirect(1, fd);
        int number;
        while(read(0, &number, sizeof(number)))
            if(number % prime)
                write(1, &number, sizeof(number));
    }
}


int main(int argc, char** argv) {
    int fd[2];
    pipe(fd);
    if(fork()) {
        redirect(0, fd);
        sieve();
    }
    else {
        redirect(1, fd);
        for(int i = 2; i <= 35; ++i)
            write(1, &i, sizeof(i));
    }
    exit();
}
