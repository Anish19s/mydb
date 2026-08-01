#include "BlockBuffer.h"
#include <cstring>

// Constructor
BlockBuffer::BlockBuffer(int blockNum) {
    this->blockNum = blockNum;
}

// Calls the parent class constructor
RecBuffer::RecBuffer(int blockNum)
    : BlockBuffer(blockNum) {}

// Load the block header into the argument pointer
int BlockBuffer::getHeader(struct HeadInfo *head) {

    unsigned char buffer[BLOCK_SIZE];

    // Read the block into the buffer
    Disk::readBlock(buffer, this->blockNum);

    // Populate the header fields
    memcpy(&head->blockType, buffer + 0, 4);
    memcpy(&head->pblock,    buffer + 4, 4);
    memcpy(&head->lblock,    buffer + 8, 4);
    memcpy(&head->rblock,    buffer + 12, 4);
    memcpy(&head->numEntries, buffer + 16, 4);
    memcpy(&head->numAttrs,   buffer + 20, 4);
    memcpy(&head->numSlots,   buffer + 24, 4);
    memcpy(head->reserved,    buffer + 28, 4);

    return SUCCESS;
}

// Load the record at slotNum into the argument pointer
int RecBuffer::getRecord(union Attribute *rec, int slotNum) {

    HeadInfo head;

    // Read the header
    getHeader(&head);

    int attrCount = head.numAttrs;
    int slotCount = head.numSlots;

    unsigned char buffer[BLOCK_SIZE];

    // Read the block
    Disk::readBlock(buffer, this->blockNum);

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
        buffer +
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

    // Get header information
    getHeader(&head);

    int attrCount = head.numAttrs;
    int slotCount = head.numSlots;

    unsigned char buffer[BLOCK_SIZE];

    // Read block into buffer
    Disk::readBlock(buffer, this->blockNum);


    int recordSize = attrCount * ATTR_SIZE;


    unsigned char *slotPointer =
        buffer +
        HEADER_SIZE +
        slotCount +
        (recordSize * slotNum);


    // Copy updated record into block
    memcpy(slotPointer, rec, recordSize);


    // Write updated block back to disk
    Disk::writeBlock(buffer, this->blockNum);


    return SUCCESS;
}