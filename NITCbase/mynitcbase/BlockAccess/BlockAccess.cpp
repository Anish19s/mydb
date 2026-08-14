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

    Attribute attrCatRelName;
    strcpy(attrCatRelName.sVal, oldName);

    while (true) {

        RecId attrRecId = BlockAccess::linearSearch(
            ATTRCAT_RELID,
            "RelName",
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

    RecId relRecId = BlockAccess::linearSearch(
        RELCAT_RELID,
        "RelName",
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
            "RelName",
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