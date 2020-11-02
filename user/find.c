#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "kernel/fs.h"
#include "user/user.h"


/// @see fmtname() in user/ls.c
char const* get_filename(char const* path) {
    char const* p;
    for(p = path + strlen(path); p >= path && *p != '/'; --p);
    ++p;
    return p;
}


void find(char const* path, char const* file) {
    char buf[512], *p;
    struct stat st;
    struct dirent de;

    const int fd = open(path, O_RDONLY);
    if(fd < 0) {
        printf("find: cannot open %s\n", path);
        return;
    }

    if(fstat(fd, &st) < 0) {
        printf("find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    // the type of the current path
    switch(st.type) {
        case T_DIR:
            if(strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf)) {
                printf("find: path too long\n");
                break;
            }
            strcpy(buf, path);
            p = buf + strlen(buf);
            *p = '/';
            ++p;
            while(read(fd, &de, sizeof(de)) == sizeof(de)) {
                if(de.inum == 0)
                    continue;
                // prevent recurse into "." and ".."
                if(strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
                    continue;
                memmove(p, de.name, DIRSIZ);
                p[DIRSIZ] = '\0';
                if(stat(buf, &st) < 0) {
                    printf("find: cannot stat %s\n", buf);
                    continue;
                }
                find(buf, file);
            }
            break;

        case T_FILE:
            if(strcmp(get_filename(path), file) == 0)
                printf("%s\n", path);
            break;
    }

    close(fd);
}


int main(int argc, char** argv) {
    if(argc < 3) {
        printf("find: too few params\n");
        exit();
    }
    find(argv[1], argv[2]);
    exit();
}
