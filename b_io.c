/**************************************************************
* Class::  CSC-415-01 Summer 2024
* Name:: Yuvraj Gupta
* Student ID:: 922933190
* GitHub-Name:: YuvrajGupta1808
* Project:: Assignment 5 – Buffered I/O read
*
* File:: b_io.c
*
* Description:: Implementation of buffered I/O functions for 
* opening, reading, and closing files using custom buffers.
**************************************************************/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include "b_io.h"
#include "fsLowSmall.h"

#define MAXFCBS 20    // The maximum number of files open at one time
#define B_CHUNK_SIZE 512  // Define the chunk size for buffering

typedef struct b_fcb {
    fileInfo *fi;        // holds the low-level system's file info
    char *buffer;        // buffer to hold file data
    int buffer_index;    // index to track the current position in the buffer
    int buffer_length;   // length of valid data in the buffer
    int file_index;      // index to track the current position in the file
} b_fcb;

// static array of file control blocks
b_fcb fcbArray[MAXFCBS];

// Indicates that the file control block array has not been initialized
int startup = 0;

void b_init() {
    if (startup) return;  // already initialized

    // Initialize fcbArray to all free
    for (int i = 0; i < MAXFCBS; i++) {
        fcbArray[i].fi = NULL; // indicates a free fcbArray
    }

    startup = 1;
}

// Method to get a free File Control Block FCB element
b_io_fd b_getFCB() {
    for (int i = 0; i < MAXFCBS; i++) {
        if (fcbArray[i].fi == NULL) {
            fcbArray[i].fi = (fileInfo *)-2; // used but not assigned
            return i;  // Not thread safe but okay for this project
        }
    }
    return (-1);  // all in use
}

b_io_fd b_open(char *filename, int flags) {
    if (startup == 0) b_init();  // Initialize our system

    int fd = b_getFCB();
    if (fd == -1) return -1; // no available FCB

    fileInfo *fi = GetFileInfo(filename);
    if (fi == NULL) return -1; // file not found

    fcbArray[fd].fi = fi;
    fcbArray[fd].buffer = (char *)malloc(B_CHUNK_SIZE); // allocate buffer
    if (fcbArray[fd].buffer == NULL) return -1; // malloc failed

    fcbArray[fd].buffer_index = 0;
    fcbArray[fd].buffer_length = 0;
    fcbArray[fd].file_index = 0;

    return fd;
}

int b_read(b_io_fd fd, char *buffer, int count) {
    if (startup == 0) b_init();  // Initialize our system

    // Check that fd is between 0 and (MAXFCBS-1)
    if ((fd < 0) || (fd >= MAXFCBS)) {
        return -1;  // invalid file descriptor
    }

    // Check that the specified FCB is actually in use  
    if (fcbArray[fd].fi == NULL || fcbArray[fd].buffer == NULL) {  // File not open for this descriptor
        return -1;
    }

    int bytes_read = 0;
    while (bytes_read < count) {
        if (fcbArray[fd].buffer_index >= fcbArray[fd].buffer_length) {
            // Buffer is empty, read next chunk from file
            int bytes_to_read = B_CHUNK_SIZE;
            if (fcbArray[fd].file_index + bytes_to_read > fcbArray[fd].fi->fileSize) {
                bytes_to_read = fcbArray[fd].fi->fileSize - fcbArray[fd].file_index;
            }
            if (bytes_to_read <= 0) break; // End of file

            int block_number = fcbArray[fd].fi->location + (fcbArray[fd].file_index / B_CHUNK_SIZE);
            int blocks_read = LBAread(fcbArray[fd].buffer, (bytes_to_read + 511) / 512, block_number);
            if (blocks_read <= 0) {
                return -1;
            }
            fcbArray[fd].buffer_length = bytes_to_read;
            fcbArray[fd].buffer_index = 0;
            fcbArray[fd].file_index += bytes_to_read;
        }

        int bytes_available = fcbArray[fd].buffer_length - fcbArray[fd].buffer_index;
        int bytes_to_copy = count - bytes_read;
        if (bytes_to_copy > bytes_available) {
            bytes_to_copy = bytes_available;
        }

        memcpy(buffer + bytes_read, fcbArray[fd].buffer + fcbArray[fd].buffer_index, bytes_to_copy);
        fcbArray[fd].buffer_index += bytes_to_copy;
        bytes_read += bytes_to_copy;
    }

    return bytes_read;
}

int b_close(b_io_fd fd) {
    if ((fd < 0) || (fd >= MAXFCBS)) {
        return -1;  // invalid file descriptor
    }

    if (fcbArray[fd].fi == NULL)  // File not open for this descriptor
    {
        return -1;
    }

    // Release resources
    free(fcbArray[fd].buffer);
    fcbArray[fd].fi = NULL;
    fcbArray[fd].buffer = NULL;
    fcbArray[fd].buffer_index = 0;
    fcbArray[fd].buffer_length = 0;
    fcbArray[fd].file_index = 0;

    return 0;
}
