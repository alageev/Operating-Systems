//
//  main.c
//  OSLab1
//
//  Created by Алексей Агеев on 08.10.2020.
//

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>
#include <sys/file.h>

/*
 A = 146 * 1024 * 1024;
 B = (void *)0x3C3EE19;
 C = mmap;
 D = 13;
 E = 138;
 F = nocache;
 G = 67;
 H = random;
 I = 38;
 J = avg;
 K = flock
 */

typedef struct {
    int  fileDescriptor;
    int* address;
    int  length;
} FillMemoryArguments;

typedef struct {
    char *start;
    size_t size;
    int file;
    int upperBound;
} WriteToFileArguments;

typedef struct {
    int file;
    int *sum;
    int *count;
    off_t size;
} ReadFileArguments;


void allocateMemory(void*, int, int);
void initialFillingOfMemory(int, void*, int, int);
void* fillMemory (void*);
void* writeFile (void*);
void* readFile(void*);

//pthread_mutex_t mallocMutex = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, const char * argv[]) {
    const size_t A = 146 * 1024 * 1024;
    void* B = (void *)0x3C3EE19;
    const int D = 13;
    const int E = 138 * 1024 * 1024;
    const int G = 67;
    const int I = 38;
    
    //    void* startAdress = B;
    int fillLength;
    
    if (A % D == 0) {
        fillLength = A / D;
    } else {
        fillLength = A / D + 1;
    }
    
    
    int outputFileDescriptor = open("output.txt", O_RDWR | O_APPEND | O_CREAT, 0677);
    
    printf("Press enter to allocate memory.");
    getchar();
    
    allocateMemory(B, A, outputFileDescriptor);
    
    printf("Memory was allocated. Press enter to fill memory.");
    getchar();
    
    initialFillingOfMemory(D, B, fillLength, A);
    
    printf("Memory was filled. Press enter to deallocate memory.");
    getchar();
    
    munmap(B - (int)B % getpagesize(),    //addr: starting adress of mapping
           A);   //length: length of mapping
    
    printf("Memory was deallocated. Press enter for the second part");
    getchar();
    
    allocateMemory(B, A, outputFileDescriptor);
    initialFillingOfMemory(D, B, fillLength, A);
    
    unsigned numberOfFiles;
    
    if (A % E == 0) {
        numberOfFiles = A / E;
    } else {
        numberOfFiles = A / E + 1;
    }
    
    int descriptors[numberOfFiles];
    for (int file = 0; file < numberOfFiles; file += 1) {
        char filename[7];
        sprintf(filename, "%i-file", file);
        descriptors[file] = open(filename, O_RDWR | O_CREAT | O_TRUNC /*| O_DIRECT*/, 0777);
    }
    
    for (;;) {
        int count = 0;
        int sum = 0;
        pthread_t writeThreads[I];
        WriteToFileArguments writeToFileArguments[I];
        ReadFileArguments readFileArguments[I];
        
        for (int threadNumber = 0; threadNumber < I; threadNumber += 1) {
            int fileId = threadNumber % numberOfFiles;
            
            writeToFileArguments[threadNumber].start = B;
            writeToFileArguments[threadNumber].size = G;
            writeToFileArguments[threadNumber].file = descriptors[fileId];
            writeToFileArguments[threadNumber].upperBound = fillLength;
            
            readFileArguments[threadNumber].sum = &sum;
            readFileArguments[threadNumber].count = &count;
            readFileArguments[threadNumber].file = descriptors[fileId];
            readFileArguments[threadNumber].size = fillLength;
            pthread_create(&writeThreads[threadNumber],
                           NULL,
                           writeFile,
                           &writeToFileArguments[threadNumber]);
        }
        
        pthread_t readThreads[I];
        
        for (int threadNumber = 0; threadNumber < I; threadNumber += 1) {
            pthread_join(writeThreads[threadNumber], NULL);
        }
        
        for (int threadNumber = 0; threadNumber < I; threadNumber += 1) {
            pthread_create(&readThreads[threadNumber],
                           NULL,
                           readFile,
                           &readFileArguments[threadNumber]);
        }
        
        for (int threadNumber = 0; threadNumber < I; threadNumber += 1) {
            pthread_join(readThreads[threadNumber], NULL);
        }
        
        printf("Current sum = %i, count = %i, avg = %f\n", sum, count, (double)sum / (double)count);
    }
    
    
    return 0;
}

void allocateMemory(void* address, int length, int outputFileDescriptor) {
    mmap(address,                   // addr: starting adress of mapping
         length,                    // length: length of mapping
         PROT_READ | PROT_WRITE,    // prot: protection flags
         MAP_PRIVATE,               // flags: visibility flags
         outputFileDescriptor,      // fd: file descriptor
         0);                        // offset: offset in a file
}

void initialFillingOfMemory(int numberOfThreads, void* address, int fillLength, int length) {
    pthread_t fillThreads[numberOfThreads];
    int randomFileDescriptor = open("/dev/urandom", O_RDONLY);
    
    for (int threadNumber = 0; threadNumber < numberOfThreads; threadNumber += 1) {
        FillMemoryArguments arguments = {
            .fileDescriptor = randomFileDescriptor,
            .address = address + fillLength * threadNumber,
            .length = threadNumber == numberOfThreads - 1 ? fillLength : length - (numberOfThreads - 1) * threadNumber
        };
        pthread_create(&fillThreads[threadNumber],
                       NULL,
                       fillMemory,
                       &arguments);
    }
    for (int threadNumber = 0; threadNumber < numberOfThreads; threadNumber += 1) {
        pthread_join(fillThreads[threadNumber], NULL);
    }
}

void* fillMemory (void* arguments) {
    FillMemoryArguments *data = (FillMemoryArguments*) arguments;
    read(data->fileDescriptor, data->address, data->length);
    pthread_exit(NULL);
}

void* writeFile(void* arguments) {
    WriteToFileArguments *data = (WriteToFileArguments *) arguments;
    size_t result;
    char* random = data->start + rand() % data->upperBound;// - data->size;
    
    flock(data->file, LOCK_EX);
    lseek(data->file, data->size, SEEK_SET);
    result = write(data->file, &random, data->size);
    flock(data->file, LOCK_UN);
    
    if (result == -1) {
        printf("Write thread failed, error: %s\n", strerror(errno));
    }
    pthread_exit(NULL);
}

void* readFile(void* arg) {
    ReadFileArguments *data = (ReadFileArguments *) arg;
    char* buf = (char *) malloc(data->size);
    if (buf == NULL) {
        printf("Could not allocate read buffer\n");
        pthread_exit(NULL);
    }
    
    flock(data->file, LOCK_EX);
    lseek(data->file, 0, SEEK_SET);
    size_t result = read(data->file, buf, data->size);
    flock(data->file, LOCK_UN);
    
    if (result == -1) {
        printf("Read thread failed, error: %s\n", strerror(errno));
        free(buf);
        pthread_exit(NULL);
    }
    
    for (int i = 0; i < data->size; i++) {
        *(data->sum) += (int) buf[i];
    }
    *(data->count) += data->size;
    
    free(buf);
    
    pthread_exit(NULL);
}
