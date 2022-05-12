#ifndef __JOB_H_
#define __JOB_H_
#include <iostream>
#include <string>
#include <list>
#include <iterator>
#include "csapp.h"
#include <mutex>
#include <memory>

#define MAXARGS 128
#define JOBCOMMAND "foo"
#define JOBNAME "testjob"



enum State{FG,BG,ST};

typedef struct{
public:
    pid_t pid;
    int jid;
    State state;
    bool isJob;
}job_t;


class JobShell{
public:
    JobShell();
    void eval(char *cmdline);
    State parseline(char *buf,char **argv);
    int builtin_command(char **argv);
    static JobShell* instance();
    bool isJobCommand(char **cmd);
    void waitFg(pid_t pid);
    friend void sigchldHandler(int sig);
    friend void sigintHandler(int sig);
    friend void sigstopHandler(int sig);
    std::shared_ptr<job_t> getJob(pid_t pid);
    std::shared_ptr<job_t> getJidJob(int jid);
    std::shared_ptr<job_t> getFgJob();
    void addJob(std::shared_ptr<job_t> job);
    void removeJob(pid_t pid);
    pid_t jidToPid(int jid);
    std::shared_ptr<job_t> jobInit(char **argv,pid_t p,int j,State s,bool isj);
    void printAllJob();
    void doBg(char **argv);
    void doFg(char **argv);
private:
    std::shared_ptr<job_t> curJob;
    std::list<std::shared_ptr<job_t> > jobs;
};
#define JSh JobShell::instance()
JobShell* JobShell::instance()
{
    static JobShell pinstance;
    return &pinstance;
}

JobShell::JobShell()
{

}

void JobShell::eval(char *cmdline)
{
    char *argv[MAXARGS];
    char buf[MAXLINE];
    State bg_fg;
    pid_t pid;
    bool isjob;

    strcpy(buf,cmdline);
    bg_fg=parseline(buf,argv);
    if(argv[0]==NULL)
    {
        return;
    }
    isjob=isJobCommand(argv);
    if(!builtin_command(argv))
    {
        sigset_t mask_all,mask_one,prev_one;
        sigfillset(&mask_all);
        sigemptyset(&mask_one); sigemptyset(&prev_one);
        sigaddset(&mask_one,SIGCHLD);
        sigprocmask(SIG_BLOCK,&mask_one,&prev_one);

        if((pid=Fork())==0)
        {
            setpgid(0,0);
            sigprocmask(SIG_SETMASK,&prev_one,nullptr);
            if(execve(argv[0],argv,environ)<0)
            {
                printf("%s: Command not found.\n",argv[0]);
                exit(0);
            }
        }
        else
        {
            sigprocmask(SIG_BLOCK,&mask_all,nullptr);
            curJob=jobInit(argv,pid,0,bg_fg,isjob);
            addJob(curJob);
            sigprocmask(SIG_SETMASK,&mask_one,nullptr);
            if(bg_fg==FG)
            {
                
                waitFg(pid);
            }
            else
            {
                sigprocmask(SIG_SETMASK,&mask_all,nullptr);
                printf("%d %s",pid,cmdline);
            }
        }
        sigprocmask(SIG_SETMASK,&prev_one,nullptr);
        
    }
}



State JobShell::parseline(char *buf,char **argv)
{
    char *delim;
    int argc;
    State bg_fg;

    buf[strlen(buf)-1]=' ';
    while(*buf && (*buf==' '))
    {
        buf++;
    }

    argc=0;
    while((delim=strchr(buf,' ')))
    {
        argv[argc++]=buf;
        *delim='\0';
        buf=delim+1;
        while(*buf && (*buf==' '))
        {
            buf++;
        }
    }
    argv[argc]=NULL;

    if(argc==0)
        return BG;
    
    if(*argv[argc-1]=='&')
    {
        bg_fg=BG;
    }
    else
    {
        bg_fg=FG;
    }
    if(bg_fg==BG)
    {
        argv[--argc]=NULL;
    }
    return bg_fg;
}

int JobShell::builtin_command(char **argv)
{
    if(!strcmp(argv[0],"quit"))
    {
        for(auto it:jobs){
            Kill(-it->pid,SIGKILL);
        }
        exit(0);
    }
    else if(!strcmp(argv[0],"&"))
    {
        return 1;
    }
    else if(!strcmp(argv[0],"jobs"))
    {
        printAllJob();
        return 1;
    }
    else if(!strcmp(argv[0],"bg"))
    {
        doBg(argv);
        return 1;
    }
    else if(!strcmp(argv[0],"fg"))
    {
        doFg(argv);
        return 1;
    }
    return 0;
}


bool JobShell::isJobCommand(char **cmd)
{
    char str[MAXLINE]="./";
    if(!strcmp(cmd[0],JOBCOMMAND))
    {
        
        cmd[0]=str;
        strcat(cmd[0],JOBNAME);
        cmd[0][strlen(cmd[0])]='\0';
        return true;
    }
    return false;
}

void JobShell::doBg(char **argv)
{
    int jid;
    pid_t pid;
    std::shared_ptr<job_t> job;
    if(argv[1]==nullptr)
    {
        std::cout<<"Command require Pid or Jid."<<std::endl;
        return;
    }
    bool isJid;
    if(argv[1][0]=='%')
    {
        isJid=true;
    }
    else if(argv[1][0]>='0' && argv[1][0]<='9')
    {
        isJid=false;
    }
    else
    {
        std::cout<<"Command require Pid or Jid."<<std::endl;
        return;
    }
    if(!isJid)
    {
        pid=atoi(argv[1]);
        if((job=getJob(pid))==NULL)
        {
           std::cout<<pid<<":Pid not exist."<<std::endl;
           return; 
        }
    }
    else
    {
        jid=atoi(argv[1]+1);
        if((job=getJidJob(jid))==NULL)
        {
           std::cout<<jid<<":Jid not exist."<<std::endl;
           return; 
        }
    }

    job->state=BG;
    Kill(-(job->pid),SIGCONT);
    std::cout << "Job ["<<job->pid<<"] ("<<job->jid<<") "<<"foo";
}
void JobShell::doFg(char **argv)
{
    int jid;
    pid_t pid;
    std::shared_ptr<job_t> job;
    if(argv[1]==nullptr)
    {
        std::cout<<"Command require Pid or Jid."<<std::endl;
        return;
    }
    bool isJid;
    if(argv[1][0]=='%')
    {
        isJid=true;
    }
    else if(argv[1][0]>='0' && argv[1][0]<='9')
    {
        isJid=false;
    }
    else
    {
        std::cout<<"Command require Pid or Jid."<<std::endl;
        return;
    }
    if(!isJid)
    {
        pid=atoi(argv[1]);
        if((job=getJob(pid))==NULL)
        {
           std::cout<<pid<<":Pid not exist."<<std::endl;
           return; 
        }
    }
    else
    {
        jid=atoi(argv[1]+1);
        if((job=getJidJob(jid))==NULL)
        {
           std::cout<<jid<<":Jid not exist."<<std::endl;
           return; 
        }
    }

    job->state=FG;
    Kill(-(job->pid),SIGCONT);
    waitFg(job->pid);
}

void JobShell::waitFg(pid_t pid)
{
    sigset_t mask_none;
    sigemptyset(&mask_none);
    while(getFgJob()!=nullptr){
        sigsuspend(&mask_none);
    }
}


std::shared_ptr<job_t>JobShell::getJob(pid_t pid)
{
    if(jobs.size()==0)return nullptr;
    for(auto it=jobs.begin();it!=jobs.end();it++){
        if((*it)->pid==pid)
        {
            return (*it);
        }
    }
    return nullptr;
}

std::shared_ptr<job_t>JobShell::getJidJob(int jid)
{
    if(jobs.size()==0)return nullptr;
    for(auto it=jobs.begin();it!=jobs.end();it++){
        if((*it)->jid==jid)
        {
            return (*it);
        }
    }
    return nullptr;
}

std::shared_ptr<job_t>JobShell::getFgJob()
{
    if(jobs.size()==0)return nullptr;
    for(auto it=jobs.begin();it!=jobs.end();it++){
        if((*it)->state==FG)
        {
            return (*it);
        }
    }
    return nullptr;
}

void JobShell::addJob(std::shared_ptr<job_t> job)
{
    if(job==nullptr)return;
    jobs.push_back(job);
}
void JobShell::removeJob(pid_t pid)
{
    if(jobs.size()==0)return;
    for(auto it=jobs.begin();it!=jobs.end();it++){
        if((*it)->pid==pid)
        {
            jobs.erase(it);
            return;
        }
    }
    

}
pid_t JobShell::jidToPid(int jid)
{
    if(jobs.size()==0)return 0;
    for(auto it=jobs.begin();it!=jobs.end();it++){
        if((*it)->pid==jid)
        {
            return (*it)->pid;
        }
    }
    return -1;
}

std::shared_ptr<job_t> JobShell::jobInit(char **argv,pid_t p,int j,State s,bool isj)
{
    if(!isj)return nullptr;
    job_t job;
    char *cmd=argv[1];
    if(cmd==nullptr)
    {
        job.pid=p;
        job.jid=jobs.size()+1;
        job.state=s;
    }
    else
    {
        job.jid=atoi(cmd);
        job.pid=p;
        job.state=s;
    }
    return std::make_shared<job_t>(job);
}

void JobShell::printAllJob()
{
    if(jobs.size()==0)return;
    for(auto it:jobs){
        std::cout<< "foo:"<<it->pid <<"  "<< it->jid <<"  "<< it->state << std::endl;
    }
}

void sigchldHandler(int sig)
{
    int olderrno = errno;
    int status;
    pid_t pid;
    sigset_t mask_all,prev_all;
    sigemptyset(&prev_all);sigfillset(&mask_all);
    while((pid=waitpid(-1,&status,WNOHANG | WUNTRACED))>0)
    {
       sigprocmask(SIG_BLOCK, &mask_all, &prev_all);

       std::shared_ptr<job_t> job=JSh->getJob(pid);
       if(job==nullptr)
       {
           return;
       }
        if(WIFSIGNALED(status) && WTERMSIG(status)==SIGINT )
       {
           JSh->removeJob(pid);
       }
       else if(WIFSTOPPED(status) && WSTOPSIG(status)==SIGSTOP && job->state!=ST)
       {
           job->state=ST;
       }
        if(job->state!=ST)
            JSh->removeJob(pid);
       sigprocmask(SIG_SETMASK,&prev_all,nullptr);
    }
    errno=olderrno;
}

void sigintHandler(int sig)
{
    sigset_t mask_all,prev_all;
    sigemptyset(&prev_all);sigfillset(&mask_all);

    std::shared_ptr<job_t> job = JSh->getFgJob();
    Kill(-(job->pid),sig);
    std::cout << "Job ["<<job->pid<<"] ("<<job->jid<<") terminated by signal 2\n";
    sigprocmask(SIG_SETMASK,&prev_all,nullptr);
}

void sigstopHandler(int sig)
{
    sigset_t mask_all,prev_all;
    sigemptyset(&prev_all);sigfillset(&mask_all);

    std::shared_ptr<job_t> job = JSh->getFgJob();
    job->state=ST;
    kill(-job->pid,sig);
    std::cout << "Job ["<<job->pid<<"] ("<<job->jid<<") stopped by signal 20\n";
    sigprocmask(SIG_SETMASK,&prev_all,nullptr);
}

#endif