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