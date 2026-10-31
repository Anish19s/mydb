#include "BlockAccess.h"
#include <cstring>
#include "../Cache/RelCacheTable.h"
#include "../Cache/AttrCacheTable.h"
#include "../define/id.h"

RecId BlockAccess::linearSearch(int relId, char attrName[ATTR_SIZE],
                                union Attribute attrVal, int op) {

    // Get previous search index
    RecId prevRecId;

    int ret = RelCacheTable::getSearchIndex(relId, &prevRecId);

    if (ret != SUCCESS)
        return RecId{-1, -1};

    // Get relation catalog entry
    RelCatEntry relCatEntry;

    ret = RelCacheTable::getRelCatEntry(relId, &relCatEntry);

    if (ret != SUCCESS)
        return RecId{-1, -1};

    int block, slot;

    // No previous search
    if (prevRecId.block == -1 && prevRecId.slot == -1) {

        block = relCatEntry.firstBlk;
        slot = 0;
    }
    else {

        // Continue from record after previous hit
        block = prevRecId.block;
        slot = prevRecId.slot + 1;
    }

    // Get attribute catalog entry
    AttrCatEntry attrCatEntry;

    ret = AttrCacheTable::getAttrCatEntry(
        relId,
        attrName,
        &attrCatEntry
    );

    if (ret != SUCCESS)
        return RecId{-1, -1};

    // Search blocks
    while (block != -1) {

        RecBuffer recBuffer(block);

        // Get header
        HeadInfo head;

        ret = recBuffer.getHeader(&head);

        if (ret != SUCCESS)
            return RecId{-1, -1};

        // Get slot map
        unsigned char slotMap[head.numSlots];

        ret = recBuffer.getSlotMap(slotMap);

        if (ret != SUCCESS)
            return RecId{-1, -1};

        // Current block exhausted
        if (slot >= head.numSlots) {

            block = head.rblock;
            slot = 0;

            continue;
        }

        // Slot is unoccupied
        if (slotMap[slot] == SLOT_UNOCCUPIED) {

            slot++;
            continue;
        }

        // Get record
        Attribute record[relCatEntry.numAttrs];

        ret = recBuffer.getRecord(record, slot);

        if (ret != SUCCESS)
            return RecId{-1, -1};

        // Get attribute from record
        Attribute recordAttr =
            record[attrCatEntry.offset];

        // Compare
        int cmpVal = compareAttrs(
            recordAttr,
            attrVal,
            attrCatEntry.attrType
        );

        // Check condition
        if (
            (op == NE && cmpVal != 0) ||
            (op == LT && cmpVal < 0) ||
            (op == LE && cmpVal <= 0) ||
            (op == EQ && cmpVal == 0) ||
            (op == GT && cmpVal > 0) ||
            (op == GE && cmpVal >= 0)
        ) {

            // Update search index
            RecId currentRecId{block, slot};

            ret = RelCacheTable::setSearchIndex(
                relId,
                &currentRecId
            );

            if (ret != SUCCESS)
                return RecId{-1, -1};

            return currentRecId;
        }

        // Next slot
        slot++;
    }

    // No matching record
    return RecId{-1, -1};
}
int BlockAccess::renameRelation(char oldName[ATTR_SIZE], char newName[ATTR_SIZE]) {
    char relNameAttrName[ATTR_SIZE];
    strcpy(relNameAttrName, "RelName");
    // Check if new relation name already exists
    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    Attribute newRelationName;
    strcpy(newRelationName.sVal, newName);

    RecId recId = BlockAccess::linearSearch(
        RELCAT_RELID,
        relNameAttrName,
        newRelationName,
        EQ
    );

    if (recId.block != -1 && recId.slot != -1) {
        return E_RELEXIST;
    }


    // Check if old relation exists
    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    Attribute oldRelationName;
    strcpy(oldRelationName.sVal, oldName);

    recId = BlockAccess::linearSearch(
        RELCAT_RELID,
        relNameAttrName,
        oldRelationName,
        EQ
    );

    if (recId.block == -1 && recId.slot == -1) {
        return E_RELNOTEXIST;
    }


    // Get the relation catalog record
    RecBuffer recBuffer(recId.block);

    Attribute relCatRecord[RELCAT_NO_ATTRS];

    int retVal = recBuffer.getRecord(
        relCatRecord,
        recId.slot
    );

    if (retVal != SUCCESS) {
        return retVal;
    }


    // Change relation name
    strcpy(
        relCatRecord[RELCAT_REL_NAME_INDEX].sVal,
        newName
    );


    // Write updated relation catalog record
    retVal = recBuffer.setRecord(
        relCatRecord,
        recId.slot
    );

    if (retVal != SUCCESS) {
        return retVal;
    }


    // Now update Attribute Catalog entries
    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

    char attrCatAttrName[ATTR_SIZE];
    strcpy(attrCatAttrName, "RelName");

    Attribute attrCatRelName;
    strcpy(attrCatRelName.sVal, oldName);

    while (true) {

        RecId attrRecId = BlockAccess::linearSearch(
            ATTRCAT_RELID,
            attrCatAttrName,
            attrCatRelName,
            EQ
        );

        // No more matching entries
        if (attrRecId.block == -1 && attrRecId.slot == -1) {
            break;
        }


        // Get Attribute Catalog record
        RecBuffer attrRecBuffer(attrRecId.block);

        Attribute attrCatRecord[ATTRCAT_NO_ATTRS];

        retVal = attrRecBuffer.getRecord(
            attrCatRecord,
            attrRecId.slot
        );

        if (retVal != SUCCESS) {
            return retVal;
        }


        // Change relation name in Attribute Catalog
        strcpy(
            attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal,
            newName
        );


        // Write updated record
        retVal = attrRecBuffer.setRecord(
            attrCatRecord,
            attrRecId.slot
        );

        if (retVal != SUCCESS) {
            return retVal;
        }
    }

    return SUCCESS;
}
int BlockAccess::renameAttribute(char relName[ATTR_SIZE],
                                  char oldName[ATTR_SIZE],
                                  char newName[ATTR_SIZE]) {

    // Check whether relation exists
    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    Attribute relNameAttr;
    strcpy(relNameAttr.sVal, relName);

    char relNameAttrName[ATTR_SIZE];
    strcpy(relNameAttrName, "RelName");

    RecId relRecId = BlockAccess::linearSearch(
        RELCAT_RELID,
        relNameAttrName,
        relNameAttr,
        EQ
    );

    if (relRecId.block == -1 && relRecId.slot == -1) {
        return E_RELNOTEXIST;
    }


    // Search Attribute Catalog
    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

    RecId attrToRenameRecId{-1, -1};

    Attribute attrCatEntryRecord[ATTRCAT_NO_ATTRS];

    while (true) {

        // Find next attribute belonging to relName
        RecId attrRecId = BlockAccess::linearSearch(
            ATTRCAT_RELID,
            relNameAttrName,
            relNameAttr,
            EQ
        );

        // No more attributes
        if (attrRecId.block == -1 && attrRecId.slot == -1) {
            break;
        }

        // Get Attribute Catalog record
        RecBuffer attrCatBuffer(attrRecId.block);

        int retVal = attrCatBuffer.getRecord(
            attrCatEntryRecord,
            attrRecId.slot
        );

        if (retVal != SUCCESS) {
            return retVal;
        }

        // If old attribute is found
        if (strcmp(
                attrCatEntryRecord[ATTRCAT_ATTR_NAME_INDEX].sVal,
                oldName
            ) == 0) {

            attrToRenameRecId = attrRecId;
        }

        // If new attribute already exists
        if (strcmp(
                attrCatEntryRecord[ATTRCAT_ATTR_NAME_INDEX].sVal,
                newName
            ) == 0) {

            return E_ATTREXIST;
        }
    }


    // Old attribute does not exist
    if (attrToRenameRecId.block == -1 &&
        attrToRenameRecId.slot == -1) {

        return E_ATTRNOTEXIST;
    }


    // Get the attribute catalog record again
    RecBuffer attrCatBuffer(attrToRenameRecId.block);

    int retVal = attrCatBuffer.getRecord(
        attrCatEntryRecord,
        attrToRenameRecId.slot
    );

    if (retVal != SUCCESS) {
        return retVal;
    }


    // Rename attribute
    strcpy(
        attrCatEntryRecord[ATTRCAT_ATTR_NAME_INDEX].sVal,
        newName
    );


    // Save updated record
    retVal = attrCatBuffer.setRecord(
        attrCatEntryRecord,
        attrToRenameRecId.slot
    );

    if (retVal != SUCCESS) {
        return retVal;
    }

    return SUCCESS;
}
int BlockAccess::insert(int relId, Attribute *record) {

    // Get relation catalog entry
    RelCatEntry relCatEntry;

    int ret = RelCacheTable::getRelCatEntry(relId, &relCatEntry);
    if (ret != SUCCESS)
        return ret;

    int blockNum = relCatEntry.firstBlk;

    RecId rec_id = {-1, -1};

    int numOfSlots = relCatEntry.numSlotsPerBlk;
    int numOfAttributes = relCatEntry.numAttrs;

    int prevBlockNum = -1;
    int freeSlot = -1;

    // Search existing record blocks for a free slot
    while (blockNum != -1) {

        RecBuffer recBuffer(blockNum);

        HeadInfo header;
        ret = recBuffer.getHeader(&header);
        if (ret != SUCCESS)
            return ret;

        unsigned char slotMap[header.numSlots];

        ret = recBuffer.getSlotMap(slotMap);
        if (ret != SUCCESS)
            return ret;

        // Search for an unoccupied slot
        for (int slot = 0; slot < header.numSlots; slot++) {

            if (slotMap[slot] == SLOT_UNOCCUPIED) {

                rec_id.block = blockNum;
                rec_id.slot = slot;
                freeSlot = slot;

                break;
            }
        }

        // Free slot found: stop traversing
        if (rec_id.block != -1)
            break;

        // Move to the next record block
        prevBlockNum = blockNum;
        blockNum = header.rblock;
    }

    // No free slot exists in the current record blocks
    if (rec_id.block == -1) {

        // Do not allocate additional blocks for the relation catalog
        if (relId == RELCAT_RELID)
            return E_MAXRELATIONS;

        // Allocate a new record block
        RecBuffer newRecBuffer;

        int newBlockNum = newRecBuffer.getBlockNum();

        if (newBlockNum == E_DISKFULL)
            return E_DISKFULL;

        rec_id.block = newBlockNum;
        rec_id.slot = 0;
        freeSlot = 0;

        // Initialize the new block's header
        HeadInfo header;

        header.blockType = REC;
        header.pblock = -1;
        header.lblock = prevBlockNum;
        header.rblock = -1;
        header.numEntries = 0;
        header.numSlots = numOfSlots;
        header.numAttrs = numOfAttributes;

        ret = newRecBuffer.setHeader(&header);
        if (ret != SUCCESS)
            return ret;

        // Initialize every slot as unoccupied
        unsigned char slotMap[numOfSlots];

        for (int i = 0; i < numOfSlots; i++)
            slotMap[i] = SLOT_UNOCCUPIED;

        ret = newRecBuffer.setSlotMap(slotMap);
        if (ret != SUCCESS)
            return ret;

        // Link the new block to the existing linked list
        if (prevBlockNum != -1) {

            RecBuffer prevRecBuffer(prevBlockNum);

            HeadInfo prevHeader;

            ret = prevRecBuffer.getHeader(&prevHeader);
            if (ret != SUCCESS)
                return ret;

            prevHeader.rblock = newBlockNum;

            ret = prevRecBuffer.setHeader(&prevHeader);
            if (ret != SUCCESS)
                return ret;

        } else {

            // This is the first record block
            relCatEntry.firstBlk = newBlockNum;

            ret = RelCacheTable::setRelCatEntry(
                relId, &relCatEntry
            );

            if (ret != SUCCESS)
                return ret;
        }

        // Update the last block of the relation
        relCatEntry.lastBlk = newBlockNum;

        ret = RelCacheTable::setRelCatEntry(
            relId, &relCatEntry
        );

        if (ret != SUCCESS)
            return ret;
    }

    // Insert the record into the selected slot
    RecBuffer recBuffer(rec_id.block);

    ret = recBuffer.setRecord(record, freeSlot);
    if (ret != SUCCESS)
        return ret;

    // Mark the slot as occupied
    HeadInfo header;
    ret = recBuffer.getHeader(&header);
    if (ret != SUCCESS)
        return ret;

    unsigned char slotMap[header.numSlots];

    ret = recBuffer.getSlotMap(slotMap);
    if (ret != SUCCESS)
        return ret;

    slotMap[freeSlot] = SLOT_OCCUPIED;

    ret = recBuffer.setSlotMap(slotMap);
    if (ret != SUCCESS)
        return ret;

    // Increment the number of entries in this record block
    header.numEntries++;

    ret = recBuffer.setHeader(&header);
    if (ret != SUCCESS)
        return ret;

    // Increment the relation's total record count
    relCatEntry.numRecs++;

    ret = RelCacheTable::setRelCatEntry(
        relId, &relCatEntry
    );
    if (ret != SUCCESS)
        return ret;

    // Insert the record into all applicable B+ tree indexes
    int flag = SUCCESS;

    for (int attrOffset = 0;
         attrOffset < numOfAttributes;
         attrOffset++) {

        AttrCatEntry attrCatEntry;

        ret = AttrCacheTable::getAttrCatEntry(
            relId, attrOffset, &attrCatEntry
        );

        if (ret != SUCCESS)
            return ret;

        // Skip attributes that do not have an index
        if (attrCatEntry.rootBlock == -1)
            continue;

        int retVal = BPlusTree::bPlusInsert(
            relId,
            attrCatEntry.attrName,
            record[attrOffset],
            rec_id
        );

        // The index was destroyed because the disk was full
        if (retVal == E_DISKFULL)
            flag = E_INDEX_BLOCKS_RELEASED;
        else if (retVal != SUCCESS)
            return retVal;
    }

    return flag;
}

int BlockAccess::deleteRelation(char *relName) {

    // Catalog relations cannot be deleted
    if (strcmp(relName, RELCAT_RELNAME) == 0 ||
        strcmp(relName, ATTRCAT_RELNAME) == 0) {
        return E_NOTPERMITTED;
    }

    // Reset relation catalog search index
    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    Attribute relNameAttr;
    strcpy(relNameAttr.sVal, relName);

    // Find relation catalog entry
    char relCatAttrName[ATTR_SIZE];
    strcpy(relCatAttrName, RELCAT_ATTR_RELNAME);

    RecId relCatRecId = linearSearch(
        RELCAT_RELID, relCatAttrName, relNameAttr, EQ
    );

    if (relCatRecId.block == -1 && relCatRecId.slot == -1)
        return E_RELNOTEXIST;

    // Read relation catalog record
    RecBuffer relCatBuffer(relCatRecId.block);

    Attribute relCatEntryRecord[RELCAT_NO_ATTRS];

    int ret = relCatBuffer.getRecord(
        relCatEntryRecord, relCatRecId.slot
    );
    if (ret != SUCCESS)
        return ret;

    // Get first record block and number of attributes
    int firstBlock = (int)relCatEntryRecord[RELCAT_FIRST_BLOCK_INDEX].nVal;
    int numAttrs = (int)relCatEntryRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal;

    // Delete all data blocks belonging to the relation
    int blockNum = firstBlock;

    while (blockNum != -1) {

        RecBuffer recBuffer(blockNum);

        HeadInfo header;

        ret = recBuffer.getHeader(&header);
        if (ret != SUCCESS)
            return ret;

        int nextBlock = header.rblock;

        recBuffer.releaseBlock();

        blockNum = nextBlock;
    }

    // Delete the relation's attribute catalog entries
    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

    char attrCatAttrName[ATTR_SIZE];
    strcpy(attrCatAttrName, ATTRCAT_ATTR_RELNAME);

    int numberOfAttributesDeleted = 0;

    while (true) {

        RecId attrCatRecId = linearSearch(
            ATTRCAT_RELID, attrCatAttrName, relNameAttr, EQ
        );

        if (attrCatRecId.block == -1 &&
            attrCatRecId.slot == -1) {
            break;
        }

        numberOfAttributesDeleted++;

        RecBuffer attrCatBuffer(attrCatRecId.block);

        HeadInfo header;

        ret = attrCatBuffer.getHeader(&header);
        if (ret != SUCCESS)
            return ret;

        Attribute attrCatRecord[ATTRCAT_NO_ATTRS];

        ret = attrCatBuffer.getRecord(
            attrCatRecord, attrCatRecId.slot
        );
        if (ret != SUCCESS)
            return ret;

        // Save index root before deleting the catalog entry
        int rootBlock =
            (int)attrCatRecord[ATTRCAT_ROOT_BLOCK_INDEX].nVal;

        // Free this attribute catalog slot
        unsigned char slotMap[header.numSlots];

        ret = attrCatBuffer.getSlotMap(slotMap);
        if (ret != SUCCESS)
            return ret;

        slotMap[attrCatRecId.slot] = SLOT_UNOCCUPIED;

        ret = attrCatBuffer.setSlotMap(slotMap);
        if (ret != SUCCESS)
            return ret;

        header.numEntries--;

        ret = attrCatBuffer.setHeader(&header);
        if (ret != SUCCESS)
            return ret;

        // Release an empty attribute catalog block
        if (header.numEntries == 0) {

            int lblock = header.lblock;
            int rblock = header.rblock;

            // Update left block's forward link
            if (lblock != -1) {

                RecBuffer leftBuffer(lblock);

                HeadInfo leftHeader;

                ret = leftBuffer.getHeader(&leftHeader);
                if (ret != SUCCESS)
                    return ret;

                leftHeader.rblock = rblock;

                ret = leftBuffer.setHeader(&leftHeader);
                if (ret != SUCCESS)
                    return ret;
            }

            // Update right block's backward link
            if (rblock != -1) {

                RecBuffer rightBuffer(rblock);

                HeadInfo rightHeader;

                ret = rightBuffer.getHeader(&rightHeader);
                if (ret != SUCCESS)
                    return ret;

                rightHeader.lblock = lblock;

                ret = rightBuffer.setHeader(&rightHeader);
                if (ret != SUCCESS)
                    return ret;

            } else {

                // This was the last attribute catalog block
                RelCatEntry attrRelCatEntry;

                ret = RelCacheTable::getRelCatEntry(
                    ATTRCAT_RELID, &attrRelCatEntry
                );
                if (ret != SUCCESS)
                    return ret;

                attrRelCatEntry.lastBlk = lblock;

                ret = RelCacheTable::setRelCatEntry(
                    ATTRCAT_RELID, &attrRelCatEntry
                );
                if (ret != SUCCESS)
                    return ret;
            }

            attrCatBuffer.releaseBlock();
        }

        // Destroy the B+ tree index, if one exists
        if (rootBlock != -1) {
            ret = BPlusTree::bPlusDestroy(rootBlock);
            if (ret != SUCCESS)
                return ret;
        }
    }

    // Delete the relation's entry from the relation catalog
    HeadInfo relCatHead;
    ret = relCatBuffer.getHeader(&relCatHead);
    if (ret != SUCCESS)
        return ret;

    relCatHead.numEntries--;

    ret = relCatBuffer.setHeader(&relCatHead);
    if (ret != SUCCESS)
        return ret;

    // Mark the relation catalog slot as free
    unsigned char relSlotMap[relCatHead.numSlots];

    ret = relCatBuffer.getSlotMap(relSlotMap);
    if (ret != SUCCESS)
        return ret;

    relSlotMap[relCatRecId.slot] = SLOT_UNOCCUPIED;

    ret = relCatBuffer.setSlotMap(relSlotMap);
    if (ret != SUCCESS)
        return ret;

    // Update relation catalog's record count in the cache
    RelCatEntry relcatEntry;

    ret = RelCacheTable::getRelCatEntry(
        RELCAT_RELID, &relcatEntry
    );
    if (ret != SUCCESS)
        return ret;

    relcatEntry.numRecs--;

    ret = RelCacheTable::setRelCatEntry(
        RELCAT_RELID, &relcatEntry
    );
    if (ret != SUCCESS)
        return ret;

    // Update attribute catalog's record count in the cache
    RelCatEntry attrcatEntry;

    ret = RelCacheTable::getRelCatEntry(
        ATTRCAT_RELID, &attrcatEntry
    );
    if (ret != SUCCESS)
        return ret;

    attrcatEntry.numRecs -= numberOfAttributesDeleted;

    ret = RelCacheTable::setRelCatEntry(
        ATTRCAT_RELID, &attrcatEntry
    );
    if (ret != SUCCESS)
        return ret;

    return SUCCESS;
}

int BlockAccess::project(int relId, Attribute *record) {
    // get the previous search index of the relation relId from the relation cache
    RecId prevRecId;
    int ret = RelCacheTable::getSearchIndex(relId, &prevRecId);

    if (ret != SUCCESS)
        return ret;

    // declare block and slot
    int block, slot;

    /*
     * If the current search index is invalid, start from the
     * first record of the relation.
     */
    if (prevRecId.block == -1 && prevRecId.slot == -1)
    {
        // get relation catalog entry
        RelCatEntry relCatEntry;
        ret = RelCacheTable::getRelCatEntry(relId, &relCatEntry);
        if (ret != SUCCESS)
            return ret;

        // start from first record block
        block = relCatEntry.firstBlk;
        slot = 0;
    }
    else
    {
        // continue from the record after the previous record
        block = prevRecId.block;
        slot = prevRecId.slot + 1;
    }

    /*
     * Find the next occupied slot.
     */
    while (block != -1)
    {
        // create RecBuffer object for the current block
        RecBuffer recBuffer(block);

        // get block header
        HeadInfo head;
        ret = recBuffer.getHeader(&head);
        if (ret != SUCCESS)
            return ret;

        // get slot map
        unsigned char slotMap[head.numSlots];
        ret = recBuffer.getSlotMap(slotMap);
        if (ret != SUCCESS)
            return ret;

        // if all slots in this block have been checked
        if (slot >= head.numSlots)
        {
            // move to the next block
            block = head.rblock;
            slot = 0;
        }
        else if (slotMap[slot] == SLOT_UNOCCUPIED)
        {
            // current slot is free, move to next slot
            slot++;
        }
        else
        {
            // occupied slot found
            break;
        }
    }

    // no record found
    if (block == -1)
    {
        return E_NOTFOUND;
    }

    // store the record id of the record found
    RecId nextRecId{block, slot};

    // update the search index
    RelCacheTable::setSearchIndex(relId, &nextRecId);

    // create RecBuffer for the record
    RecBuffer recBuffer(nextRecId.block);

    // copy the record into the caller-provided buffer
    ret = recBuffer.getRecord(record, nextRecId.slot);

    return ret;
}

int BlockAccess::search(int relId, Attribute *record, char attrName[ATTR_SIZE],
                        Attribute attrVal, int op) {

    // Declare a variable called recId to store the searched record
    RecId recId;

    // Get the attribute catalog entry corresponding to relId and attrName
    AttrCatEntry attrCatEntry;
    int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);

    // If the attribute does not exist / relation is invalid, return the error
    if (ret != SUCCESS) {
        return ret;
    }

    // Get root block of the B+ tree
    int rootBlock = attrCatEntry.rootBlock;

    // If no index exists for this attribute, use linear search
    if (rootBlock == -1) {

        recId = BlockAccess::linearSearch(relId, attrName, attrVal, op);

    }
    // Otherwise, use B+ tree search
    else {

        recId = BPlusTree::bPlusSearch(relId, attrName, attrVal, op);
    }

    // No record satisfying the condition was found
    if (recId.block == -1 && recId.slot == -1) {
        return E_NOTFOUND;
    }

    // Fetch the actual record using the RecId
    RecBuffer recBuffer(recId.block);

    ret = recBuffer.getRecord(record, recId.slot);

    if (ret != SUCCESS) {
        return ret;
    }

    return SUCCESS;
}