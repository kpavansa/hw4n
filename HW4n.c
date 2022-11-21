#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/wait.h>
#include<stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h> 

//jobs-structure 
typedef struct jobStructure
{
    char *job_cmd;
    char *job_stat;   
    char *st_time; 
    char *end_time;  
    char fout[33]; 
    char ferr[33];
    int jid; 
    int ext_stat;       
    pthread_t threadId;
} jobStructure;

//Queues-structure
typedef struct queue
{
    int size;    
    jobStructure **jobBuff; 
    int start;  
    int end;    
    int count;  
} queue;


//Global-Variables
int jobsAfterJobs;      
int jobExecuting;    
jobStructure j_Arr[999]; 
queue *jqueue;


char *dupInputString(char *strng, int i)
{
    int chI;
    char *duplc = malloc(sizeof(char) * strlen(strng));

    for(i;(chI = strng[++i]) != '\0';)
    {
        duplc[i] = chI;
    }
    duplc[i] = '\0';
    return duplc;
}

jobStructure jobGen(char *job_cmd, int jid)
{
    jobStructure gJob;
    gJob.jid = jid;
    gJob.ext_stat = -1;
    gJob.job_cmd = dupInputString(job_cmd, -1);
    gJob.st_time = gJob.end_time = NULL;
    gJob.job_stat = "waiting";
    sprintf(gJob.fout, "%d.out", gJob.jid);
    sprintf(gJob.ferr, "%d.err", gJob.jid);
    return gJob;
}



//outputs-formatting
void jobsShow(jobStructure *job, int ze, char *chr, char *command, char c)
{
   
    if (job != NULL)
    {
    if (ze != 0 && (strcmp(command, "showjobs") ==0))
        {
            printf("jobid\tcommand\t\t\tstatus\n");
            int i=0,flag=0;
            while( i < ze) 
            {
                flag=strcmp(job[i].job_stat, "success") != 0?1:0;
                
                if(flag)
                printf("%d\t%s\t%s\n",job[i].jid+1,job[i].job_cmd,job[i].job_stat);

                i=i+1;
            }
        }
    else if( ze != 0 && (strcmp(command, "submithistory") == 0) )//showing submithistory-command
        {
            printf("jobid\tcommand\t\t\tstarttime\t\t\tendtime\t\t\t\tstatus\n");
            int i = 0,flag=0;
            while(i < ze)
            {
                flag=strcmp(job[i].job_stat, "success") == 0?1:0;

                if(flag)
                printf("%d\t%s\t%s\t%s\t%s\n",job[i].jid+1,job[i].job_cmd,job[i].st_time,job[i].end_time,job[i].job_stat);

                i++;
            }
        }
        }
}


//queue-initialization
queue *queue_init(int ze)
{
    queue *q = malloc(sizeof(queue));
    q->jobBuff = malloc(ze*sizeof(jobStructure *));
    q->size = ze;
    q->end = 0;
    q->start = 0;
    q->count = 0;
    return q;
}

//queue-insertion based on processes
int q_ins(queue *q, jobStructure *j_pointer)
{
    if (q == NULL)  
        return -1;
    
    else if(q->count == q->size)
        return -1;
    
    int qmod=q->end % q->size;
    int qmodend=(q->end + 1) % q->size;
    q->jobBuff[qmod] = j_pointer;
    q->end = qmodend;
    q->count=q->count+1;
    return q->count;
}

//process-removal after completion
//update-output into files
jobStructure *q_delete(queue *q)
{ 
    if (q == NULL)  
        return (jobStructure *)-1;
    
    else if(q->count == 0)
        return (jobStructure *)-1;
    int qmod=(q->start + 1) % q->size;
    jobStructure *gJob = q->jobBuff[q->start];
    q->start = qmod;
    q->count=q->count-1;
    return gJob;
}


//taking ip from the provided-args
int gettingTheInput( int ze, char *strng, int w, int f)
{
    int i, chI,flag=0;
    for (i = 0; i < ze - 1 && (chI = getchar()) != '\n'; ++i)
    {
        flag=(chI == EOF)?1:0;
        if(flag)
            return -1;
        
         strng[i] = chI;
    }
    strng[i] = '\0';
    return i;
}

//ip comparison for adjustment
bool spaceorNullChar(char chI)
{
    if(chI == ' ' ||chI == '\t' ||  chI == '\n' ||chI == '\r' ) 
        return true;

    else if(chI == '\x0b' ||chI == '\x0c')
        return true;

    else return false;
}

//get the ip for job-queue and update it
char *charPFirst(char *strng, int i)
{
    char *charP;
        for(i=0; spaceorNullChar(strng[i])==true;i++)
        {
            continue;
        }
    charP = strng+ i;
    return charP;
}


//measurement of time-date whenever applicable
char *timeAndDateStr(char *nsc)
{
    time_t timer = time(NULL);
    int  chI,i;   
    char *strng=ctime(&timer);
    char *duplc = malloc(sizeof(char) * strlen(strng));
    for( i=-1 ;(chI = strng[++i]) != '\0' && chI != '\n'; )
    {
        duplc[i] = chI;
    }
    duplc[i] = '\0';
    return duplc;

}

//Input-tokenization
char **argus(char *inputLine)
{
    char *duplc = malloc(sizeof(char) * (strlen(inputLine) + 1));
    strcpy(duplc, inputLine);

    char *arg;
    int i = 0;
    char **args = malloc(sizeof(char *));
    while ((arg = strtok(duplc, " \t")) != NULL)
    {
        args[i] = malloc(sizeof(char) * (strlen(arg) + 1));
        strcpy(args[i], arg);
        args = realloc(args, sizeof(char *) * (++i + 1));
        duplc = NULL;
    }
    args[i] = NULL;
    return args;
}

//Logging the file
int updLog(char *filename, int file_descriptor)
{
    file_descriptor= open(filename, O_CREAT | O_APPEND | O_WRONLY, 0755);
    if (file_descriptor == -1)
    {
        fprintf(stderr, "File Opening failed. Verify permissions \"%s\"\n", filename);
        perror("open");
        exit(1);
    }
    return file_descriptor;
}

//Accepting-programs one after another
void *procsdone(void *arg)
{
    pid_t pid;   
    char **args; 
     jobStructure *j_pointer;

    j_pointer = (jobStructure *)arg;

    jobExecuting++;
    j_pointer->job_stat = "working";
    j_pointer->st_time = timeAndDateStr(NULL);

    pid = fork();
    if (pid == 0) //Child-Process
    {
        dup2(updLog(j_pointer->fout,0), 1); // Duplicating the file-discriptors
        dup2(updLog(j_pointer->ferr,0), 2); 
        args = argus(j_pointer->job_cmd);
        execvp(args[0], args); //commands-execution
        fprintf(stderr, "command-execution failed: \"%s\"\n", args[0]);
        perror("execvp");
        exit(1);
    }
    else if (pid > 0) //Parent-process
    {
        waitpid(pid, &j_pointer->ext_stat, WUNTRACED); //Wait until child process-completion
        j_pointer->job_stat = "success";
        j_pointer->end_time = timeAndDateStr(NULL);

        if (!WIFEXITED(j_pointer->ext_stat)) //child-process-status
            fprintf(stderr, "Child process %d haven't terminated normally!\n", pid);
    }
    else
    {
        fprintf(stderr, "Error: Forking-process is failed\n");
        perror("fork");
        exit(1);
    }

    jobExecuting--;
    return NULL;
}

//Threading-the-process
void *procsGreen(void *args)
{
    jobStructure *j_pointer;
    jobExecuting = 0;
    while(true)
    {
        if (jqueue->count > 0)
        {
            if(jobsAfterJobs>jobExecuting)
            {
                j_pointer = q_delete(jqueue);
                pthread_create(&j_pointer->threadId, NULL, procsdone, j_pointer);
                pthread_detach(j_pointer->threadId);
            }
        }
        sleep(1); 
    }
    return NULL;
}



int main(int argc, char **argv)
{
    printf("For quitting the program Hit 'CNTL+%c'\n",92);
    int i = 0;              
    char inputLine[1000]; 
    char *kw, *c=NULL;           
    char *job_cmd;
    jobStructure *j_pointer;

    char *ferr;  
    pthread_t threadId; 

    if (argc != 2)
    {
        printf("Usage: %s No of parallel processes\n", argv[0]);
        exit(0);
    }


    jobsAfterJobs = atoi(argv[1]);

    jobsAfterJobs < 1?jobsAfterJobs = 1:jobsAfterJobs;

    jobsAfterJobs > 8?jobsAfterJobs = 8:jobsAfterJobs;


    ferr = malloc(sizeof(char) * (strlen(argv[0]) + 7));
    sprintf(ferr, "%s.err", argv[0]);
    dup2(updLog(ferr,0), 2);

    jqueue = queue_init(100);

    pthread_create(&threadId, NULL, procsGreen, NULL);

    while (printf("Enter command> ") && gettingTheInput(1000, inputLine, 0, 0) != -1)
    {
        if ((kw = strtok(dupInputString(inputLine, -1), " \t\n\r\x0b\x0c")) != NULL) //input comparison with null and space chars
        {
            if (strcmp(kw, "submit") == 0) //submit-program
            {
                if (i >= 1000)
                {
                    printf("Max number of job submissions done. Re-run to operate\n");
                }
                else if (jqueue->count >= jqueue->size)
                {
                    printf("Max-Queues submissions done. Wait for the queues to finish\n");
                }
                else
                {
                    job_cmd = charPFirst((strstr(inputLine, "submit") + 6), 0);
                    j_Arr[i] = jobGen(job_cmd, i);
                    q_ins(jqueue, j_Arr + i);
                    printf("job %d added to the queue\n", (i++)+1);
                }
            }
            else if (strcmp(kw, "showjobs") == 0 || strcmp(kw, "submithistory") == 0)
            {
                jobsShow(j_Arr, i, c, kw,'c');
            }
        }
    }
    kill(0, 2); 

    exit(0);
}