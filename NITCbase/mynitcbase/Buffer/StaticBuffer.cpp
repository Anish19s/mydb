#include "StaticBuffer.h"
#include "../Disk_Class/Disk.h"

// declare the blockAllocMap array
unsigned char StaticBuffer::blockAllocMap[DISK_BLOCKS];

unsigned char StaticBuffer::blocks[BUFFER_CAPACITY][BLOCK_SIZE];
struct BufferMetaInfo StaticBuffer::metainfo[BUFFER_CAPACITY];


StaticBuffer::StaticBuffer() {

    // ----------------------------------------------------
    // Copy block allocation map from disk into buffer
    // Block allocation map occupies disk blocks 0 to 3
    // ----------------------------------------------------

    for (int i = 0; i < 4; i++) {
        Disk::readBlock(blocks[i], i);
    }

    // Copy the 4 blocks into blockAllocMap
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < BLOCK_SIZE; j++) {
            blockAllocMap[i * BLOCK_SIZE + j] = blocks[i][j];
        }
    }


    // ----------------------------------------------------
    // Initialise metadata of all buffer blocks
    // ----------------------------------------------------

    for (int bufferIndex = 0;
         bufferIndex < BUFFER_CAPACITY;
         bufferIndex++) {

        metainfo[bufferIndex].free = true;
        metainfo[bufferIndex].dirty = false;
        metainfo[bufferIndex].timeStamp = -1;
        metainfo[bufferIndex].blockNum = -1;
    }
}


StaticBuffer::~StaticBuffer() {

    // ----------------------------------------------------
    // Copy block allocation map back into buffer
    // ----------------------------------------------------

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < BLOCK_SIZE; j++) {
            blocks[i][j] =
                blockAllocMap[i * BLOCK_SIZE + j];
        }
    }


    // ----------------------------------------------------
    // Write block allocation map blocks back to disk
    // ----------------------------------------------------

    for (int i = 0; i < 4; i++) {
        Disk::writeBlock(blocks[i], i);
    }


    // ----------------------------------------------------
    // Write back all dirty blocks currently in buffer
    // ----------------------------------------------------

    for (int i = 0; i < BUFFER_CAPACITY; i++) {

        if (!metainfo[i].free && metainfo[i].dirty) {

            Disk::writeBlock(
                blocks[i],
                metainfo[i].blockNum
            );
        }
    }
}


/*
   Get the buffer index where a particular block is stored
   or E_BLOCKNOTINBUFFER otherwise
*/
int StaticBuffer::getBufferNum(int blockNum) {

    // Check if blockNum is valid
    if (blockNum < 0 || blockNum >= DISK_BLOCKS) {
        return E_OUTOFBOUND;
    }

    // Search for the block in the buffer
    for (int bufferIndex = 0;
         bufferIndex < BUFFER_CAPACITY;
         bufferIndex++) {

        if (!metainfo[bufferIndex].free &&
            metainfo[bufferIndex].blockNum == blockNum) {

            return bufferIndex;
        }
    }

    return E_BLOCKNOTINBUFFER;
}


int StaticBuffer::getFreeBuffer(int blockNum) {

    // Check if block number is valid
    if (blockNum < 0 || blockNum >= DISK_BLOCKS) {
        return E_OUTOFBOUND;
    }


    // ----------------------------------------------------
    // Increase timestamp of all occupied buffers
    // ----------------------------------------------------

    for (int i = 0; i < BUFFER_CAPACITY; i++) {

        if (!metainfo[i].free) {
            metainfo[i].timeStamp++;
        }
    }


    int bufferNum = -1;


    // ----------------------------------------------------
    // Look for a free buffer
    // ----------------------------------------------------

    for (int i = 0; i < BUFFER_CAPACITY; i++) {

        if (metainfo[i].free) {
            bufferNum = i;
            break;
        }
    }


    // ----------------------------------------------------
    // If no free buffer exists,
    // find the buffer with largest timestamp
    // ----------------------------------------------------

    if (bufferNum == -1) {

        int maxTimeStamp = -1;

        for (int i = 0; i < BUFFER_CAPACITY; i++) {

            if (metainfo[i].timeStamp > maxTimeStamp) {

                maxTimeStamp = metainfo[i].timeStamp;
                bufferNum = i;
            }
        }


        // ------------------------------------------------
        // If selected buffer is dirty,
        // write it back to disk before reusing it
        // ------------------------------------------------

        if (metainfo[bufferNum].dirty) {

            int retVal = Disk::writeBlock(
                blocks[bufferNum],
                metainfo[bufferNum].blockNum
            );

            if (retVal != SUCCESS) {
                return retVal;
            }
        }
    }


    // ----------------------------------------------------
    // Update metadata for the newly allocated buffer
    // ----------------------------------------------------

    metainfo[bufferNum].free = false;
    metainfo[bufferNum].dirty = false;
    metainfo[bufferNum].blockNum = blockNum;
    metainfo[bufferNum].timeStamp = 0;


    return bufferNum;
}


int StaticBuffer::setDirtyBit(int blockNum) {

    int bufferNum = getBufferNum(blockNum);


    // Block number out of bounds
    if (bufferNum == E_OUTOFBOUND) {
        return E_OUTOFBOUND;
    }


    // Block is not currently in buffer
    if (bufferNum == E_BLOCKNOTINBUFFER) {
        return E_BLOCKNOTINBUFFER;
    }


    // Mark the buffer as dirty
    metainfo[bufferNum].dirty = true;


    return SUCCESS;
}