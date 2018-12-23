#define MAX_TOKEN_SIZE    200000
#define MAX_PATH_SIZE     100000
#define MAX_MODULE_COUNT  200000
#define MAX_SYMBOL_SIZE   16
#define MAX_SYMBOL_COUNT  256
#define MAX_SYSTEM_MEMORY 512
#define MAX_DEF_COUNT     16
#define MAX_USELIST_COUNT 16

#include <iostream>
#include <fstream>
#include <cstring>
#include <stdlib.h>
#include <string.h>
using namespace std;

//---------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------

struct moduleDT{int id; int baseAddress; int moduleSize;};
struct symbolDT{char* symbol;bool used;int numberOfTimesDefined;int relativeAddress;int absoluteAddress; moduleDT* module; symbolDT(){used=false;numberOfTimesDefined=0;};};
struct stringSize3{char val[4];};

//---------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------

char delimiters[] = " \n\t";
char path[MAX_PATH_SIZE]; 
moduleDT moduleTable[MAX_MODULE_COUNT];
symbolDT symbolTable[MAX_SYMBOL_COUNT];
int symbolCount = 0;
int moduleCount = 0;

int currentTokenSize = 0;
int lineAfterLastToken = 0;
int charactersAfterLastTokenOrLine = 0;
int currentTokenLine = 0;
int currentTokenOffset=0;

char lastCharacterParsed = 'a';
int charactersInLastLineParsed = 0;
bool fileEndExecuted = false;
//---------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------


bool isCharacter(char x){
    return ((x<='Z' && x>='A') || (x<='z' && x>='a'));
}

bool isDigit(char x){
    return (x<='9' && x>='0');
}

bool isDelimiterFunc(char x){
    bool isDelimiter = false;
        for(int d=0;d<sizeof(delimiters)-1;d++){
            if(delimiters[d]==x){
                isDelimiter = true;
                break;
            }
        }
    return isDelimiter;
}

bool getToken(ifstream &fileStream,char* token){   
    int i=0;

    while(fileStream.get(token[i])){
        //cout<<"Char->"<<lastCharacterParsed<<endl;
        lastCharacterParsed = token[i];
        bool isDelimiter = isDelimiterFunc(token[i]);

        if(isDelimiter && i!=0){
            currentTokenLine = currentTokenLine + lineAfterLastToken;
            currentTokenOffset = (lineAfterLastToken>0)?charactersAfterLastTokenOrLine:charactersAfterLastTokenOrLine+currentTokenSize+currentTokenOffset;
            currentTokenSize = i;
            if(token[i]=='\n'){
                charactersInLastLineParsed = 0;
                lineAfterLastToken = 1;
                charactersAfterLastTokenOrLine = 0;
            }else{
                lineAfterLastToken = 0;
                charactersAfterLastTokenOrLine = 1;
            }
        }else if(isDelimiter){
            if(token[i]=='\n'){
                lineAfterLastToken ++;
                charactersInLastLineParsed = charactersAfterLastTokenOrLine;
                charactersAfterLastTokenOrLine = 0;
            }else{
                charactersAfterLastTokenOrLine ++;
            }
        }
        
        if(isDelimiter && i!=0){
            token[i] = '\0';
            //cout<<"Token:"<<token<<", Line:"<<currentTokenLine<<", Offset:"<<currentTokenOffset<<", Size:"<<currentTokenSize<<endl;
            return true;
        }else if(!isDelimiter){
            i++;
        }
    }
    if(i!=0){
        
        currentTokenLine = currentTokenLine + lineAfterLastToken;
        currentTokenOffset = (lineAfterLastToken>0)?charactersAfterLastTokenOrLine:charactersAfterLastTokenOrLine+currentTokenSize+currentTokenOffset;
        currentTokenSize = i;

        lineAfterLastToken = 0;
        charactersAfterLastTokenOrLine = 0;

        token[i] = '\0';
        //cout<<"ZZToken:"<<token<<", Line:"<<currentTokenLine<<", Offset:"<<currentTokenOffset<<", Size:"<<currentTokenSize<<endl;
        return true;
    }
    
    if(lastCharacterParsed=='\n'){
        lineAfterLastToken--;
        charactersAfterLastTokenOrLine = charactersInLastLineParsed;
    }
    
    //cout<<"LAST TOKEN SIZE:"<<currentTokenSize<<",Lines:"<<lineAfterLastToken<<", Chars:"<<charactersAfterLastTokenOrLine<<endl;
    
    
    
    token[i]='\0';
    return false;
}

int getNumberFromString(char* str,int len){
    if(len>8)
        return -1;
    for(int i=0;i<len;i++){
        if(!(str[i]<='9' && str[i]>='0'))
            return -1;
    }
    return atoi(str);
}

bool isValidSymbol(char* token){
    if( !isCharacter(token[0]) )
        return false;
    bool end = false;
    for(int i=1;i<=MAX_SYMBOL_SIZE;i++){
        if(token[i]=='\0'){
            return true;
        }
        if(!(isCharacter(token[i]) || isDigit(token[i]))){
            return false;
        }
    }
    return true;
}

bool isValidAddressing(char * token){
    if(token[1]=='\0' and (token[0]=='A' || token[0]=='E' || token[0]=='I' || token[0]=='R'))
        return true;
    return false;
}

void __parseerror(int linenum, int lineoffset, int errcode) {
    printf("Parse Error line %d offset %d: ", linenum, lineoffset);
    switch(errcode){
        case 0: cout<<"NUM_EXPECTED"<<endl; break; // 0 Number expect
        case 1: cout<<"SYM_EXPECTED"<<endl; break; // 1 Symbol Expected
        case 2: cout<<"ADDR_EXPECTED"<<endl; break; // 2 Addressing Expected which is A/E/I/R
        case 3: cout<<"SYM_TOO_LONG"<<endl; break; // 3 Symbol Name is too long
        case 4: cout<<"TOO_MANY_DEF_IN_MODULE"<<endl; break; // 4 > 16
        case 5: cout<<"TOO_MANY_USE_IN_MODULE"<<endl; break; // 5 > 16
        case 6: cout<<"TOO_MANY_INSTR"<<endl; break; // 6 total num_instr exceeds memory size (512)
        default: cout<<"!!!!!!!!!!!!!!!!!!!!_____UNKNOWN PARSE ERROR_____!!!!!!!!!!!!!!!!!!!!\n";
    }
    exit(0);
}
void __errors(int errcode, char* s1=NULL){
    switch(errcode){
        case 0: cout<<"Error: Absolute address exceeds machine size; zero used"<<endl; break; //7 (see rule 8)
        case 1: cout<<"Error: Relative address exceeds module size; zero used"<<endl; break; //8 (see rule 9)
        case 2: cout<<"Error: External address exceeds length of uselist; treated as immediate"<<endl; break; //9 (see rule 6)
        case 3: cout<<"Error: "<<s1<<" is not defined; zero used"<<endl; break; // 10 (insert the symbol name for %s) (see rule 3)
        case 4: cout<<"Error: This variable is multiple times defined; first value used"<<endl; break; //11 (see rule 2)
        case 5: cout<<"Error: Illegal immediate value; treated as 9999"<<endl; break; //12 (see rule 10)
        case 6: cout<<"Error: Illegal opcode; treated as 9999"<<endl; break; //13 (see rule 11)
        default: cout<<"!!!!!!!!!!!!!!!!!!!!_____UNKNOWN ERROR_____!!!!!!!!!!!!!!!!!!!!\n";
    }
}
void __warnings(int errcode, int num1,char* s1,int num2=0,int num3=0 ){
    switch(errcode){
        case 0: cout<<"Warning: Module "<<num1<<": "<<s1<<" too big "<<num2<<" (max="<<num3<<") assume zero relative"<<endl; break; //14 (see rule 5)
        case 1: cout<<"Warning: Module "<<num1<<": "<<s1<<" appeared in the uselist but was not actually used"<<endl; break; //15 (see rule 7)
        case 2: cout<<"Warning: Module "<<num1<<": "<<s1<<" was defined but never used"<<endl; break; //16 (see rule 4) 
        default: cout<<"!!!!!!!!!!!!!!!!!!!!_____UNKNOWN WARNING_____!!!!!!!!!!!!!!!!!!!!\n";
    }
}
bool checkAddressConsistency(int id,int size){
    for(int i=0;i<symbolCount;i++){
        if(symbolTable[i].module->id==id){
            if(symbolTable[i].relativeAddress>=size){
                __warnings(0,id+1,symbolTable[i].symbol,symbolTable[i].relativeAddress,size-1);
                symbolTable[i].relativeAddress = 0;
            }
        }
    }
    return true;
}

int findSymbolIndex(char* token){
    for(int i=0;i<symbolCount;i++){
        if(strcmp(token,symbolTable[i].symbol)==0){
            return i;
        }
    }
    return -1;
}

void handleEndWithNewlineCase(int &a){
    if(lastCharacterParsed=='\n' && (lineAfterLastToken==0 || charactersAfterLastTokenOrLine>0)){
                a=a+1;
    }
}

void moveLineAndOffsetToEndOfFile(){
    currentTokenLine = currentTokenLine + lineAfterLastToken;
    currentTokenOffset = (lineAfterLastToken>0)?charactersAfterLastTokenOrLine:charactersAfterLastTokenOrLine+currentTokenSize+currentTokenOffset;
    currentTokenOffset = (currentTokenOffset>0)?currentTokenOffset-1:currentTokenOffset;
    handleEndWithNewlineCase(currentTokenOffset);
}



void firstPass(){
    int memoryUsed = 0;
    int instructionCountSoFar=0;

    ifstream fileStream(path);
    char token[MAX_TOKEN_SIZE];
    while(!fileStream.eof()){
        if(!getToken(fileStream,token)){
            moveLineAndOffsetToEndOfFile();
            break;
        }
        // Get Deflist
        int lSymbolCount = getNumberFromString(token,currentTokenSize);
        // cout<<"\nSymbolCount:"<<lSymbolCount;
        if(lSymbolCount<0){
            __parseerror(currentTokenLine+1,currentTokenOffset+1,0);
        }
        else if(lSymbolCount>MAX_DEF_COUNT){
            __parseerror(currentTokenLine+1,currentTokenOffset+1,4);
        }
        
        for(int i=0;i<lSymbolCount;i++){
            char* newSymbol = new char[MAX_SYMBOL_SIZE];
            if(!getToken(fileStream,newSymbol)){
                moveLineAndOffsetToEndOfFile();
            }
            
            
            if(!isValidSymbol(newSymbol)){
                __parseerror(currentTokenLine+1,currentTokenOffset+1,1);
            }else if(currentTokenSize>MAX_SYMBOL_SIZE){
                __parseerror(currentTokenLine+1,currentTokenOffset+1,3);
            }else{
                int temp = findSymbolIndex(newSymbol);
                if(temp<0){
                    if(symbolCount>=MAX_SYMBOL_COUNT){
                        __parseerror(currentTokenLine+1,currentTokenOffset+1,4);
                    }
                    symbolTable[symbolCount].symbol = newSymbol;
                    symbolTable[symbolCount].numberOfTimesDefined ++;
                }else{
                    symbolTable[temp].numberOfTimesDefined ++;
                    if(!getToken(fileStream,token)){
                        moveLineAndOffsetToEndOfFile();
                    }
                    continue;
                }
            }
            
            if(!getToken(fileStream,token)){
                moveLineAndOffsetToEndOfFile();
            }
            int lSymbolRA = getNumberFromString(token,currentTokenSize);
            if(lSymbolRA<0){
                __parseerror(currentTokenLine+1,currentTokenOffset+1,0);
            }
            symbolTable[symbolCount].relativeAddress = lSymbolRA;
            symbolTable[symbolCount++].module = &moduleTable[moduleCount]; 
        }
        


        // Get Uselist
        if(!getToken(fileStream,token)){
            moveLineAndOffsetToEndOfFile();
        }
        int lUseListCount = getNumberFromString(token,currentTokenSize);
        // cout<<"\nUseCount:"<<lUseListCount;
        if(lUseListCount<0){
            __parseerror(currentTokenLine+1,currentTokenOffset+1,0);
        }
        else if(lUseListCount>MAX_USELIST_COUNT){
            __parseerror(currentTokenLine+1,currentTokenOffset+1,5);
        }
        for(int i=0;i<lUseListCount;i++){
            if(!getToken(fileStream,token)){
                moveLineAndOffsetToEndOfFile();
            }
            if(!isValidSymbol(token)){
                __parseerror(currentTokenLine+1,currentTokenOffset+1,1);
            }else if(currentTokenSize>MAX_SYMBOL_SIZE){
                __parseerror(currentTokenLine+1,currentTokenOffset+1,3);
            }
        }


        // Get Instructions
        if(!getToken(fileStream,token)){
            moveLineAndOffsetToEndOfFile();
        }
        int lModuleSize = getNumberFromString(token,currentTokenSize);
        // cout<<"\nInstruction Count:"<<lModuleSize;
        if(lModuleSize<0){
            __parseerror(currentTokenLine+1,currentTokenOffset+1,0);
        }
        else if(lModuleSize+memoryUsed>=MAX_SYSTEM_MEMORY){
            __parseerror(currentTokenLine+1,currentTokenOffset+1,6);
        }
        //cout<<"MODULE SIZE:"<<lModuleSize<<endl;
        for(int i=0;i<lModuleSize;i++){
    
            if(instructionCountSoFar>=MAX_SYSTEM_MEMORY){
                if(!getToken(fileStream,token)){
                    moveLineAndOffsetToEndOfFile();
                }
                __parseerror(currentTokenLine+1,currentTokenOffset+1,6);
            }

            if(!getToken(fileStream,token)){ //getting type
                moveLineAndOffsetToEndOfFile();
            }  
            if(!isValidAddressing(token)){
                //cout<<"TYPE:"<<token;
                __parseerror(currentTokenLine+1,currentTokenOffset+1,2);
            }

            if(!getToken(fileStream,token)){ //getting instruction
                moveLineAndOffsetToEndOfFile();
            }   
            
            // cout<<"\nINTRUCTION:"<<token;
            int instruction = getNumberFromString(token,currentTokenSize);
            if(instruction<0){
                __parseerror(currentTokenLine+1,currentTokenOffset+1,0);
            }
            
            instructionCountSoFar ++;
        }
        moduleTable[moduleCount].baseAddress = memoryUsed;
        moduleTable[moduleCount].id = moduleCount;
        moduleTable[moduleCount++].moduleSize = lModuleSize;
        
        memoryUsed += lModuleSize;
        checkAddressConsistency(moduleCount-1,lModuleSize);
    }
}

void resolveSymbols(){
    //cout<<"Print from resolveSymboles():\n";
    for(int i=0;i<symbolCount;i++){
        symbolTable[i].absoluteAddress = symbolTable[i].module->baseAddress + symbolTable[i].relativeAddress;
        //cout<<"\nSymbol:"<<symbolTable[i].symbol<<", ModuleBA:"<<symbolTable[i].module->baseAddress<<", Relative Address:"<<symbolTable[i].relativeAddress;
    }
}

void outputSymbolTable(){
   cout<<"Symbol Table\n";
    for(int i=0;i<symbolCount;i++){
        if(symbolTable[i].numberOfTimesDefined>1){
            cout<<symbolTable[i].symbol<<"="<<symbolTable[i].absoluteAddress<<" ";
            __errors(4);
        }else{
            cout<<symbolTable[i].symbol<<"="<<symbolTable[i].absoluteAddress<<"\n";
        }
    }
    cout<<"\nMemory Map\n";
}

stringSize3 getSize3String(int loc){
    stringSize3 ans;
    for(int i=0;i<3;i++){
        ans.val[2-i] = 48+loc%10;
        loc /= 10;
    }
    ans.val[3]='\0';
    return ans;
}

void secondPass(){
    
    ifstream fileStream(path);
    char token[MAX_TOKEN_SIZE];
    int memoryAddress = 0;
    int moduleInProcess = 0;
    while(!fileStream.eof()){
       if(!getToken(fileStream,token))
            break;
        // Get Deflist
        int lSymbolCount = atoi(token);
        for(int i=0;i<lSymbolCount;i++){
            char* newSymbol = new char[MAX_SYMBOL_SIZE];
            getToken(fileStream,newSymbol);
            getToken(fileStream,token); 
        }
        
        // Get Uselist
        getToken(fileStream,token);
        int useList[MAX_USELIST_COUNT];
        bool useListUsed[MAX_USELIST_COUNT];
        int lUseListCount = atoi(token);
        char useListSymbol[MAX_USELIST_COUNT][MAX_SYMBOL_SIZE];
        for(int i=0;i<lUseListCount;i++){
            getToken(fileStream,token);
            useList[i] = findSymbolIndex(token);
            // if(useList[i]>=0){
            //     symbolTable[useList[i]].used = true;
            // }
            useListUsed[i] = false;
            strcpy(useListSymbol[i],token);    
        }

        // Get Instructions
        getToken(fileStream,token);
        int lModuleSize = atoi(token);
        for(int i=0;i<lModuleSize;i++){
            getToken(fileStream,token);  //getting type
            char type = token[0];
            getToken(fileStream,token); //getting instruction
            int instruction = atoi(token);
            //cout<<"Instruction:"<<instruction<<endl;
            int opcode = instruction/1000;
            int operand = instruction%1000;
            
            cout<<getSize3String(memoryAddress).val<<": ";
            switch(type){
                case 'I':    
                    if(instruction>=10000){
                        cout<<9999<<" ";
                        __errors(5);
                    }else{
                        cout<<opcode<<getSize3String(operand).val<<endl;
                    }break;
                case 'A': 
                    if(opcode>=10){
                        cout<<9999<<" ";
                        __errors(6);
                    }else if(operand>=MAX_SYSTEM_MEMORY){
                        cout<<opcode<<"000"<<" ";
                        __errors(0);
                    }else{
                        cout<<opcode<<getSize3String(operand).val<<endl;
                    }break;
                case 'R': 
                    if(opcode>=10){
                        cout<<9999<<" ";
                        __errors(6);
                    }else if(operand>=moduleTable[moduleInProcess].moduleSize){
                        operand = 0;
                        cout<<opcode<<getSize3String(operand+moduleTable[moduleInProcess].baseAddress).val<<" ";
                        __errors(1);    
                    }else{
                        cout<<opcode<<getSize3String(operand+moduleTable[moduleInProcess].baseAddress).val<<endl; 
                    }break;
                case 'E':
                    if(opcode>=10){
                        cout<<9999<<" ";
                        __errors(6);
                    }else if(operand>=lUseListCount){
                        cout<<opcode<<getSize3String(instruction).val<<" ";
                        __errors(2);        
                    }else if(useList[operand]<0){
                        int origOperand = operand;
                        operand=0;
                        cout<<opcode<<getSize3String(symbolTable[useList[operand]].absoluteAddress).val<<" ";
                        useListUsed[origOperand] = true;
                        if(useList[origOperand]>=0){
                            symbolTable[useList[origOperand]].used = true;
                        }
                        __errors(3,useListSymbol[origOperand]);
                        
                    }else{
                        cout<<opcode<<getSize3String(symbolTable[useList[operand]].absoluteAddress).val<<endl;
                        useListUsed[operand] = true;
                        if(useList[operand]>=0){
                            symbolTable[useList[operand]].used = true;
                        }
                    }
                   
                    break;
                default: cout<<"------ERROR--------";
            }
            memoryAddress++;
            
        }
        for(int i=0;i<lUseListCount;i++){
            if(!useListUsed[i])
                __warnings(1,moduleInProcess+1,useListSymbol[i]);
        }

        moduleInProcess ++;
    }
    cout<<endl;

    for(int i=0;i<symbolCount;i++){
        if(!symbolTable[i].used){
            __warnings(2,symbolTable[i].module->id+1,symbolTable[i].symbol);
        }
    }
    cout<<endl;
}

int main(int argc, char** argv){
    if(argv[1]=='\0'){
        cout<<"Object file path required in arguments!!\n"; 
        exit(99);
    }
    strcpy(path,argv[1]);
    firstPass();
    resolveSymbols();
    outputSymbolTable();
    secondPass();
}


