#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"


void find(const char *path, char *target) {
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;
    if((fd = open(path, 0)) < 0) {
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }
    if(fstat(fd, &st) < 0) {
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }
    switch(st.type) {
        case T_FILE:
        if(strcmp(path + strlen(path) - strlen(target), target) == 0) {
            printf("%s\n", path);
        }
        break;
        case T_DIR:
        if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf) {
            printf("find: path too long\n");
            break;
        }
        strcpy(buf, path);
        p = buf + strlen(buf);
        *p++ = '/';
        while(read(fd, &de, sizeof(de)) == sizeof(de)) {
            if(de.inum == 0)
                continue;
            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0;
            if(stat(buf, &st) < 0) {
                printf("find: cannot stat %s\n", buf);
                continue;
            }
            if(st.type == T_DIR && strcmp(de.name, ".") != 0 && strcmp(de.name, "..") != 0) {
                find(buf, target);
            }
            else if(st.type == T_FILE) {
                if(strcmp(de.name, target + 1) == 0) { // target[0] is '/'
                    printf("%s\n", buf);
                }
            }
        }
        close(fd);
    }
}



//函数作用：查找目录下的文件
int main(int argc , const char* argv[]) {
    if (argc < 3) {
        fprintf(2, "Usage: find <path>\n");
        exit(1);
    }
    char target[DIRSIZ + 1];
    target[0] = '/';
    strcpy(target + 1, argv[2]);
    find(argv[1], target);
    exit(0);
}



