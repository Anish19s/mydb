#include "BlockBuffer.h"
#include <cstring>
#include <cstdio>

// Constructor
BlockBuffer::BlockBuffer(int blockNum) {
    this->blockNum = blockNum;
}

// Calls the parent class constructor
RecBuffer::RecBuffer(int blockNum)
    : BlockBuffer(blockNum) {}

// Load the block header into the argument pointer
int BlockBuffer::getHeader(struct HeadInfo *head) {

    unsigned char* bufferptr;
    int ret=loadBlockAndGetBufferPtr(&bufferptr);
    
    if(ret!=SUCCESS){
        return ret;
    }


    // Populate the header fields
    memcpy(&head->blockType, bufferptr + 0, 4);
    memcpy(&head->pblock,    bufferptr + 4, 4);
    memcpy(&head->lblock,    bufferptr + 8, 4);
    memcpy(&head->rblock,    bufferptr + 12, 4);
    memcpy(&head->numEntries, bufferptr + 16, 4);
    memcpy(&head->numAttrs,   bufferptr + 20, 4);
    memcpy(&head->numSlots,   bufferptr + 24, 4);
    memcpy(head->reserved,    bufferptr + 28, 4);

    return SUCCESS;
}

// Load the record at slotNum into the argument pointer
int RecBuffer::getRecord(union Attribute *rec, int slotNum) {
    
    HeadInfo head;

    int ret = getHeader(&head);
    if (ret != SUCCESS)
        return ret;
    // Read the header

    int attrCount = head.numAttrs;
    int slotCount = head.numSlots;

    unsigned char* bufferptr;
    ret=loadBlockAndGetBufferPtr(&bufferptr);
    
    if(ret!=SUCCESS){
        return ret;
    }

    /*
        Record layout:

        +---------------------------+
        | Header (32 bytes)         |
        +---------------------------+
        | Slot Map (numSlots bytes) |
        +---------------------------+
        | Record 0                  |
        +---------------------------+
        | Record 1                  |
        +---------------------------+
        | ...                       |
    */

    int recordSize = attrCount * ATTR_SIZE;

    unsigned char *slotPointer =
        bufferptr +
        HEADER_SIZE +
        slotCount +
        (recordSize * slotNum);

    // Copy the record into rec
    memcpy(rec, slotPointer, recordSize);

    return SUCCESS;
}

// Store the record at slotNum into the block
int RecBuffer::setRecord(union Attribute *rec, int slotNum) {

   HeadInfo head;

    int ret = getHeader(&head);
    if (ret != SUCCESS)
        return ret;

    int attrCount = head.numAttrs;
    int slotCount = head.numSlots;

   

    unsigned char* bufferptr;
    ret=loadBlockAndGetBufferPtr(&bufferptr);
    
    if(ret!=SUCCESS){
        return ret;
    }

    int recordSize = attrCount * ATTR_SIZE;


    unsigned char *slotPointer =
        bufferptr +
        HEADER_SIZE +
        slotCount +
        (recordSize * slotNum);


    // Copy updated record into block
    memcpy(slotPointer, rec, recordSize);


    // Write updated block back to disk
    Disk::writeBlock(bufferptr, this->blockNum);


    return SUCCESS;
}
int BlockBuffer::loadBlockAndGetBufferPtr(unsigned char **buffPtr) {
  // check whether the block is already present in the buffer using StaticBuffer.getBufferNum()

    int bufferNum = StaticBuffer::getBufferNum(this->blockNum);

   

    if (bufferNum == E_BLOCKNOTINBUFFER) {


        bufferNum = StaticBuffer::getFreeBuffer(this->blockNum);


        if (bufferNum < 0) {
            return bufferNum;
        }
    Disk::readBlock(StaticBuffer::blocks[bufferNum], this->blockNum);
  }

  // store the pointer to this buffer (blocks[bufferNum]) in *buffPtr
  *buffPtr = StaticBuffer::blocks[bufferNum];

  return SUCCESS;
}
int RecBuffer::getSlotMap(unsigned char *slotMap) {
    unsigned char *bufferPtr;

    // Get the starting address of the buffer containing the block
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);
    if (ret != SUCCESS) {
        return ret;
    }

    struct HeadInfo head;

    // Get the header of the block
    getHeader(&head);

    // Number of slots in the block
    int slotCount = head.numSlots;

    // Pointer to the beginning of the slot map
    unsigned char *slotMapInBuffer = bufferPtr + HEADER_SIZE;

    // Copy slot map to caller's buffer
    memcpy(slotMap, slotMapInBuffer, slotCount);

    return SUCCESS;
}
int compareAttrs(union Attribute attr1, union Attribute attr2, int attrType) {

    double diff;

    if (attrType == STRING) {
        diff = strcmp(attr1.sVal, attr2.sVal);
    }
    else {
        diff = attr1.nVal - attr2.nVal;
    }

    if (diff > 0)
        return 1;
    else if (diff < 0)
        return -1;
    else
        return 0;
}