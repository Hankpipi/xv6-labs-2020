#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void primes(int p[]) {
    if(fork() == 0) {
        int num;
        if(read(p[0], &num, sizeof(num)) == 0) {
            close(p[0]);
            exit(0);
        }
        int pnext[2];
        int prime = num;
        pipe(pnext);
        printf("prime %d\n", num);
        while(read(p[0], &num, sizeof(num)) == sizeof(num)) {
            if(num % prime == 0)continue;
            write(pnext[1], &num, sizeof(num));
        }            
        close(p[0]);
        close(pnext[1]);
        primes(pnext);
        wait(0);
        exit(0);
    }
}

int main(int argc, char *argv[])
{
    int p[2];
    pipe(p);
    for(int i = 2; i <= 35; ++i) {
        write(p[1], &i, sizeof(i));
    }
    close(p[1]);
    primes(p);
    wait(0);
    exit(0);
}
