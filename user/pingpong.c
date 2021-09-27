#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    int a[2],b[2];
    pipe(a),pipe(b);
    char buf[3];
    char data[3] = {'a','b','c'};
    if(fork() == 0) {
        if(read(a[0], buf, 1) == 1) {
            printf("%d: received ping\n", getpid());
        }
        write(b[1], data, 1);
        close(a[0]), close(b[1]);
        exit(0);
    }
    else {
        write(a[1], data, 1);
        wait(0);
        if(read(b[0], buf, 1) == 1) {
            printf("%d: received pong\n", getpid());
        }
        close(a[1]),close(b[0]);
        exit(0);
    }
}
