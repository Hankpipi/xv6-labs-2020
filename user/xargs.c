#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/param.h"

const int MAXBYTES = 1024;

int main(int argc, char *argv[])
{
    char ch;
    int i, j = 0;
    char* cmd[MAXARG];
    for(i = 1; i < argc; ++i) cmd[i - 1] = argv[i];
    i = argc - 1;
    cmd[i] = malloc(MAXBYTES);
    while(read(0, &ch, 1) == 1) {
        if(ch == ' ') {
            j = 0;
            cmd[++i] = malloc(MAXBYTES);
        }
        else if(ch == '\n') {
            if(fork() == 0) {
                exec(cmd[0], cmd);
            }
            for(int k = argc; k <= i; ++k) free(cmd[k]);
            wait(0);
            i = argc - 1, j = 0;
            cmd[i] = malloc(MAXBYTES);            
        }
        else {
            cmd[i][j++] = ch;
        }
    }
    exit(0);
}
