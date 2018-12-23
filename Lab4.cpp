#define MAX_IO_COUNT 11000
#define HEAD_MOVEMENT_TIME 1
//********************************************************************************************
#include <fstream>
#include <string>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
using namespace std;
//********************************************************************************************
bool debugFlag = false;
//********************************************************************************************
int totalIOCount = 0;
int currentTime = 0;
int lastHeadPosition = 0;
int totalHeadMovements = 0;
//********************************************************************************************

struct IODT{int track;int arrivalTime;int startTime;int endTime;};

//********************************************************************************************

IODT IOList[MAX_IO_COUNT];

//********************************************************************************************

class IOScheduler{
    public:
    int counter;
    IOScheduler(){ counter = 0; }

    void addJobsToQueue(){
        while(counter<totalIOCount && currentTime>=IOList[counter].arrivalTime){ counter++; }
    }
    int getClosestJob(){
        int job = -1;
        for(int i=0;i<counter;i++){
            if(IOList[i].endTime>=0) continue;
            if(job<0) job = i;
            else if( abs(IOList[i].track-lastHeadPosition) < abs(IOList[job].track - lastHeadPosition) ) job = i;
        }
        return job;
    }
    virtual bool getNextJob(int &job) = 0;
};
class FIFOScheduler : public IOScheduler {
    public:
    bool getNextJob(int &job){
        if(counter>=totalIOCount)
            return false;
        job = counter++;
        return true;
    }
};
class SSTFScheduler : public IOScheduler {
    public:
    bool getNextJob(int &job){
        addJobsToQueue();
        job = getClosestJob();
        
        if(job>=0) return true;
        
        job = counter++;
        return (job<totalIOCount);
    }
};
class LookScheduler : public IOScheduler {
    unsigned int direction:1;
    bool isInTheDirectionOfMovement(int i){
        if(direction==0)
            return IOList[i].track<=lastHeadPosition;
        return IOList[i].track>=lastHeadPosition;
    }
    public:
    LookScheduler(){direction = 1;}

    void changeHeadDirection(int dir = -1){ 
        if(dir<0) direction = ~ direction;
        else if(dir) direction = 1;
        else direction = 0; 
    }
    void getClosestJobInTheDirectionOfMovement(int &jobInMovement,int & jobInQueue){
        int job = -1;
        int closestJob = -1;
        for(int i=0;i<counter;i++){
            if(IOList[i].endTime<0 && isInTheDirectionOfMovement(i))
                job = (job<0)?i: ( (abs(IOList[i].track - lastHeadPosition ) < abs(IOList[job].track - lastHeadPosition))? i : job ) ;
            if(IOList[i].endTime<0)
                closestJob = (closestJob<0)? i : ( (abs(IOList[i].track - lastHeadPosition ) < abs(IOList[closestJob].track - lastHeadPosition))? i : closestJob ) ;
        }
        jobInMovement = job;
        jobInQueue = closestJob;
    }
    bool getNextJob(int &job){
        addJobsToQueue();
        int closestJob = -1;
        getClosestJobInTheDirectionOfMovement(job,closestJob);

        if(debugFlag) printf("In scheduler: jobInDirection:%d, closestJob:%d\n",job,closestJob);
        
        if(job<0){
            job = closestJob;
            if(job>=0) direction = ~ direction;
            else if(counter>=totalIOCount)  return false;
            else{
                job = counter++ ;
                direction = (lastHeadPosition>=IOList[job].track) ? 0 : 1;
            }
        }                
        return true;
    }
};
class CLookScheduler : public LookScheduler {
	bool getNextJob(int &job){	
		addJobsToQueue();
        job = -1;
        int farthestJob = -1;
        for(int i=0;i<counter;i++){
            if(IOList[i].endTime<0 && IOList[i].track >= lastHeadPosition)
                job = (job<0)?i: ( (abs(IOList[i].track - lastHeadPosition ) < abs(IOList[job].track - lastHeadPosition))? i : job ) ;
            if(IOList[i].endTime<0)
                farthestJob = (farthestJob<0)? i : ( (IOList[i].track < IOList[farthestJob].track ? i : farthestJob ) ) ;
        }
        
        if(debugFlag) printf("In scheduler: jobInDirection:%d, closestJob:%d\n",job,farthestJob);
        
        if(job<0){
            job = farthestJob;
            if(job>=0) return true;
            else if(counter>=totalIOCount) return false;
            else job = counter++ ;
        }                
        return true;
    }
};
class FLookScheduler : public LookScheduler {
    public:
    bool getNextJob(int &job){
        job = -1;
        int closestJob = -1;
        getClosestJobInTheDirectionOfMovement(job,closestJob);
        
        if(debugFlag) printf("In scheduler: jobInDirection:%d, closestJob:%d\n",job,closestJob);
        
        if(job<0){
            job = closestJob;
            if(job>=0) changeHeadDirection();
            else if(counter>=totalIOCount) return false;
            else{ 
                if(IOList[counter].arrivalTime<=currentTime){
                    addJobsToQueue();
                    return getNextJob(job);
                }else{
                    if(IOList[counter].track > lastHeadPosition) changeHeadDirection(1);
                    else if(IOList[counter].track < lastHeadPosition) changeHeadDirection(0);
                    job = counter;
                    counter ++;
                }
            }
        }                
        return true;   
    }
};

//********************************************************************************************

ifstream fileInput;
IOScheduler* sch;

//********************************************************************************************

bool getNextLineFromInput(string &str){
    while(getline(fileInput,str)){
        bool isComment = false;
        for(unsigned int i=0;i<str.length();i++){
            if(str[i]=='#'){isComment = true;break;}
        }
        if(isComment) continue;
        return true;
    }
    return false;
}
void readInput(){
    string ioRequest;
     while(getNextLineFromInput(ioRequest)){
        int timestamp;
        int track;
        sscanf(ioRequest.c_str(), "%d %d",&timestamp,&track);
        IOList[totalIOCount].arrivalTime = timestamp;
        IOList[totalIOCount].startTime = -1;
        IOList[totalIOCount].endTime = -1;
        IOList[totalIOCount++].track = track;
     }
}
void simulation(){
    int job;
    while(sch->getNextJob(job)){
        
        if(debugFlag){
            printf("Job:%d, Time:%d, Track:%d, headPosition:%d, IOs pending: [ ",job,currentTime,IOList[job].track,lastHeadPosition);
            for(int i=0;i<totalIOCount;i++){
                if(IOList[i].arrivalTime > currentTime) break;
                if(IOList[i].endTime < 0 ) printf("%d ",i);
            }
            printf("]\n");
        }

        if(IOList[job].arrivalTime>currentTime)
            currentTime = IOList[job].arrivalTime;
        
        IOList[job].startTime = currentTime;
        currentTime += abs(IOList[job].track - lastHeadPosition)*HEAD_MOVEMENT_TIME;
        totalHeadMovements += abs(IOList[job].track - lastHeadPosition);
        IOList[job].endTime = currentTime;
        lastHeadPosition = IOList[job].track;    
    }
}

//********************************************************************************************

void printIOStats(){
    for(int i=0;i<totalIOCount;i++)
        printf("%5d: %5d %5d %5d\n", i, IOList[i].arrivalTime , IOList[i].startTime , IOList[i].endTime );
}
void printSummary(){
    double totalWait = 0, totalTurnaround = 0;
    int maxWait = -1, currentWait = 0;

    for(int i=0;i<totalIOCount;i++){
        currentWait = IOList[i].startTime - IOList[i].arrivalTime; 
        totalWait += currentWait;
        totalTurnaround += IOList[i].endTime - IOList[i].arrivalTime;
        maxWait = maxWait<currentWait?currentWait:maxWait;
    }
    printf("SUM: %d %d %.2lf %.2lf %d\n", currentTime , totalHeadMovements , totalTurnaround/totalIOCount, totalWait/totalIOCount , maxWait);
}

//********************************************************************************************

int main(int argc, char** argv){
    int c;
    char schedulerType = 'x';
    bool schedulerSpecified = false;
    while ( (c = getopt (argc, argv, ":s:d")) != -1){
        switch(c){
            case 's' : schedulerType = optarg[0]; schedulerSpecified = true;  break;
            case 'd' : debugFlag = true; break;
            case ':' : if(optopt=='s') fprintf(stderr,"Provide the algorithm type after -s\n"); exit(99);
            default: fprintf(stderr,"Unknown Option: %c\n",c); exit(99); 
        }
    }
    if(!schedulerSpecified){ fprintf(stderr,"Specify a scheduling algorithm with -s\n"); exit(99); }
    switch(schedulerType){
        case 'i' : sch = new FIFOScheduler(); break;
        case 'j' : sch = new SSTFScheduler(); break;
        case 's' : sch = new LookScheduler(); break;
        case 'c' : sch = new CLookScheduler(); break;
        case 'f' : sch = new FLookScheduler(); break;
        default: fprintf(stderr,"Unsupported IO scheduling Algorithm: <%c>\n",schedulerType); exit(99);
    }
    if(argc-optind<1){ fprintf(stderr,"Input file containing IO info is required.\n"); exit(99);}
    fileInput.open(argv[optind]);
    readInput();
    simulation();
    printIOStats();
    printSummary();
}



