#include "myshell.h"
#define MAXARGS   128

typedef struct JOB{
    int spec;
    int childNum;
    pid_t pid[MAXARGS];
    pid_t pgid;
    int bg;
    int sigchldCount;
    int ing[MAXARGS];
    int status;
    char name[MAXLINE];
    struct JOB* next;
}job;

int argc;            /* Number of args */
int pipeNum; //number of pipe
int pipeLoc[MAXARGS];//pipe's location in argv
char *pipeArg[MAXARGS][MAXLINE] = {};
char *givenCmd[MAXARGS];
sigjmp_buf portal1;
int sigchldCount;
pid_t homePid;
job* homeJob = NULL;
job* rearJob;
job* nowJob;


/* Function prototypes */
void eval(char *cmdline);
int parseline(char *buf, char **argv);
int builtin_command(char **argv); 
void sigchld_handler(int);
void sigint_handler(int);
void sigtstp_handler(int);
void handler2(int sig);
void freeAll(int*, int);// free pipeArg, givenCmd
int cdCmd(char**, char*);
void makeJob(char*);
void printJobs();
int fgbgJob(char*, int);
void killJob(char*);


int main() 
{
    char cmdline[MAXLINE]; /* Command line */
    homePid = getpid();
    homeJob = (job*)malloc(sizeof(job));
    homeJob->spec = 0;
    homeJob->status = -1;
    homeJob->next = NULL;
    strcpy(homeJob->name, "Home\n");
    rearJob = homeJob;
    nowJob = homeJob;
    switch(sigsetjmp(portal1, 1)){
    case 0:
        Signal(SIGINT, sigint_handler);
        Signal(SIGTSTP, sigtstp_handler);
        break;
    case 1:
        rearJob->status = -1;
        if(rearJob->pgid > 0)
            Kill((-1)*rearJob->pgid, SIGKILL);
        break;
    case 2:
        if(rearJob->status != -1)
            rearJob->status = 0;
        if(rearJob->pgid > 0)
            Kill((-1)*rearJob->pgid, SIGTSTP);
        break;
    
    }
    while (1) {
        Sio_puts("> ");        
        if(!(fgets(cmdline, MAXLINE, stdin))){
            unix_error("cmdline buf error\n");
            continue;
        } 
        if (feof(stdin))
            exit(0);
        pipeNum = 0;
        eval(cmdline);
    } 
}

void sigchld_handler(int s)
{
    int olderrno = errno;
    while(waitpid(-1, NULL, 0) > 0){
        (rearJob->sigchldCount)++;
    }
    if(rearJob->sigchldCount == rearJob->childNum)
        rearJob->status = -1;
    Signal(SIGCHLD, SIG_DFL);
    errno = olderrno;
}

void sigint_handler(int sig)
{
    Sio_puts("\n");
    siglongjmp(portal1, 1);
}

void sigtstp_handler(int sig)
{
    Sio_puts("\n");
    Signal(SIGCHLD, SIG_DFL);
    siglongjmp(portal1, 2);
}

void freeAll(int* pipeLoc, int pipeNum)
{
    int lastLoc, i, j;
    for(i = 0; i<pipeNum+1; i++)
        free(givenCmd[i]);
    lastLoc = 0;
    if(pipeNum > 0){
        for(i = 0; i<pipeNum; i++){
            for(j = lastLoc; j<pipeLoc[i]; j++)
                free(pipeArg[i][j-lastLoc]);
            lastLoc = ++j;
        }
        for(j = lastLoc; j < argc; j++)
            free(pipeArg[pipeNum][j-lastLoc]);
    }
}

int cdCmd(char** argv, char* buf)
{
    char charTemp[MAXARGS];
    char bufTemp[MAXLINE];
    strcpy(charTemp, argv[1]);
    if(!getcwd(buf, MAXLINE))
        return -1;
    strcpy(bufTemp, buf);
    strcat(bufTemp,"/");
    strcat(bufTemp, charTemp);
    if (chdir(bufTemp) == 0){
        //printf("path: %s\n", buf);
    }
    else{
        unix_error("cd error");
        return -1;
    }
    return 0;
}

void makeJob(char* cmdline)
{
    job* temp = (job*)malloc(sizeof(job));
    temp->spec = (rearJob->spec) + 1;
    temp->status = 1;
    temp->childNum = pipeNum + 1;
    temp->sigchldCount = 0;
    strcpy(temp->name, cmdline);
    temp->next = NULL;
    rearJob->next = temp;
    rearJob = temp;
    nowJob = rearJob;
}

void printJobs()
{
    for(job* temp = homeJob; temp != NULL; temp = temp->next){
        if(temp->status == 1)
            printf("[%d] pid = %d running %s", temp->spec, temp->pid[0], temp->name);
        else if(temp->status == 0)
            printf("[%d] pid = %d suspended %s", temp->spec, temp->pid[0], temp->name);
    }
}

int fgbgJob(char* targetStr, int fgbg)
{
    int target = atoi(targetStr+1);
    //char bf[MAXLINE];
    for(job* temp = homeJob; temp != NULL; temp = temp->next){
        if(temp->spec == target){
            if(temp->status == 0){
                nowJob = temp;
                if(fgbg == 0)
                    nowJob->bg = 0;
                else
                    nowJob->bg = 1;
                nowJob->sigchldCount = 0;
                nowJob->status = 1;
            }
            else if(temp->status == -1){
                Sio_puts("No Such Job\n");
                return -1;
            }
            else if(temp->status == 1){
                temp->bg = 0;
            }
            break;
        }
        else if(temp->spec > target || temp->next == NULL){
            Sio_puts("No Such Job\n");
            return -1;
        }
    }
    return 0;
}

void killJob(char* targetStr)
{
    int target = atoi(targetStr+1);

    for(job* temp = homeJob; temp != NULL; temp = temp->next){
        if(temp->spec == target){
            temp->status = -1;
            //Kill((-1)*(temp->pid), SIGKILL);
            for(int i = 0; i < temp->childNum; i++){
                //printf("%dth wait\n", i);
                if(temp->ing == 0)
                    Waitpid(temp->pid[i], NULL, 0);
                else
                    Kill(temp->pid[i], SIGKILL);
            }
            break;
        }
        else if(temp->spec > target || temp->next == NULL){
            Sio_puts("No Such Job\n");
            break;
        }
    }
    
}

void eval(char *cmdline)
{
    char *argv[MAXARGS]; /* Argument list execve() */
    char buf[MAXLINE];   /* Holds modified command line */
    int bg;              /* Should the job run in bg or fg? */
    pid_t pid[MAXARGS];           /* Process id */
    int fdTemp[2];
    int pipeFd[2];
    int child_status, i, j, lastLoc;
    for(i = 0; i<MAXARGS; i++){//pipeArg, givenCmd 초기화
        for(j = 0; j<MAXLINE; j++)
            pipeArg[i][j] = NULL;
        givenCmd[i] = NULL;
    }
    strcpy(buf, cmdline);
    bg = parseline(buf, argv); 
    nowJob = rearJob;
    if (argv[0] == NULL)  
        return;   /* Ignore empty lines */
    else if (bg == 2){
        Sio_puts("Quotation is not matched\n");
        return;
    }
    if (strcmp(argv[0], "cd") == 0){//cd parts
        cdCmd(argv, buf);
        return;
    }
    else if (strcmp(argv[0], "jobs") == 0){//jobs parts
        printJobs();
        return;
    }
    else if (strcmp(argv[0], "fg") == 0){
        if((fgbgJob(argv[1], 0)) == -1)
            return;
        strcpy(buf, nowJob->name);
        argv[0] = NULL;
        argv[1] = NULL;
        bg = parseline(buf, argv);
    }
    else if (strcmp(argv[0], "bg") == 0){
        fgbgJob(argv[1], 1);
        strcpy(buf, nowJob->name);
        argv[0] = NULL;
        argv[1] = NULL;
        bg = parseline(buf, argv);
    }
    else if (strcmp(argv[0], "kill") == 0){
        killJob(argv[1]); 
        return;
    }
    else{
        makeJob(cmdline);
        nowJob->bg = bg;
    }
    givenCmd[0] = (char*)malloc(sizeof(char)*MAXLINE);
    givenCmd[0][0] = '\0';
    if(strcmp(argv[0],"sort") == 0)
        strcat(givenCmd[0], "/usr");
    strcat(givenCmd[0], "/bin/");
    strcat(givenCmd[0], argv[0]);
    if(pipeNum == 0){
        if (!builtin_command(argv)) { //quit -> exit(0), & -> ignore, other -> run
            nowJob->ing[0] = 1;
            if(nowJob->bg == 1)
                Signal(SIGCHLD, sigchld_handler);
            if((pid[0] = Fork()) == 0){//child
                Signal(SIGINT, SIG_DFL);
                if (execve(givenCmd[0], argv, environ) < 0) {//ex) /bin/ls ls -al &
                    unix_error("Execve error");
                    nowJob->status = -1;
                    return;
                }
            }//parent
            //Parent waits for foreground job to terminate 
            nowJob->pid[0] = pid[0];
            nowJob->pgid = pid[0];
            if (!nowJob->bg){ 
                Waitpid(pid[0], &child_status, 0);
                nowJob->status = -1;
            }
            else//when there is backgrount process!
                Setpgid(pid[0],pid[0]);
            nowJob->ing[0] = 0;
        }
    }
    else if(pipeNum > 0){//pipe parts
        //command 경로 수정
        for(i = 0; i<pipeNum; i++){
            givenCmd[i+1] = (char*)malloc(sizeof(char)*MAXLINE);
            givenCmd[i+1][0] = '\0';
            if(strcmp(argv[pipeLoc[i]+1], "sort") == 0)
                strcat(givenCmd[i+1], "/usr");
            strcat(givenCmd[i+1], "/bin/");
            strcat(givenCmd[i+1], argv[pipeLoc[i]+1]);
        }
        lastLoc = 0;
        for(i = 0; i<pipeNum; i++){//pipeArg data insult
            for(j = lastLoc; j<pipeLoc[i]; j++){
                pipeArg[i][j-lastLoc] = (char*)malloc(sizeof(char)*100);
                strcpy(pipeArg[i][j-lastLoc], argv[j]);
            }
            pipeArg[i][j++] = NULL;
            lastLoc = j;
        }
        for(j = lastLoc; j < argc; j++){
            pipeArg[pipeNum][j-lastLoc] = (char*)malloc(sizeof(char)*100);
            strcpy(pipeArg[pipeNum][j-lastLoc], argv[j]);
        }
        pipeArg[pipeNum][j] = NULL;
        fdTemp[0] = dup(STDIN_FILENO);
        fdTemp[1] = dup(STDOUT_FILENO);//backup std I/O
        if(nowJob->bg == 1)
            Signal(SIGCHLD, sigchld_handler);
        for(i = 0; i<pipeNum+1; i++){
            if((pipe(pipeFd))<0){
                unix_error("pipe error");
                return;
            }
            nowJob->ing[i] = 1;
            if((pid[i] = Fork()) == 0){//child
                Signal(SIGINT, SIG_DFL);
                if(i != pipeNum){
                    Close(pipeFd[0]);
                    Dup2(pipeFd[1], STDOUT_FILENO);
                    Close(pipeFd[1]);
                }
                if (execve(givenCmd[i], pipeArg[i], environ) < 0) {	//ex) /bin/ls
                    unix_error("Execve error");
                    return;
                }
            }
            else{//parent
                Close(pipeFd[1]);
                if(i != pipeNum)
                    Dup2(pipeFd[0], STDIN_FILENO);
                else
                    Dup2(fdTemp[0], STDIN_FILENO);
                Close(pipeFd[0]);
                nowJob->pid[i] = pid[i];
                if(i == 0)
                    nowJob->pgid = pid[0];

                if(!nowJob->bg)
                    Waitpid(pid[i], &child_status, 0);
                else
                    Setpgid(pid[i],pid[0]);
                nowJob->ing[i] = 0;
            }
        }
        if(!nowJob->bg)
            nowJob->status = -1;
    }
    freeAll(pipeLoc, pipeNum);
    return;
}

/* If first arg is a builtin command, run it and return true */
int builtin_command(char **argv) 
{
    if (!strcmp(argv[0], "exit")){ /* quit command */
        job* run = homeJob;
        job* temp;
        while(1){
            if(run->next == NULL)
                break;
            temp = run;
            run = run->next;
            free(temp);
        }
        free(run);
        exit(0);  
    }
    if (!strcmp(argv[0], "&"))    /* Ignore singleton & */
        return 1;
    return 0;                     /* Not a builtin command */
}

int parseline(char *buf, char **argv) 
{
    char *delim;         /* Points to first space delimiter */
    int bg;              /* Background job? */
    int bigQuote = 0, smallQuote = 0, quote = 0, errCheck = 0;
    char* Qdelim;
    char* Qbuf;
    char bufTemp[MAXLINE];
    for(int i = 0; i<strlen(buf); i++){//insert space left right about pipe
        if(buf[i] == '|' || buf[i] == '&'){
            strncpy(bufTemp, buf, i);
            bufTemp[i] = '\0';
            if(buf[i] == '|')
                strcat(bufTemp, " | ");
            else
                strcat(bufTemp, " &  ");
            strcat(bufTemp, buf+i+1);
            strcpy(buf, bufTemp);
            i++;
        }
    }
    buf[strlen(buf)-1] = ' ';  /* Replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* Ignore leading spaces */
        buf++;
    /* Build the argv list */
    argc = 0;
    while ((delim = strchr(buf, ' '))) {
        if(quote == 1){// if one quote detect, then find another
            argv[argc++] = buf + 1;
            *Qdelim = '\0';
            buf = Qdelim + 1;
            quote = 0;
        }
        else{
            argv[argc++] = buf;
            *delim = '\0';
            buf = delim + 1;
        }
        if(strcmp(argv[argc-1], "|") == 0){ //remember pipe's location in argv
            argv[argc-1] = NULL;
            pipeLoc[pipeNum++] = argc-1;
        }
        while (*buf && (*buf == ' ')) /* Ignore spaces */
            buf++;
        if(*buf == '\"'){//big Quote check
            quote = 1;
            errCheck = 1;
            for(Qdelim = buf + 1; *Qdelim; Qdelim++){
                if(*Qdelim == '\"'){
                    errCheck = 0;
                    break;
                }
            }
        }
        else if(*buf == '\''){//small Quote check
            quote = 1;
            errCheck = 1;
            for(Qdelim = buf + 1; *Qdelim; Qdelim++){
                if(*Qdelim == '\''){
                    errCheck = 0;
                    break;
                }
            }
        }
        if(errCheck == 1)
            return 2;//bg == 2: Quote error
    }
    argv[argc] = NULL;
    if (argc == 0)  /* Ignore blank line */
        return 1;
    if ((bg = (*argv[argc-1] == '&')) != 0)
        argv[--argc] = NULL;
    return bg;
}

void unix_error(char *msg) /* Unix-style error */
{
    fprintf(stderr, "%s: %s\n", msg, strerror(errno));
}

pid_t Fork(void) 
{
    pid_t pid;

    if ((pid = fork()) < 0)
        unix_error("Fork error");
    return pid;
}

void Execve(const char *filename, char *const argv[], char *const envp[]) 
{
    if (execve(filename, argv, envp) < 0)
        unix_error("Execve error");
}

pid_t Wait(int *status) 
{
    pid_t pid;

    if ((pid  = wait(status)) < 0)
        unix_error("Wait error");
    return pid;
}

pid_t Waitpid(pid_t pid, int *iptr, int options) 
{
    pid_t retpid;
    if ((retpid  = waitpid(pid, iptr, options)) < 0) 
        unix_error("Waitpid error");
    return(retpid);
}

void Kill(pid_t pid, int signum) 
{
    int rc;

    if ((rc = kill(pid, signum)) < 0);
}

void Pause() 
{
    (void)pause();
    return;
}

unsigned int Sleep(unsigned int secs) 
{
    return sleep(secs);
}

void Setpgid(pid_t pid, pid_t pgid) {
    int rc;

    if ((rc = setpgid(pid, pgid)) < 0)
        unix_error("Setpgid error");
    return;
}

pid_t Getpgrp(void) {
    return getpgrp();
}

handler_t *Signal(int signum, handler_t *handler) 
{
    struct sigaction action, old_action;

    action.sa_handler = handler;  
    sigemptyset(&action.sa_mask); /* Block sigs of type being handled */
    action.sa_flags = SA_RESTART; /* Restart syscalls if possible */

    if (sigaction(signum, &action, &old_action) < 0)
        unix_error("Signal error");
    return (old_action.sa_handler);
}

void sio_error(char s[])
{
    sio_puts(s);
    _exit(1);
}

static size_t sio_strlen(char s[])
{
    int i = 0;
    while(s[i] != '\0')
        ++i;
    return i;
}

ssize_t sio_puts(char s[]) /* Put string */
{
    return write(STDOUT_FILENO, s, sio_strlen(s)); //line:csapp:siostrlen
}

ssize_t Sio_puts(char s[])
{
    ssize_t n;

    if ((n = sio_puts(s)) < 0)
        sio_error("Sio_puts error");
    return n;
}

void Close(int fd) 
{
    int rc;

    if ((rc = close(fd)) < 0)
        unix_error("Close error");
}

int Dup2(int fd1, int fd2) 
{
    int rc;

    if ((rc = dup2(fd1, fd2)) < 0)
        unix_error("Dup2 error");
    return rc;
}

void Free(void *ptr) 
{
    free(ptr);
}

void Fclose(FILE *fp) 
{
    if (fclose(fp) != 0)
        unix_error("Fclose error");
}

void app_error(char *msg)
{
    fprintf(stderr, "%s\n", msg);
    exit(0);
}

char *Fgets(char *ptr, int n, FILE *stream) 
{
    char *rptr;

    if (((rptr = fgets(ptr, n, stream)) == NULL) && ferror(stream))
        app_error("Fgets error");

    return rptr;
}

