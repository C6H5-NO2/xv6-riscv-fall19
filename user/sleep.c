#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char** argv) {
    if(argc < 2) {
        printf("sleep: too few args\n");
        exit();
    }
    if(argc > 2) {
        printf("sleep: too many args\n");
        exit();
    }
    sleep(atoi(argv[1]));
    exit();
}
