#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

void fun(int s){
}


int main(){
    signal(SIGINT, fun);
    while(1) pause();
    return 0;
}