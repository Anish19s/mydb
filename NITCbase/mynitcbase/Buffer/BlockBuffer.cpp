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
int BlockBuffer::loadBlockAndGetBufferPtr(unsigned char **buffPtr) {

    // Check whether block is already present in buffer
    int bufferNum = StaticBuffer::getBufferNum(this->blockNum);

    if (bufferNum != E_BLOCKNOTINBUFFER &&
        bufferNum != E_OUTOFBOUND) {

        // Block is already in buffer.
        // This buffer was just accessed, so its timestamp becomes 0.
        for (int i = 0; i < BUFFER_CAPACITY; i++) {

            if (StaticBuffer::metainfo[i].free == false) {

                if (i == bufferNum)
                    StaticBuffer::metainfo[i].timeStamp = 0;
                else
                    StaticBuffer::metainfo[i].timeStamp++;
            }
        }
    }

    else {

        // Get a free/replacement buffer
        bufferNum = StaticBuffer::getFreeBuffer(this->blockNum);

        // Invalid block number
        if (bufferNum == E_OUTOFBOUND) {
            return E_OUTOFBOUND;
        }

        // Read block from disk into selected buffer
        int retVal = Disk::readBlock(
            StaticBuffer::blocks[bufferNum],
            this->blockNum
        );

        if (retVal != SUCCESS) {
            return retVal;
        }
    }

    // Return pointer to the buffer containing the block
    *buffPtr = StaticBuffer::blocks[bufferNum];

    return SUCCESS;
}
int BlockBuffer::setHeader(struct HeadInfo *head) {

    unsigned char *bufferPtr;

    // Get the starting address of the buffer containing the block
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);

    // If loading the block failed, return the error code
    if (ret != SUCCESS)
        return ret;

    // Cast bufferPtr to HeadInfo*
    struct HeadInfo *bufferHeader = (struct HeadInfo *)bufferPtr;

    // Copy the header fields except reserved
    bufferHeader->blockType = head->blockType;
    bufferHeader->numSlots = head->numSlots;
    bufferHeader->numEntries = head->numEntries;

    // Mark the buffer block as dirty
    int bufferNum = StaticBuffer::getBufferNum(this->blockNum);
    ret = StaticBuffer::setDirtyBit(bufferNum);

    if (ret != SUCCESS)
        return ret;

    return SUCCESS;
}
int RecBuffer::setRecord(union Attribute *rec, int slotNum) {

    unsigned char *bufferPtr;

    int retVal = loadBlockAndGetBufferPtr(&bufferPtr);

    if (retVal != SUCCESS) {
        return retVal;
    }

    HeadInfo head;

    retVal = getHeader(&head);

    if (retVal != SUCCESS) {
        return retVal;
    }

    int numAttrs = head.numAttrs;
    int numSlots = head.numSlots;

    if (slotNum < 0 || slotNum >= numSlots) {
        return E_OUTOFBOUND;
    }

    int recordSize = ATTR_SIZE * numAttrs;

    unsigned char *recordPtr =
        bufferPtr +
        HEADER_SIZE +
        numSlots +
        (slotNum * recordSize);

    memcpy(recordPtr, rec, recordSize);

    retVal = StaticBuffer::setDirtyBit(this->blockNum);

    if (retVal != SUCCESS) {
        return retVal;
    }

    return SUCCESS;
}
int BlockBuffer::setBlockType(int blockType) {

    unsigned char *bufferPtr;

    // Get the starting address of the buffer containing the block
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);

    // If loading the block failed, return the error code
    if (ret != SUCCESS)
        return ret;

    // Store blockType in the first 4 bytes of the buffer
    *((int32_t *)bufferPtr) = blockType;

    // Update the block allocation map
    StaticBuffer::blockAllocMap[this->blockNum] = blockType;

    // Update dirty bit
    int bufferNum = StaticBuffer::getBufferNum(this->blockNum);
    ret = StaticBuffer::setDirtyBit(bufferNum);

    if (ret != SUCCESS)
        return ret;

    return SUCCESS;
}
iint BlockBuffer::getFreeBlock(int blockType) {

    // Find a free block in the disk
    int freeBlock = -1;

    for (int i = 0; i < DISK_BLOCKS; i++) {
        if (StaticBuffer::blockAllocMap[i] == FREE) {
            freeBlock = i;
            break;
        }
    }

    // No free block
    if (freeBlock == -1)
        return E_DISKFULL;

    // Set this object's block number
    blockNum = freeBlock;

    // Find a free buffer
    int ret = StaticBuffer::getFreeBuffer();

    if (ret<0)
        return ret;

    int bufferNum = ret;

    // Initialize the block header
    HeadInfo head;

    head.pblock = -1;
    head.lblock = -1;
    head.rblock = -1;
    head.numEntries = 0;
    head.numAttrs = 0;
    head.numSlots = 0;

    ret = setHeader(&head);

    if (ret != SUCCESS)
        return ret;

    // Set the block type
    ret = setBlockType(blockType);

    if (ret != SUCCESS)
        return ret;

    return blockNum;
}
BlockBuffer::BlockBuffer(char blockType) {

    // Allocate a free block on disk and a free buffer in memory
    int ret = getFreeBlock(blockType);

    // If allocation succeeded, ret is the allocated block number.
    // If it failed, ret is the error code.
    blockNum = ret;
}
RecBuffer::RecBuffer() : BlockBuffer('R') {}
