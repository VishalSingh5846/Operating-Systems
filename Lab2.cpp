#define NUMBER_OF_PRIORITY_LEVELS 4
#define TIME_QUANTUM_FOR_NON_PREEMPTIVE_SCHEDULERS 1000000
#include <iostream>
#include <fstream>
#include <string>
#include <string.h>
#include <unistd.h>
#include <vector>
#include <stdlib.h>
#include <list>
#include <queue> 
#include <sstream>
using namespace std;

//********************************************************************************************
//********************************************************************************************

typedef enum {  STATE_DONE , STATE_PREEMPTED , STATE_READY , STATE_RUNNING , STATE_BLOCKED } processStateDT;
typedef enum { TRANSITION_TO_DONE , TRANSITION_TO_READY , TRANSITION_TO_RUN , TRANSITION_TO_BLOCKED , TRANSITION_TO_PREEMPT } eventDT;

struct processStatisticsDT{int finishingTime;int turnaroundTime;int ioTime;int waitTime;processStatisticsDT(){
    ioTime =0; waitTime=0;finishingTime=-1;turnaroundTime=-1;}};

struct processNode{
    int timeSinceLastStateChange; //
    int CPUTimeUsed; //
    int processID; //
    int arrivalTime; //
    int totalCPUTime; //
    int CB;int currentCB; // 
    int currentIO;int IO; //
    processStateDT processState; //
    int staticPriority;int currentPriority; // 
    processStatisticsDT processStatistics;
    processNode(){CPUTimeUsed = 0;currentCB = 0;currentIO = 0;};
};

struct eventNode{int timestamp;processNode* process; eventDT eventType; int eventNumber;};

//********************************************************************************************
//********************************************************************************************

vector<int> randomValueVector;
int randomValueCounter=0;
int currentTime=0;
int timeQuantum=TIME_QUANTUM_FOR_NON_PREEMPTIVE_SCHEDULERS;
int noIOTime = 0;
int lastIOTime = 0;
bool VERBOSE = false;
processNode* lastProcessToFinish = NULL;
vector<processNode*> processList;

//********************************************************************************************
//********************************************************************************************

void readRandomNumbersFromFile(ifstream &input){
    string num;
    getline(input,num);
    int numEl = atoi(num.c_str());
    randomValueVector.resize(numEl);
    for(int i=0;i<numEl;i++){
        getline(input,num);
        randomValueVector[i] = atoi(num.c_str());
    }
    if(randomValueVector.size()==0){
        cout<<"Please provide a valid random value file.\n";
        exit(0);
    }
}

int getRandomNumber(int burst){
    int ans  = 1 + (randomValueVector[randomValueCounter] % burst);
    randomValueCounter=(randomValueCounter+1)%randomValueVector.size();
    return ans;   
}

bool CompareEvent(eventNode* e1,eventNode* e2){
        if(e1->timestamp!=e2->timestamp) return e1->timestamp>e2->timestamp;
        return e1->eventNumber>e2->eventNumber;
}

class Event{
    list<eventNode*> eventList;
    list<eventNode*>::iterator it;
    int eventNumber;
    public:
    Event(){ eventNumber = 0;}

    eventNode* getNextEvent(){
        if(eventList.empty())
            return NULL;
        eventNode* ans = *eventList.begin();
        eventList.erase(eventList.begin());
        return ans;
    }
    
    eventNode* seeNextEvent(){
        if(eventList.empty())
            return NULL;
        eventNode* ans = *eventList.begin();
        return ans;
    }
    
    bool anyEventPendingForProcessOnTimstamp(int processID,int timestamp){
        for(it=eventList.begin();it!=eventList.end();it++){
            eventNode* temp = *it;
            if(temp->timestamp<timestamp)
                continue;
            if(temp->timestamp>timestamp)
                break;
            if(temp->process->processID==processID){
                return true;
            }
        }
        return false;
    }
    
    void removeEvent(processNode* process){
        for(it=eventList.begin();it!=eventList.end();it++){
            eventNode* temp = *it;
            if(temp->process->processID==process->processID){
                if(VERBOSE){
                    cout<<"        REMOVED EVENT:";printEvent(temp);
                }
                eventList.erase(it);
                it--;
                
            }
        }
    }
    
    void addEvent(int timestamp,processNode* process,eventDT eventType){
        eventNode* temp = new eventNode;
        temp->timestamp = timestamp;
        temp->process = process;
        temp->eventType = eventType;
        temp->eventNumber = eventNumber++;
        
        for(it=eventList.begin();it!=eventList.end();it++){
            if(CompareEvent(*it,temp)){
                eventList.insert(it,temp);
                return;
            }
        }
        eventList.push_back(temp);
    }
    
    string getEventTypeString(eventDT eventType){
        switch(eventType){
            case TRANSITION_TO_BLOCKED: return "TRANSITION_TO_BLOCKED"; break;
            case TRANSITION_TO_PREEMPT: return "TRANSITION_TO_PREEMPT"; break;
            case TRANSITION_TO_READY: return "TRANSITION_TO_READY"; break;
            case TRANSITION_TO_RUN: return "TRANSITION_TO_RUN"; break;
            case TRANSITION_TO_DONE: return "TRANSITION_TO_DONE"; break;
            default: return "!!!!!!!!!!!!!  UNKNOWN EVENT TYPE  !!!!!!!!!!!!!!!";
        }
    }
    
    void printEvent(eventNode* temp){
        cout<<"Process:"<<temp->process->processID<<", Event-> Timestamp:"<<temp->timestamp<<", Event Type:"<<getEventTypeString(temp->eventType)<<endl;
    }
    
    void printAllEvent(){
        eventNode* temp;
        for(it=eventList.begin();it!=eventList.end();it++){
            temp = *it;
            printEvent(temp);    
        }
    }
};

Event* globalEvtObj;

class Scheduler{
    public:
    vector<processNode*> readyQueue[2][NUMBER_OF_PRIORITY_LEVELS];
    int freshQueue;
    vector<processNode*> blockedQueue;
    processNode* runningProcess;
    
    Scheduler(){
        runningProcess = NULL;
        freshQueue = 0;
    }
    
    processNode* createProcess(int pid, int arrivalTime,int totalCPUTime,int CB,int IO){
        processNode* newProcess = new processNode();
        newProcess->processID = pid;
        newProcess->arrivalTime = arrivalTime;
        newProcess->totalCPUTime = totalCPUTime;
        newProcess->CB = CB;
        newProcess->IO = IO;
        newProcess->currentCB = 0;
        newProcess->CPUTimeUsed = 0;
        newProcess->currentIO = 0;
        newProcess->staticPriority = getRandomNumber(NUMBER_OF_PRIORITY_LEVELS);
        newProcess->currentPriority = newProcess->staticPriority - 1;
        newProcess->timeSinceLastStateChange = arrivalTime;
        newProcess->processState = STATE_READY;
        return newProcess;
    }
    
    void moveProcessToBlockedQueue(processNode* proc){
        if(blockedQueue.size()==0){
            noIOTime += currentTime - lastIOTime;
        }
        blockedQueue.push_back(proc);
    }
    
    void removeProcessFromBlockedQueue(processNode* proc){
        for(int i=0;i<blockedQueue.size();i++){
            if(blockedQueue[i]->processID == proc->processID){
                if(blockedQueue.size()==1){
                    lastIOTime = currentTime;
                }
                blockedQueue.erase(blockedQueue.begin()+i);
                return;
            }
        }
    }
    
    virtual processNode* getNextProcess()=0;
    virtual void addProcessToReadyQueue(processNode* process)=0;
    virtual void printSchedularName()=0;
};

class FIFOScheduler: public Scheduler {
    public:
    processNode* getNextProcess(){
        if(readyQueue[0][0].size()==0)
            return NULL;
        processNode* nextProcess = readyQueue[0][0][0];
        readyQueue[0][0].erase(readyQueue[0][0].begin());
        return nextProcess;
    }
    
    void addProcessToReadyQueue(processNode* process){
        readyQueue[0][0].push_back(process);
    }
    
    void printSchedularName(){
        cout<<"FCFS\n";
    }
};

class LCFSScheduler: public FIFOScheduler {
    public:
    void addProcessToReadyQueue(processNode* process){
        readyQueue[0][0].insert(readyQueue[0][0].begin(),process);
    }
    
    void printSchedularName(){
        cout<<"LCFS\n";
    }
};

class SRTFScheduler: public FIFOScheduler {
    public:
    void addProcessToReadyQueue(processNode* process){
        int RT = process->totalCPUTime-process->CPUTimeUsed;
        int i=0;
        for(;i<readyQueue[0][0].size();i++){
            int temp = (readyQueue[0][0][i]->totalCPUTime - readyQueue[0][0][i]->CPUTimeUsed); 
            if(temp<=RT) continue;
            break;
        }
        readyQueue[0][0].insert(readyQueue[0][0].begin()+i,process);
    }
    
    void printSchedularName(){
        cout<<"SRTF\n";
    }
};

class RRScheduler: public FIFOScheduler {
    void printSchedularName(){
        cout<<"RR "<<timeQuantum<<endl;
    }
};

class PRIOScheduler: public Scheduler {
    public:
    bool getIndexOfHighestPriorityJob(int queueNumber,int &i){
        for(int prio=NUMBER_OF_PRIORITY_LEVELS-1;prio>=0;prio--){
            if(readyQueue[queueNumber][prio].size()!=0){
                i = prio;
                return true;
            }
        }
        i=-1;
        return false;
    }
    
    processNode* getNextProcess(){
        processNode* nextProcess = NULL;
        int i=-1;
        if(getIndexOfHighestPriorityJob(freshQueue,i)){
            nextProcess = readyQueue[freshQueue][i][0];
            readyQueue[freshQueue][i].erase(readyQueue[freshQueue][i].begin());
        }else{
            freshQueue = (freshQueue + 1)%2;
            if(getIndexOfHighestPriorityJob(freshQueue,i)){
                nextProcess = readyQueue[freshQueue][i][0];
                readyQueue[freshQueue][i].erase(readyQueue[freshQueue][i].begin());
            }
        }
        return nextProcess;
    }
    
    void addProcessToReadyQueue(processNode* process){
       int queueNumber = freshQueue;
       if(process->currentPriority<0){
            process->currentPriority = process->staticPriority-1;
            queueNumber = (freshQueue + 1) % 2;
        }
       readyQueue[queueNumber][process->currentPriority].push_back(process);
    }
    
    void printSchedularName(){
        cout<<"PRIO "<<timeQuantum<<endl;
    }
};

class PREPRIOScheduler: public PRIOScheduler {
    public:
    void addProcessToReadyQueue(processNode* process){
        if(runningProcess!=NULL && runningProcess->currentPriority<process->currentPriority && !globalEvtObj->anyEventPendingForProcessOnTimstamp(runningProcess->processID,currentTime)){
            if(VERBOSE)
                printf("---> PRIO preemption %d by %d, at %d ? --> YES\n",runningProcess->processID,process->processID,currentTime);
            globalEvtObj->removeEvent(runningProcess);
            globalEvtObj->addEvent(currentTime,runningProcess,TRANSITION_TO_PREEMPT);
        }
        else if(VERBOSE && runningProcess!=NULL && runningProcess->processID!=process->processID)
                printf("---> PRIO preemption %d by %d, at %d ? --> NO\n",runningProcess->processID,process->processID,currentTime);
            
        int queueNumber = freshQueue;
        if(process->currentPriority<0){
                process->currentPriority = process->staticPriority-1;
                queueNumber = (freshQueue + 1) % 2;
        }
        readyQueue[queueNumber][process->currentPriority].push_back(process);
    }
    
    void printSchedularName(){
        cout<<"PREPRIO "<<timeQuantum<<endl;
    }
};


//********************************************************************************************
//********************************************************************************************

void readInputFile(ifstream &input, Event &obj, Scheduler* sch){
    string str,temp;
    int val[4];
    int pid = 0;
    while(getline(input,str)){
        stringstream ss(str);
        for(int i=0;i<4;i++){
            ss>>temp;
            val[i] = atoi(temp.c_str());
        }
        processNode* temp = sch->createProcess(pid++,val[0],val[1],val[2],val[3]);
        processList.push_back(temp);
        obj.addEvent(val[0],temp,TRANSITION_TO_READY);
    }
}

void fetchNextProcessAndUpdateStats(Event &evt,Scheduler* sch){
        sch->runningProcess = sch->getNextProcess();
        processNode* curProcess = sch->runningProcess;
        
        if(sch->runningProcess==NULL)
            return;
        
        sch->runningProcess->processState = STATE_RUNNING;
        sch->runningProcess->processStatistics.waitTime += currentTime - sch->runningProcess->timeSinceLastStateChange;

        if(sch->runningProcess->currentCB<=0){
            int temp1 = getRandomNumber(sch->runningProcess->CB);
            int temp2 = (sch->runningProcess->totalCPUTime - sch->runningProcess->CPUTimeUsed);
            sch->runningProcess->currentCB = temp1>temp2?temp2:temp1;
            
        }        
        
        if(sch->runningProcess->currentCB+sch->runningProcess->CPUTimeUsed==sch->runningProcess->totalCPUTime && sch->runningProcess->currentCB<=timeQuantum){
            evt.addEvent(currentTime+sch->runningProcess->currentCB,sch->runningProcess,TRANSITION_TO_DONE);
        }else if(sch->runningProcess->currentCB>timeQuantum){
            evt.addEvent(currentTime+timeQuantum,sch->runningProcess,TRANSITION_TO_PREEMPT);
        }else if(sch->runningProcess->currentCB<=timeQuantum){
            evt.addEvent(sch->runningProcess->currentCB+currentTime,sch->runningProcess,TRANSITION_TO_BLOCKED);
        }else if(VERBOSE){
            printf("Current CB:%d, CB Ran for: %d\n",sch->runningProcess->currentCB,currentTime-sch->runningProcess->timeSinceLastStateChange);
            cout<<" !!!!!!!!   UNKNOWN CONDITION IN fetchNextProcessAndUpdate  !!!!!!!!!!!!!\n";
        }
        
        if(VERBOSE)
            cout<<currentTime<<" "<<curProcess->processID<<" "<<currentTime-curProcess->timeSinceLastStateChange<<": READY -> RUNNG cb="<<curProcess->currentCB<<" rem="<<curProcess->totalCPUTime-curProcess->CPUTimeUsed<<" prio="<<curProcess->currentPriority<<endl;
        
        sch->runningProcess->timeSinceLastStateChange = currentTime;
}

void schedulerRoutine(eventNode *curEvent,Event &evt,Scheduler* sch,bool scheduleNextJob){
    processNode* curProcess = curEvent->process;
    switch(curProcess->processState){
        
        case STATE_BLOCKED:
            if(VERBOSE && curProcess->processID!=sch->runningProcess->processID)
                cout<<" ERROR: THE PROCESS BEING BLOCKED IS NOT RUNNING !!!!!!!!\n";
            
            sch->moveProcessToBlockedQueue(curProcess);
            curProcess->currentIO = getRandomNumber(curProcess->IO);
            curProcess->CPUTimeUsed += currentTime - curProcess->timeSinceLastStateChange;
            curProcess->processStatistics.ioTime += curProcess->currentIO; 
            evt.addEvent(currentTime+curProcess->currentIO,curProcess,TRANSITION_TO_READY);
            curProcess->currentCB = 0;
            
            if(VERBOSE)
                cout<<currentTime<<" "<<curProcess->processID<<" "<<currentTime-curProcess->timeSinceLastStateChange<<": RUNNG -> BLOCK  ib="<<curProcess->currentIO<<" rem="<<curProcess->totalCPUTime-curProcess->CPUTimeUsed<<endl;
            
            curProcess->currentPriority = curProcess->staticPriority-1;
            curProcess->timeSinceLastStateChange = currentTime;
            sch->runningProcess = NULL;
            break;
        
        case STATE_PREEMPTED:
            if(VERBOSE && curProcess->processID!=sch->runningProcess->processID)
                cout<<" ERROR: THE PROCESS BEING PREEMPTED IS NOT RUNNING !!!!!!!!\n";
            
            curProcess->processState = STATE_READY;
            curProcess->currentCB -= currentTime - curProcess->timeSinceLastStateChange;
            curProcess->CPUTimeUsed += currentTime - curProcess->timeSinceLastStateChange;
            if(curProcess->currentCB==0 && curProcess->CPUTimeUsed!=curProcess->totalCPUTime){
                curProcess->currentCB = getRandomNumber(curProcess->CB);
            }else if(VERBOSE && curProcess->CPUTimeUsed==curProcess->totalCPUTime){
                cout<<"!!!!!!!!!!!  PROCESS WHICH IS FINISHED IS BEING PREMPTED !!!!!!!!!!!!!!!\n";
            }
            
            if(VERBOSE)
                cout<<currentTime<<" "<<curProcess->processID<<" "<<currentTime-curProcess->timeSinceLastStateChange<<": RUNNG -> READY  cb="<<curProcess->currentCB<<" rem="<<curProcess->totalCPUTime-curProcess->CPUTimeUsed<<" prio="<<curProcess->currentPriority<<endl;
            
            curProcess->currentPriority--;
            sch->addProcessToReadyQueue(curProcess);
            curProcess->timeSinceLastStateChange = currentTime;
            sch->runningProcess = NULL;
            break;
        
        case STATE_DONE:
            if(VERBOSE && curProcess->processID!=sch->runningProcess->processID)
                cout<<" ERROR: THE PROCESS GETTING FINISHED WAS NOT RUNNING !!!!!!!!\n";
            
            curProcess->CPUTimeUsed += currentTime - curProcess->timeSinceLastStateChange;
            curProcess->processStatistics.finishingTime = currentTime;
            curProcess->processStatistics.turnaroundTime = currentTime - curProcess->arrivalTime;
            lastProcessToFinish = curProcess;
            
            if(VERBOSE)
                cout<<currentTime<<" "<<curProcess->processID<<" "<<currentTime-curProcess->timeSinceLastStateChange<<": Done"<<endl;
            
            curProcess->timeSinceLastStateChange = currentTime;
            sch->runningProcess = NULL;
            break;
        
        case STATE_RUNNING:
            if(VERBOSE)
                cout<<"!!!!!  Scheduler: NOTHING TO DO  !!!!!\n";
            break;
        
        case STATE_READY:
            if(VERBOSE){
                if(curProcess->CPUTimeUsed==0)
                    cout<<currentTime<<" "<<curProcess->processID<<" "<<currentTime-curProcess->timeSinceLastStateChange<<": CREATED -> READY"<<endl;
                else
                    cout<<currentTime<<" "<<curProcess->processID<<" "<<currentTime-curProcess->timeSinceLastStateChange<<": BLOCK -> READY"<<endl;
            }
            
            curProcess->timeSinceLastStateChange = currentTime;
            sch->addProcessToReadyQueue(curProcess);
            sch->removeProcessFromBlockedQueue(curProcess);
            break;
        
        default: if(VERBOSE) cout<<"!!!!!!!!!!!!!!!!! UNKNOWN ERROR IN SCHEDULER ROUTINE !!!!!!!!!!!!!!!!\n";
    }
    
    if(sch->runningProcess==NULL && scheduleNextJob ){
        fetchNextProcessAndUpdateStats(evt,sch);
    }
}

void simulation(Event &evt, Scheduler* sch){
    eventNode* curEvent = evt.getNextEvent();
    while(curEvent!=NULL){
        currentTime = curEvent->timestamp;
        bool scheduleNextJob = true;

        switch(curEvent->eventType){
            case TRANSITION_TO_READY: curEvent->process->processState = STATE_READY; break;
            case TRANSITION_TO_RUN: break;
            case TRANSITION_TO_BLOCKED: curEvent->process->processState = STATE_BLOCKED; break;
            case TRANSITION_TO_PREEMPT: curEvent->process->processState = STATE_PREEMPTED; break;
            case TRANSITION_TO_DONE: curEvent->process->processState = STATE_DONE; break;
            default: if(VERBOSE) cout<<"!!!!!!!!!!!!!!!!!!   SIMULATION: UNKNOWN EVENT TYPE  !!!!!!!!!!!!!!!!!!!";
        }
        
        if(evt.seeNextEvent()!=NULL && evt.seeNextEvent()->timestamp==currentTime){
            scheduleNextJob = false;
        }
        
        schedulerRoutine(curEvent,evt,sch,scheduleNextJob);
        delete(curEvent);
        curEvent = evt.getNextEvent();
    }
}

void printSummary(){
    int cpuTimeSum = 0;
    int waitTimeSum = 0;
    int turnAroundTimeSum = 0;
    for(int i=0;i<processList.size();i++){
        processNode* p = processList[i];
        cpuTimeSum += p->totalCPUTime;
        waitTimeSum += p->processStatistics.waitTime;
        turnAroundTimeSum += p->processStatistics.finishingTime - p->arrivalTime;
        printf("%04d: %4d %4d %4d %4d %1d | %5d %5d %5d %5d\n",i,p->arrivalTime,p->totalCPUTime,p->CB,p->IO,p->staticPriority,p->processStatistics.finishingTime,p->processStatistics.finishingTime-p->arrivalTime,p->processStatistics.ioTime,p->processStatistics.waitTime);
    }
    int ft = lastProcessToFinish->processStatistics.finishingTime;
    double averageCPUUtil = cpuTimeSum*100.0/lastProcessToFinish->processStatistics.finishingTime;
    double averageWait = waitTimeSum*1.0/processList.size();
    double averageTurnaround = turnAroundTimeSum*1.0/processList.size();
    noIOTime += currentTime - lastIOTime;
    double averageIOUtil = 100.0*(ft-noIOTime)/ft;
    printf("SUM: %d %.2lf %.2lf %.2lf %.2lf %.3lf\n",lastProcessToFinish->processStatistics.finishingTime,averageCPUUtil,averageIOUtil,averageTurnaround,averageWait,processList.size()*100.0/lastProcessToFinish->processStatistics.finishingTime);
}

int main(int argc, char** argv){
    int c;
    string schedulerType;
    while ( (c = getopt (argc, argv, "vs:")) != -1){
        switch(c){
            case 'v' : VERBOSE = true;
            break;
            case 's' : schedulerType = optarg;
            break;
            case '?' : if(optopt == 's') cout<<"Specify a scheduler after -s\n"; exit(1);
            break;
            default: cout<<((char)c); exit(99);;
        }
    }
    
    if(argc-optind<2){ cout<<"Specify input and random number file.\n"; exit(1);}
    ifstream inputFileStream(argv[optind]);
    ifstream randomNumberFileStream(argv[optind+1]);
    Event evt;
    globalEvtObj = &evt;
    Scheduler* sch;

    switch(schedulerType[0]){
        case 'F' : sch = new FIFOScheduler; break;
        case 'L' : sch = new LCFSScheduler; break;
        case 'S' : sch = new SRTFScheduler; break;
        case 'R' : sch = new RRScheduler;  timeQuantum = atoi(schedulerType.substr(1,schedulerType.length()-1).c_str()); break;
        case 'P' : sch = new PRIOScheduler; timeQuantum = atoi(schedulerType.substr(1,schedulerType.length()-1).c_str()); break;
        case 'E' : sch = new PREPRIOScheduler; timeQuantum = atoi(schedulerType.substr(1,schedulerType.length()-1).c_str()); break;
        default: cout<<"Sceduler not supported:"<<schedulerType<<endl; exit(99); 
    }
    
    readRandomNumbersFromFile(randomNumberFileStream);
    readInputFile(inputFileStream,evt,sch);
    simulation(evt,sch);
    sch->printSchedularName();
    printSummary();
}

