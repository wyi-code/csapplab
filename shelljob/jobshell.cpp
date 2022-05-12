
#include "csapp.h"
#include "job.h"
#include <iostream>

#define MAXARGS 128


int main(int argc, char **argv, char**env)
{

    Signal(SIGCHLD,sigchldHandler);
    Signal(SIGINT,sigintHandler);
    Signal(SIGTSTP,sigstopHandler);
    char cmdline[MAXLINE];
    while(1)
    {
        std::cout<<">";
        fgets(cmdline,MAXLINE,stdin);
        if(feof(stdin))
            exit(0);

        JSh->eval(cmdline);
        fflush(stdout);
        fflush(stdout);
    }
    return 0;
}
