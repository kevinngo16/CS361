#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/mman.h>
#include <stdint.h>

#define bool int
#define true 1
#define false 0
//=============================
long FIFOHitCount     = 0;//global counters
long FIFOMissCount    = 0;
long UnlimitHitCount  = 0;
long UnlimitMissCount = 0;
//=============================

typedef struct{
        uint8_t pid;
        uint8_t page;
}MemoryAccess;

typedef struct{
        int frame; // -1 if empty
        int UMHits;//for umlimited memory. 0 for first time loading into frame. 1 for already loaded
        int pageHits;
        int pageMisses;
}PageTableEntry;

typedef struct{
        uint8_t pid;
        uint8_t page;
        bool vacant;//0 if empty, 1 if loaded
}FrameTableEntry;

void usage(char * errorName){//for printing out error messages
        printf("ERROR: %s\n", errorName);
        printf("File does not exist, file name not included, or a negative value has been entered as an arguement\n")                                                                                                                        ;
        return;
}
//===============================================================================================================
int main(int argc, char *argv[]){
        printf("\n\nProgram by Kevin Ngo, kngo6, ACCC name: temp30, UIN:650421455\nVMPager Homework (3)\n");
        char *fileStart = NULL;  // char * can access any address, byte by byte
        int fd, filesize, i, j;
        int frameTableSize = 0  ;
        int memoryAccesses = 0  ;
        int pageTableSize  = 256;
        int nProcesses     = 256;
        struct stat statBuffer;
//====================================================
        // First to process the command line arguments

        if( argc < 2) {//if the user hasn't enter the file name
                usage( argv[ 0 ] );//or if we have too many arguements
                exit( -1 );
        }
        else if( argc < 3){//if no memoryAccesses and frameTableSize has been entered
                memoryAccesses = 0;
                frameTableSize = 256;
        }
        else if( argc < 4) {//if the user enters a memoryAccesses size
                memoryAccesses = atoi( argv[ 2 ] );
                if( memoryAccesses < 0 ) {
                        usage( argv[ 2 ] );
                        exit( -2 );
                }
                else if(memoryAccesses == 0)
                        memoryAccesses = 0;
                frameTableSize = 256;//setting framTableSize as default = 256;
        }

        else if( argc < 5){//if the user enters a memoryAccesses and frameTableSize
                memoryAccesses = atoi(argv[2]);
                frameTableSize = atoi(argv[3]);
                if(frameTableSize < 0){
                        usage(argv[3]);
                        exit(-2);
                }
                else if(frameTableSize == 0)//set the default size
                        frameTableSize = 256;
                if(memoryAccesses < 0){
                        usage(argv[3]);
                        exit(-2);
                }
                else if(memoryAccesses == 0)
                        memoryAccesses = 0;
        }

        else{//if the user doesn't enter a size to begin with
                memoryAccesses = 0;
                frameTableSize = 256;
        }
//=====================================================

        //Initializing tables
        PageTableEntry    pageTable[pageTableSize][nProcesses];
        MemoryAccess    * mAccess    = NULL;  // This will be treated as an array of mAccess to go through the image/file
        FrameTableEntry * frameTable = NULL;

//======================================================
        //opening file
        if( ( fd = open( argv[1] , O_RDWR)) < 0 ) {
                printf("%s\n", "Errer on opening file");
                exit( -4 );
        }
        //get file stats such as size
        if( fstat( fd, &statBuffer ) < 0 ) {
                printf("%s\n", "fstat");
                exit( -5 );
        }


        filesize = statBuffer.st_size; // size of file in bytes
        printf("\nFile size: %d bytes\n", filesize);
        int fileBlockSize = statBuffer.st_blocks;
        printf("File used %d blocks in memory\n", fileBlockSize);
//======================================================

        //allocating memory for mAccess to get information from file
        if ( ( mAccess = ( MemoryAccess * ) mmap( 0,statBuffer.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0))==                                                                                                                         MAP_FAILED ) {
                printf("%s\n", "mmap");
                exit( -6 );
        }



//======================================================

        //setting everything up before processing
        //malloc-ing frametable to have size of frameTableSize entries
        frameTable = (FrameTableEntry *)malloc(sizeof(FrameTableEntry)*frameTableSize);

        for(i=0; i<pageTableSize;i++){
                for(j=0; j<nProcesses; j++){
                        pageTable[i][j].frame      = -1;
                        pageTable[i][j].pageHits   =  0;
                        pageTable[i][j].pageMisses =  0;
                        pageTable[i][j].UMHits     =  0;
                }
        }

        for(i=0; i<frameTableSize; i++){
                frameTable[i].pid    = 0;
                frameTable[i].page   = 0;
                frameTable[i].vacant = 0;
        }



//======================================================

        printf("%s %s %s %i %s\n", "\nProcessing " , argv[ 1 ] , " with " , frameTableSize, " frameTableSize(bytes).\n");

        int upperLimit;
        int entryPoint=0;//entryPoint = where in Frame table to load pid and page. We'll start at
                        //the beginning of the array and if entryPoint is the size of frameTable array
                        //then we should reset entryPoint.


        //Setting upper bound of array of mAccess
        if(memoryAccesses == 0 || memoryAccesses > filesize)
                upperLimit = filesize/sizeof(MemoryAccess);
        else
                upperLimit = memoryAccesses;

        //processing the file
        for(i=0;i<upperLimit;i++){
                //calculate where to look at for the pid and page
                //if we get a page hit
                if(pageTable[mAccess[i].page][mAccess[i].pid].frame == 1){
                        pageTable[mAccess[i].page][mAccess[i].pid].pageHits++;
                        FIFOHitCount++;
                        UnlimitHitCount++;
                }
                //if we get a page miss
                else{
                        FIFOMissCount++;
                        if(pageTable[mAccess[i].page][mAccess[i].pid].UMHits == 1)
                                UnlimitHitCount++;
                        if(pageTable[mAccess[i].page][mAccess[i].pid].UMHits == 0){
                                pageTable[mAccess[i].page][mAccess[i].pid].UMHits = 1;
                                UnlimitMissCount++;
                        }
                        if(entryPoint == frameTableSize){//if we're pointing to the end of frame array,
                                                        //we point to the beginning of frame array
                                entryPoint =0;
                        }
                        //check if entryPoint is the same as frameTable size
                        if(frameTable[entryPoint].vacant == -1){//if the frame was loaded in for the first time
                                pageTable[mAccess[i].page][mAccess[i].pid].frame = 1;
                                pageTable[mAccess[i].page][mAccess[i].pid].pageMisses++;
                        }
                        else{//if the frame is being updated
                                //removing page from frameTable
                                pageTable[ frameTable[entryPoint].page ][ frameTable[entryPoint ].pid].frame = -1;
                                pageTable[       mAccess[i].page       ][        mAccess[i].pid      ].frame = 1;
                                frameTable[entryPoint].pid    = mAccess[i].pid;
                                frameTable[entryPoint].page   = mAccess[i].page;
                                frameTable[entryPoint].vacant = 1;
                                entryPoint++;
                        }
                }
        }
        //print out the results
        printf("upperLimit of FrameTable:%d\n\n",upperLimit);
        //will print out the arguements the user has enter
        printf("File name: %s\n", argv[1]);
        printf("Number of memoryAccess: %d",memoryAccesses);
        if(memoryAccesses == 0)
                printf(", will process the whole file\n");
        else
                printf("\n");
        printf("Size of frametable: %d\n\n", frameTableSize);
        //print out the results of processing the file
        printf("Unlimit Memory:\nPage Hits: %d\nPage Misses: %d\n\n",UnlimitHitCount, UnlimitMissCount);
        printf("FIFO Simulation:\nPage Hits: %d\nPage Misses:%d\n\n\n", FIFOHitCount, FIFOMissCount);

        float UnlimVsFIFOHit = (float) UnlimitHitCount/FIFOHitCount   *100.0;
        float UnlimVsFIFOMiss= (float) UnlimitMissCount/FIFOMissCount *100.0;
        printf("Infinite memory has %.2f%% more page hits and %.2f%% more page misses than FIFO\n\n",UnlimVsFIFOHit-100, UnlimVsFIFOMiss-100);

        // Okay, we are done processing.  Now to close everything up.
        munmap( 0,statBuffer.st_size );
        close( fd );
        free(frameTable);
        return EXIT_SUCCESS;

}
