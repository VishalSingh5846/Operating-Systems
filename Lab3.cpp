#define COST_MAP 400
#define DEFAULT_FRAME_COUNT 16
#define COST_UNMAP 400
#define COST_PAGE_IN 3000
#define COST_PAGE_OUT 3000
#define COST_FILE_IN 2500
#define COST_FILE_OUT 2500
#define COST_ZERORING_PAGE 150
#define COST_SEGV 240
#define COST_SEGPROT 300
#define COST_CONTEXT_SWITCH 121
#define COST_PROCESS_EXIT 175
#define COST_READ 1
#define COST_WRITE 1
#define MAX_FRAME_COUNT 128
#define MAX_FRAME_SIZE_IN_BITS 7
#define PAGE_TABLE_SIZE 64
#define ESC_REFERENCE_BIT_CLEAR_PERIOD_IN_INSTRUCTION_COUNT 50
#define WORKING_SET_INSTRUCTION_THRESHOLD 50
//********************************************************************************************
#include <fstream>
#include <string>
#include <string.h>
#include <unistd.h>
#include <vector>
#include <stdlib.h>
#include <list>
using namespace std;
//********************************************************************************************
//********************************************************************************************
bool printPageTableOfCurrentProcessAfterEachInstructionFlag = false;
bool printPageTableOfAllProcessAfterEachInstructionFlag = false;
bool printFrameTableFlag = false;
bool printFrameTableAfterEachIntructionFlag = false;
bool printPageTableFlag = false;
bool printSummaryFlag = false;
bool DEBUG = false;
bool VERBOSE = false;
//********************************************************************************************
int FRAME_COUNT = DEFAULT_FRAME_COUNT;
unsigned long instructionCounter = 0;
unsigned long long cycleCounter = 0;
unsigned long contextSwitchCounter = 0;
unsigned long processExitCounter = 0;
//********************************************************************************************
int numProcess = 0;
int currentProcess = -1;
int randomValueCounter = 0;
vector<int> randomValueVector;
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
        fprintf(stderr,"Please provide a valid random value file.\n");
        exit(0);
    }
}
int getRandomNumber(int num){
    int ans  = (randomValueVector[randomValueCounter] % num);
    randomValueCounter=(randomValueCounter+1)%randomValueVector.size();
    return ans;   
}

//********************************************************************************************

struct FrameDT{int processCurrentlyHolding; int pageNumber;unsigned int age;unsigned int lastModified;
    FrameDT(){
        processCurrentlyHolding = -1;
        pageNumber = -1;
        age = 0;
        lastModified = 0;
    };
};
struct VMA{ int start;int end;int writeProtected;int fileMapped;
    VMA(){
        start = -1;
        end = -1;
        writeProtected = false;
        fileMapped = false;
    };
};
struct PageTableEntry{
    unsigned int validBit:1;
    unsigned int writeProtectBit:1;
    unsigned int modifyBit:1;
    unsigned int referencedBit:1;
    unsigned int pagedOutBit:1;
    unsigned int fileMappedBit:1;
    unsigned int isPartOfVMABit:1;
    unsigned int vmaCheckDoneStatusBit:1;
    unsigned int frameNumber:MAX_FRAME_SIZE_IN_BITS;
    
    PageTableEntry(){
        validBit = 0;
        writeProtectBit = 0;
        modifyBit = 0;
        referencedBit = 0;
        pagedOutBit = 0;
        fileMappedBit = 0;
        isPartOfVMABit = 0;
        vmaCheckDoneStatusBit = 0;
        frameNumber = 0;
    };
};
struct VMMStats{
    unsigned long SEGV;
    unsigned long SEGPROT;
    unsigned long UNMAP;
    unsigned long MAP;
    unsigned long PAGEIN;
    unsigned long PAGEOUT;
    unsigned long ZERO;
    unsigned long FILEIN;
    unsigned long FILEOUT;
    VMMStats(){
        SEGV = 0;
        SEGPROT = 0;
        UNMAP = 0;
        MAP = 0;
        PAGEIN = 0;
        PAGEOUT = 0;
        ZERO =0;
        FILEIN = 0;
        FILEOUT = 0;
    }
};
struct ProcessDT{int processID;bool done;PageTableEntry pageTable[PAGE_TABLE_SIZE]; list<VMA> vmaList; VMMStats stats;
    ProcessDT(){done= false;processID=-1;};} ;

//********************************************************************************************

FrameDT frames[MAX_FRAME_COUNT]; 
ProcessDT* processTable;

//********************************************************************************************

class Pager{
    public:
    int counter;
    list <int> freeFrames;
    Pager(){
        counter = 0;
        for(int i=0;i<FRAME_COUNT;i++)
            freeFrames.push_back(i);
    }
    int getFrameFromFreeList(){
        if(freeFrames.size() > 0){
            int ans = freeFrames.front();
            freeFrames.pop_front();
            return ans;
        }
        return -1;
    }
    void freeUpFrame(int frameNumber){
        frames[frameNumber].processCurrentlyHolding = -1;
        freeFrames.push_back(frameNumber);
    }
    virtual int getVictimFrame() = 0;
};
class FIFOPager : public Pager{
    public: 
    int getVictimFrame(){
        int ans = getFrameFromFreeList();
        if(ans>=0) return ans;
        ans = counter;
        counter = (counter+1)%FRAME_COUNT;
        return  ans;
    }
};
class RandomPager : public Pager{
    public:
    int getVictimFrame(){
        int ans = getFrameFromFreeList();
        if(ans>=0) return ans;
        return getRandomNumber(FRAME_COUNT);
    }
};
class ClockPager : public Pager{
    public:
    int getVictimFrame(){
        int ans = getFrameFromFreeList();
        if(ans>=0) return ans;
        for(int j=0;j<FRAME_COUNT;j++){
            PageTableEntry* pte = &processTable[frames[counter].processCurrentlyHolding].pageTable[frames[counter].pageNumber];
            if(pte->referencedBit==0)
                break;
            else
                pte->referencedBit=0;
            counter = (1+counter)%FRAME_COUNT;
        }
        ans = counter;
        counter = (counter+1)%FRAME_COUNT;
        return ans;
    }
};
class ESCPager : public Pager{
    int lastInterruptAtInstructionCounterVal;
    public:
    ESCPager(){ lastInterruptAtInstructionCounterVal = 0;}
    void simulateHardwareInterrupt(){
        if(instructionCounter-lastInterruptAtInstructionCounterVal>=ESC_REFERENCE_BIT_CLEAR_PERIOD_IN_INSTRUCTION_COUNT){
            lastInterruptAtInstructionCounterVal = instructionCounter;
            for(int i=0;i<FRAME_COUNT;i++){
                processTable[frames[i].processCurrentlyHolding].pageTable[frames[i].pageNumber].referencedBit = 0;
            } 
        }
    }
    void searchForClass(int counter,int &ans00,int &ans01,int &ans10, int &ans11){
        ans00 = -1;
        ans01 = -1;
        ans10 = -1;
        ans11 = -1;
        for(int i=0;i<FRAME_COUNT;i++){
            PageTableEntry* pte = &processTable[frames[counter].processCurrentlyHolding].pageTable[frames[counter].pageNumber];
            if(pte->referencedBit==0 && pte->modifyBit==0){
                ans00 = counter;
                return;
            }else if(pte->referencedBit==0 && pte->modifyBit==1 && ans01<0){
                ans01 = counter;
            }else if(pte->referencedBit==1 && pte->modifyBit==0 && ans10<0){
                ans10 = counter;
            }else if(pte->referencedBit==1 && pte->modifyBit==1 && ans11<0){
                ans11 = counter;
            }
            counter = (counter + 1)%FRAME_COUNT;
        }
    }
    public:
    int getVictimFrame(){
        int ans = getFrameFromFreeList();
        if(ans>=0) return ans;
        int ans00,ans01,ans10,ans11;
        searchForClass(counter,ans00,ans01,ans10,ans11); 
        if(ans00>=0) ans = ans00;
        else if(ans01>=0) ans = ans01;
        else if(ans10>=0) ans = ans10;
        else ans = ans11;
        counter = (ans+1)%FRAME_COUNT;
        simulateHardwareInterrupt();
        return ans;
    }
};
class AgingPager : public Pager{
    void modifyAge(){
        for(int i=0;i<FRAME_COUNT;i++){
            PageTableEntry* pte = &processTable[frames[i].processCurrentlyHolding].pageTable[frames[i].pageNumber];
            int refBit = pte->referencedBit;
            pte->referencedBit = 0;
            frames[i].age = (refBit << 31) + (frames[i].age >> 1);
        }
    }
    public:
    int getVictimFrame(){
        int ans = getFrameFromFreeList();
        if(ans>=0) return ans;
        modifyAge();
        int min = counter;
        for(int j=1;j<FRAME_COUNT;j++){
            int i = (counter + j)%FRAME_COUNT;
            if(frames[min].age>frames[i].age)
                min = i;
        }
        counter = (min+1)%FRAME_COUNT;
        return min;
    }
};
class WorkingSetPager : public Pager{
    public:
    int getVictimFrame(){
        int ans = getFrameFromFreeList();
        if(ans>=0) return ans;
        int secondBest=counter;
        PageTableEntry* pte;
        for(int i=0;i<FRAME_COUNT;i++){
            pte = &processTable[frames[counter].processCurrentlyHolding].pageTable[frames[counter].pageNumber];
            if(pte->referencedBit==1){
                frames[counter].lastModified = instructionCounter-1;
                pte->referencedBit = 0;
            }
            else if(instructionCounter - frames[counter].lastModified>WORKING_SET_INSTRUCTION_THRESHOLD && ans<0){
                ans = counter;
                break;
            }
            else if(frames[counter].lastModified<frames[secondBest].lastModified){
                secondBest = counter;
            }
            counter = (counter + 1)%FRAME_COUNT;
        }
        if(ans<0) ans = secondBest;
        counter = (ans + 1)%FRAME_COUNT;
        return ans;
    }
};

class Swap{
    public:
    void getFromSwap(int process,int pageNumber,int frameNumber){
        frames[frameNumber].processCurrentlyHolding = process;
        frames[frameNumber].pageNumber = pageNumber;
        frames[frameNumber].age = 0;
        frames[frameNumber].lastModified = instructionCounter-1;
        processTable[process].pageTable[pageNumber].frameNumber = frameNumber;
    }
    void flushToSwap(int frameNumber){
    }
};

//********************************************************************************************

Pager* pager;
ifstream inputFile;
Swap swapDevice;

//********************************************************************************************

bool getNextLineFromInput(string &str){
    while(getline(inputFile,str)){
        bool isComment = false;
        for(unsigned int i=0;i<str.length();i++){
            if(str[i]=='#'){isComment = true;break;}
        }
        if(isComment) continue;
        return true;
    }
    return false;
}
void getProcessInformation(){
    string line;
    getNextLineFromInput(line);
    numProcess = atoi(line.c_str());
    processTable = new ProcessDT[numProcess];
    for(int i=0;i<numProcess;i++){
        getNextLineFromInput(line);
        int numVMAs = atoi(line.c_str());
        processTable[i].processID = i;
        for(int j=0;j<numVMAs;j++){
            getNextLineFromInput(line);
            VMA temp;
            sscanf(line.c_str(),"%d %d %d %d",&temp.start,&temp.end,&temp.writeProtected,&temp.fileMapped);
            processTable[i].vmaList.push_back(temp);
        }
    }
}
void printProcessInfo(){
    printf("\nNumber of process:%d",numProcess);
    for(int i=0;i<numProcess;i++){
        printf("\n\nProcess %d\n",i);
        for(list<VMA>::iterator it=processTable[i].vmaList.begin();it!=processTable[i].vmaList.end();it++){
            printf("Start: %d, End: %d, WP: %d, FM: %d\n",(*it).start,(*it).end,(*it).writeProtected,(*it).fileMapped);
        }
    }
}

//********************************************************************************************

bool isPartOfVMA(int pageNumber,int process,VMA* &vma){
    for(list<VMA>::iterator it = processTable[process].vmaList.begin();it!=processTable[process].vmaList.end();it++){
        if(pageNumber<=(*it).end && pageNumber>=(*it).start){
            vma = &(*it);
            return true;
        }
    }
    vma = NULL;
    return false;
}
bool isFileMapped(int process, int pageNumber){
    for(list<VMA>::iterator it = processTable[process].vmaList.begin();it!=processTable[process].vmaList.end();it++){
        if(pageNumber<=(*it).end && pageNumber>=(*it).start){
            return (*it).fileMapped==1;
        }
    }
    return false;
}
void getPageFromSwap(int process,int pageNumber,int newFrameNumber){
    processTable[process].stats.PAGEIN += 1;
    cycleCounter += COST_PAGE_IN;
    swapDevice.getFromSwap(process,pageNumber,newFrameNumber);
}
void getFileFromSwap(int process,int pageNumber,int newFrameNumber){
    processTable[process].stats.FILEIN += 1;
    cycleCounter += COST_FILE_IN;
    swapDevice.getFromSwap(process,pageNumber,newFrameNumber);
}
void flushPageToSwap(int frameNumber){
    processTable[frames[frameNumber].processCurrentlyHolding].stats.PAGEOUT += 1;
    cycleCounter += COST_PAGE_OUT;
    swapDevice.flushToSwap(frameNumber);
}
void flushFileToSwap(int frameNumber){
    processTable[frames[frameNumber].processCurrentlyHolding].stats.FILEOUT += 1;
    cycleCounter += COST_FILE_OUT;
    swapDevice.flushToSwap(frameNumber);
}
void saveVictimFrameIfRequired(int frameNumber){
    if(frames[frameNumber].processCurrentlyHolding>=0){
        PageTableEntry* pageTable = processTable[frames[frameNumber].processCurrentlyHolding].pageTable;
        int pageNumber =  frames[frameNumber].pageNumber;
        int process = frames[frameNumber].processCurrentlyHolding;
        PageTableEntry* page = &pageTable[pageNumber];
        if(VERBOSE) printf(" UNMAP %d:%d\n",process,pageNumber);
        processTable[process].stats.UNMAP++;
        cycleCounter += COST_UNMAP;
        page->validBit = 0;
        page->referencedBit = 0;
        
        if(page->modifyBit==1){
            page->modifyBit = 0;
            if(page->fileMappedBit==0){
                page->pagedOutBit = 1;
                flushPageToSwap(frameNumber);
            }else 
                flushFileToSwap(frameNumber);
                
            if(VERBOSE){
                if(page->fileMappedBit==1) printf(" FOUT\n");
                else printf(" OUT\n");
            } 
        }
    }
}
void segmentProtRoutine(int process){
    processTable[process].stats.SEGPROT += 1;
    cycleCounter += COST_SEGPROT;
    if(VERBOSE) printf(" SEGPROT\n");
}

//********************************************************************************************

void pageFaultHandler(int pageNumber){
    PageTableEntry* pageEntry = &processTable[currentProcess].pageTable[pageNumber];
    if(pageEntry->vmaCheckDoneStatusBit == 0){
        VMA* vma = NULL;
        bool legalAddress = isPartOfVMA(pageNumber,currentProcess,vma);
        if(legalAddress) pageEntry->isPartOfVMABit = 1;
        if(vma!=NULL && vma->fileMapped) pageEntry->fileMappedBit = 1;
        if(vma!=NULL && vma->writeProtected) pageEntry->writeProtectBit = 1;
        pageEntry->vmaCheckDoneStatusBit = 1;
    }
    if(pageEntry->isPartOfVMABit==0){
        processTable[currentProcess].stats.SEGV += 1;
        cycleCounter += COST_SEGV;
        if(VERBOSE) printf(" SEGV\n");
        return;
    }
    if(pageEntry->validBit==1){
        if(pageEntry->writeProtectBit==1){
            segmentProtRoutine(currentProcess);
            return;
        }else if(DEBUG) fprintf(stderr,"Error: Entry is valid, and is part of VMA and not write protected, don't know why page fault was raised!\n");
    }
    if(pageEntry->pagedOutBit==1){
        int newFrameNumber = pager->getVictimFrame();
        saveVictimFrameIfRequired(newFrameNumber);
        processTable[currentProcess].pageTable[pageNumber].validBit = 1;
        processTable[currentProcess].stats.MAP++;
        if(pageEntry->fileMappedBit==1)
            getFileFromSwap(currentProcess,pageNumber,newFrameNumber);
        else
            getPageFromSwap(currentProcess,pageNumber,newFrameNumber);
        cycleCounter += COST_MAP;
        if(VERBOSE) printf(" IN\n MAP %d\n",newFrameNumber);
        return;
    }
    if(pageEntry->fileMappedBit==0){
        int newFrameNumber = pager->getVictimFrame();
        saveVictimFrameIfRequired(newFrameNumber);
        processTable[currentProcess].pageTable[pageNumber].validBit = 1;
        processTable[currentProcess].pageTable[pageNumber].frameNumber = newFrameNumber;
        processTable[currentProcess].stats.ZERO += 1;
        processTable[currentProcess].stats.MAP++;
        frames[newFrameNumber].processCurrentlyHolding = currentProcess;
        frames[newFrameNumber].pageNumber = pageNumber;
        frames[newFrameNumber].lastModified = instructionCounter-1;
        frames[newFrameNumber].age = 0;
        cycleCounter += COST_MAP;
        cycleCounter += COST_ZERORING_PAGE;
        if(VERBOSE) printf(" ZERO\n MAP %d\n",newFrameNumber);
        return;
    }
    if(pageEntry->fileMappedBit==1){
        int newFrameNumber = pager->getVictimFrame();
        saveVictimFrameIfRequired(newFrameNumber);
        processTable[currentProcess].pageTable[pageNumber].validBit = 1;
        processTable[currentProcess].pageTable[pageNumber].frameNumber = newFrameNumber;
        processTable[currentProcess].stats.FILEIN++;
        processTable[currentProcess].stats.MAP++;
        frames[newFrameNumber].processCurrentlyHolding = currentProcess;
        frames[newFrameNumber].pageNumber = pageNumber;
        frames[newFrameNumber].lastModified = instructionCounter-1;
        frames[newFrameNumber].age = 0;
        cycleCounter += COST_FILE_IN;
        cycleCounter += COST_MAP;
        if(VERBOSE) printf(" FIN\n MAP %d\n",newFrameNumber);
        return;
    }
    if(DEBUG) fprintf(stderr,"Error: Unknown Reason for page fault!\n");
}

//********************************************************************************************

void printPageTable(int i){
    printf("PT[%d]:",i);
        for(int j = 0;j<PAGE_TABLE_SIZE;j++){
            PageTableEntry * pte = &processTable[i].pageTable[j];
            printf(" ");
            if(processTable[i].done){ printf("*"); continue;}
            if(pte->validBit==0 && pte->pagedOutBit==1){ printf("#"); continue;}
            if(pte->validBit==0){ printf("*"); continue; }
            printf("%d:",j);
            if(pte->referencedBit==1) printf("R"); else printf ("-");
            if(pte->modifyBit==1) printf("M"); else printf ("-");
            if(pte->pagedOutBit==1) printf("S"); else printf ("-");
        }
        printf("\n");
}
void printPageTableForAllProcess(){
    for(int i=0;i<numProcess;i++)
        printPageTable(i);
}
void printFrameTable(){
    printf("FT:");
    for(int i=0;i<FRAME_COUNT;i++){
        if(frames[i].processCurrentlyHolding>=0)
            printf(" %d:%d",frames[i].processCurrentlyHolding,frames[i].pageNumber);
        else printf(" *");
    }
    printf("\n");
}
void printVMAStats(){
    for(int i=0;i<numProcess;i++){
        printf("PROC[%d]: U=%lu M=%lu I=%lu O=%lu FI=%lu FO=%lu Z=%lu SV=%lu SP=%lu\n",i,
            processTable[i].stats.UNMAP, processTable[i].stats.MAP, processTable[i].stats.PAGEIN, processTable[i].stats.PAGEOUT,
            processTable[i].stats.FILEIN, processTable[i].stats.FILEOUT, processTable[i].stats.ZERO,
            processTable[i].stats.SEGV, processTable[i].stats.SEGPROT);
    }
    printf("TOTALCOST %lu %lu %lu %llu\n", instructionCounter, contextSwitchCounter, processExitCounter, cycleCounter);
}

//********************************************************************************************

void handleExit(int process){
    for(int i=0;i<PAGE_TABLE_SIZE;i++){
        if(processTable[process].pageTable[i].validBit==1 && processTable[process].pageTable[i].fileMappedBit==1 && processTable[process].pageTable[i].modifyBit==1){
            flushFileToSwap(processTable[process].pageTable[i].frameNumber);
            processTable[process].stats.FILEOUT += 1;
            cycleCounter += COST_FILE_OUT;
            if(VERBOSE) printf(" FOUT\n");
        }
        if(processTable[process].pageTable[i].validBit==1){
            processTable[process].stats.UNMAP += 1;
            cycleCounter += COST_UNMAP;
            pager->freeUpFrame(processTable[process].pageTable[i].frameNumber);
            if(VERBOSE) printf(" UNMAP %d:%d\n",process,i);
        }
    }
}
void hardwareSimulation(){
    char instruction;
    int operand;
    string num;
    while(getNextLineFromInput(num)){
        bool segmentViolation = false;
        sscanf(num.c_str(), "%c %d",&instruction,&operand);
        if(VERBOSE) printf("%lu: ==> %c %d\n",instructionCounter,instruction,operand);
        instructionCounter++;
        PageTableEntry* page = &processTable[currentProcess].pageTable[operand];
        switch(instruction){
            case 'r':
                if(page->validBit==0)
                    pageFaultHandler(operand);
                if(page->validBit==1){
                    page->referencedBit = 1;
                }else segmentViolation = true;
                cycleCounter += COST_READ;
                break;
            case 'w':
                    if(page->validBit==0)
                        pageFaultHandler(operand);    
                    if(page->validBit==1 && page->writeProtectBit==1){
                        segmentProtRoutine(currentProcess);
                        page->referencedBit = 1;
                    }else if(page->validBit==1){
                        page->modifyBit = 1;
                        page->referencedBit = 1;
                    }else segmentViolation = true;
                    cycleCounter += COST_WRITE;
                break;
            case 'e':
                cycleCounter += COST_PROCESS_EXIT;
                processTable[currentProcess].done = true;
                processExitCounter += 1;
                if(VERBOSE) printf("EXIT current process %d\n",currentProcess);
                handleExit(currentProcess);
                break;
            case 'c':
                contextSwitchCounter += 1; 
                cycleCounter += COST_CONTEXT_SWITCH;
                currentProcess = operand;
                break;
            default: if(DEBUG) fprintf(stderr,"Error: Unknown Instruction encountered!\n"); 
        }
        if(instruction=='c') continue;
        if(instruction!='e' && !segmentViolation && printPageTableOfAllProcessAfterEachInstructionFlag) printPageTableForAllProcess();
        else if(instruction!='e' && !segmentViolation && printPageTableOfCurrentProcessAfterEachInstructionFlag) printPageTable(currentProcess);
        if(instruction!='e' && !segmentViolation && printFrameTableAfterEachIntructionFlag) printFrameTable();
    }
}

//********************************************************************************************

int main(int argc, char** argv){
    int c;
    string options,algorithm;
    while ( (c = getopt (argc, argv, ":a:o:f:")) != -1){
        switch(c){
            case 'a' : algorithm = optarg;  break;
            case 'o' : options = optarg;  break;
            case 'f' : FRAME_COUNT = atoi(optarg); if(FRAME_COUNT>128 || FRAME_COUNT<1){fprintf(stderr,"Supports frame count  [1,128]\n"); exit(99);}  break;
            case '?' : if(optopt=='o') break; else if(optopt=='a') fprintf(stderr,"Specify paging algorithm\n"); else if(optopt=='f') fprintf(stderr,"Specify number of frames\n"); exit(99); 
            case ':' : if(optopt=='o') break; else if(optopt=='a') fprintf(stderr,"Specify paging algorithm after -a\n"); else if(optopt=='f') fprintf(stderr,"Specify number of frames after -f\n"); exit(99);
            default: fprintf(stderr,"Unknown Option: %c\n",c); exit(99); 
        }
    }
    for(unsigned int i=0;i<options.length();i++){
        switch(options[i]){
                case 'F' : printFrameTableFlag = true;break;
                case 'y' : printPageTableOfAllProcessAfterEachInstructionFlag = true;break;
                case 'x' : printPageTableOfCurrentProcessAfterEachInstructionFlag = true;break;
                case 'f' : printFrameTableAfterEachIntructionFlag = true;break;
                case 'P' : printPageTableFlag = true;break;
                case 'O' : VERBOSE = true;break;
                case 'S' : printSummaryFlag = true;break;
                case 'D' : DEBUG = true;break;
                default: fprintf(stderr,"Unknown option flag: %c",c); exit(99);;
        }
    }
    if(argc-optind<2){ fprintf(stderr,"Specify input and random number file.\n"); exit(1);}
    inputFile.open(argv[optind]);
    ifstream randomNumberFileStream(argv[optind+1]);
    switch(algorithm[0]){
        case 'f' : pager = new FIFOPager();break;
        case 'r' : pager = new RandomPager();break;
        case 'c' : pager = new ClockPager();break;
        case 'e' : pager = new ESCPager();break;
        case 'a' : pager = new AgingPager();break;
        case 'w' : pager = new WorkingSetPager();break;
        default: fprintf(stderr,"Unknown Frame/Page replacement Algorithm: <%c>\n",algorithm[0]); exit(99);
    }
    readRandomNumbersFromFile(randomNumberFileStream);
    getProcessInformation();
    hardwareSimulation();
    if(printPageTableFlag) printPageTableForAllProcess();
    if(printFrameTableFlag) printFrameTable();
    if(printSummaryFlag) printVMAStats();
}



