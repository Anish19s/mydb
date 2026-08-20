#include "StaticBuffer.h"
#include "StaticBuffer.h"
#include "../Disk_Class/Disk.h"
// the declarations for this class can be found at "StaticBuffer.h"

unsigned char StaticBuffer::blocks[BUFFER_CAPACITY][BLOCK_SIZE];
struct BufferMetaInfo StaticBuffer::metainfo[BUFFER_CAPACITY];

StaticBuffer::StaticBuffer() {

  // initialise all blocks as free
  for (int bufferIndex = 0;bufferIndex<BUFFER_CAPACITY;bufferIndex++) {
    metainfo[bufferIndex].free = true;
    metainfo[bufferIndex].dirty = false;
    metainfo[bufferIndex].timeStamp = -1;
    metainfo[bufferIndex].blockNum = -1;
  }
}

StaticBuffer::~StaticBuffer() {

    for (int i = 0; i < BUFFER_CAPACITY; i++) {

        if (!metainfo[i].free && metainfo[i].dirty) {

            Disk::writeBlock(
                blocks[i],
                metainfo[i].blockNum
            );
        }
    }
}
/* Get the buffer index where a particular block is stored
   or E_BLOCKNOTINBUFFER otherwise
*/
int StaticBuffer::getBufferNum(int blockNum) {
  // Check if blockNum is valid (between zero and DISK_BLOCKS)
  // and return E_OUTOFBOUND if not valid.
  if (blockNum < 0 || blockNum >=DISK_BLOCKS) {
    return E_OUTOFBOUND;
  }
  // find and return the bufferIndex which corresponds to blockNum (check metainfo)
   for (int bufferIndex = 0;bufferIndex<BUFFER_CAPACITY;bufferIndex++) {
    if(!metainfo[bufferIndex].free && metainfo[bufferIndex].blockNum == blockNum){
        metainfo[bufferIndex].free = false;
        metainfo[bufferIndex].blockNum = blockNum;
        return bufferIndex;
    }
  }
  // if block is not in the buffer
  return E_BLOCKNOTINBUFFER;
}
int StaticBuffer::getFreeBuffer(int blockNum) {

    // Check if block number is valid
    if (blockNum <= 0 || blockNum >= DISK_BLOCKS) {
        return E_OUTOFBOUND;
    }

    // Increase timestamp of all occupied buffers
    for (int i = 0; i < BUFFER_CAPACITY; i++) {
        if (metainfo[i].free == false) {
            metainfo[i].timeStamp++;
        }
    }

    int bufferNum = -1;

    // Look for a free buffer
    for (int i = 0; i < BUFFER_CAPACITY; i++) {

        if (metainfo[i].free == true) {
            bufferNum = i;
            break;
        }
    }

    // No free buffer -> find buffer with largest timestamp
    if (bufferNum == -1) {

        int maxTimeStamp = -1;

        for (int i = 0; i < BUFFER_CAPACITY; i++) {

            if (metainfo[i].timeStamp > maxTimeStamp) {
                maxTimeStamp = metainfo[i].timeStamp;
                bufferNum = i;
            }
        }

        // If selected buffer is dirty, write it back to disk
        if (metainfo[bufferNum].dirty == true) {

            int retVal = Disk::writeBlock(
                blocks[bufferNum],
                metainfo[bufferNum].blockNum
            );

            if (retVal != SUCCESS) {
                return retVal;
            }
        }
    }

    // Update metadata
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

    // Mark buffer as dirty
    metainfo[bufferNum].dirty = true;

    return SUCCESS;
}