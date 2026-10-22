#include "BPlusTree.h"
#include <cstring>
#include<iostream>

RecId BPlusTree::bPlusSearch(int relId, char attrName[ATTR_SIZE], union Attribute attrVal, int op) {
    // Declare a variable called searchIndex of type IndexId
    IndexId searchIndex;

    // Get the search index from the attribute cache
    int ret = AttrCacheTable::getSearchIndex(relId, attrName, &searchIndex);
    if (ret != SUCCESS) {
        return RecId{-1, -1};
    }

    // Get the attribute catalog entry from the attribute cache
    AttrCatEntry attrCatEntry;
    ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
    if (ret != SUCCESS) {
        return RecId{-1, -1};
    }

    int block = -1;
    int index = -1;

    /*
     * If searchIndex is {-1, -1}, this is a new search.
     * Traverse down the B+ tree from the root to locate the appropriate leaf.
     */
    if (searchIndex.block == -1 && searchIndex.index == -1) {
        block = attrCatEntry.rootBlock;
        index = 0;

        if (block == -1) {
            return RecId{-1, -1};
        }

        while (StaticBuffer::getStaticBlockType(block) == IND_INTERNAL) {
            IndInternal internalBlock(block);
            HeadInfo intHead;
            internalBlock.getHeader(&intHead);

            InternalEntry entry;
            int cond = 0;

            for (int i = 0; i < intHead.numEntries; i++) {
                internalBlock.getEntry(&entry, i);
                int cmp = compareAttrs(entry.attrVal, attrVal, attrCatEntry.attrType);

                if (
                    (op == EQ && cmp >= 0) ||
                    (op == LE) ||
                    (op == LT) ||
                    (op == GE && cmp >= 0) ||
                    (op == GT && cmp > 0) ||
                    (op == NE)
                ) {
                    block = entry.lChild;
                    cond = 1;
                    break;
                }
            }

            if (cond == 0) {
                block = entry.rChild;
            }
        }
    } else {
        /*
         * Continuing a previous search. Start from the entry right after searchIndex.
         */
        block = searchIndex.block;
        index = searchIndex.index + 1;

        IndLeaf leafBlock(block);
        HeadInfo leafHead;
        leafBlock.getHeader(&leafHead);

        if (index >= leafHead.numEntries) {
            block = leafHead.rblock;
            index = 0;

            if (block == -1) {
                return RecId{-1, -1};
            }
        }
    }

    /*
     * Traverse through leaf index entries until a match is found
     * or the leaf chain is exhausted.
     */
    while (block != -1) {
        IndLeaf leafBlock(block);
        HeadInfo leafHead;
        leafBlock.getHeader(&leafHead);

        while (index < leafHead.numEntries) {
            Index leafEntry;
            leafBlock.getEntry(&leafEntry, index);

            int cmp = compareAttrs(leafEntry.attrVal, attrVal, attrCatEntry.attrType);
            int cond = 0;

            switch (op) {
                case EQ:
                    if (cmp == 0) cond = 1;
                    else if (cmp > 0) cond = -1;
                    break;
                case LE:
                    if (cmp <= 0) cond = 1;
                    else cond = -1;
                    break;
                case LT:
                    if (cmp < 0) cond = 1;
                    else cond = -1;
                    break;
                case GE:
                    if (cmp >= 0) cond = 1;
                    break;
                case GT:
                    if (cmp > 0) cond = 1;
                    break;
                case NE:
                    if (cmp != 0) cond = 1;
                    break;
            }

            if (cond == 1) {
                IndexId nextIndex{block, index};
                AttrCacheTable::setSearchIndex(relId, attrName, &nextIndex);
                return RecId{leafEntry.block, leafEntry.slot};
            } else if (cond == -1) {
                return RecId{-1, -1};
            }

            index++;
        }

        block = leafHead.rblock;
        index = 0;
    }

    return RecId{-1, -1};
}

int BPlusTree::bPlusCreate(int relId, char attrName[ATTR_SIZE]) {

    // Do not allow indexes on catalog relations
    if (relId == RELCAT_RELID || relId == ATTRCAT_RELID)
        return E_NOTPERMITTED;

    // Get the attribute catalog entry
    AttrCatEntry attrCatEntry;

    int ret = AttrCacheTable::getAttrCatEntry(
        relId, attrName, &attrCatEntry
    );

    if (ret != SUCCESS)
        return ret;

    // If index already exists, nothing to do
    if (attrCatEntry.rootBlock != -1)
        return SUCCESS;

    // Allocate the root leaf block
    IndLeaf rootBlockBuf;
    int rootBlock = rootBlockBuf.getBlockNum();

    if (rootBlock == E_DISKFULL)
        return E_DISKFULL;

    // Set the root block in the attribute catalog entry
    attrCatEntry.rootBlock = rootBlock;

    ret = AttrCacheTable::setAttrCatEntry(
        relId, attrName, &attrCatEntry
    );

    if (ret != SUCCESS)
        return ret;

    // Get the relation catalog entry
    RelCatEntry relCatEntry;

    ret = RelCacheTable::getRelCatEntry(relId, &relCatEntry);

    if (ret != SUCCESS)
        return ret;

    // Start from the first record block
    int block = relCatEntry.firstBlk;

    // Traverse all record blocks
    while (block != -1) {

        RecBuffer recBuffer(block);

        unsigned char slotMap[relCatEntry.numSlotsPerBlk];

        ret = recBuffer.getSlotMap(slotMap);

        if (ret != SUCCESS)
            return ret;

        // Examine every slot
        for (int slot = 0;
             slot < relCatEntry.numSlotsPerBlk;
             slot++) {

            if (slotMap[slot] == SLOT_OCCUPIED) {

                Attribute record[relCatEntry.numAttrs];

                ret = recBuffer.getRecord(record, slot);

                if (ret != SUCCESS)
                    return ret;

                // Record identifier
                RecId recId{block, slot};

                // Value of the indexed attribute
                Attribute attrVal = record[attrCatEntry.offset];

                // Insert into the B+ tree
                ret = bPlusInsert(
                    relId, attrName, attrVal, recId
                );

                if (ret == E_DISKFULL)
                    return E_DISKFULL;

                if (ret != SUCCESS)
                    return ret;
            }
        }

        // Get the current block's header
        HeadInfo header;

        ret = recBuffer.getHeader(&header);

        if (ret != SUCCESS)
            return ret;

        // Move to the next record block
        block = header.rblock;
    }

    return SUCCESS;
}

int BPlusTree::bPlusInsert(int relId, char attrName[ATTR_SIZE],
                           Attribute attrVal, RecId recId) {

    // Get the attribute catalog entry
    AttrCatEntry attrCatEntry;

    int ret = AttrCacheTable::getAttrCatEntry(
        relId, attrName, &attrCatEntry
    );

    if (ret != SUCCESS)
        return ret;

    // Get the root block of the B+ tree
    int blockNum = attrCatEntry.rootBlock;

    // Check whether an index exists
    if (blockNum == -1)
        return E_NOINDEX;

    // Find the leaf where the entry should be inserted
    int leafBlkNum = findLeafToInsert(
        blockNum, attrVal, attrCatEntry.attrType
    );

    if (leafBlkNum < 0)
        return leafBlkNum;

    // Prepare the index entry
    struct Index entry;
    entry.attrVal = attrVal;
    entry.block = recId.block;
    entry.slot = recId.slot;

    // Insert the entry into the leaf
    ret = insertIntoLeaf(relId, attrName, leafBlkNum, entry);

    // Handle disk-full error
    if (ret == E_DISKFULL) {

        // Destroy the existing B+ tree
        bPlusDestroy(blockNum);

        // Update the root block in the attribute catalog cache
        attrCatEntry.rootBlock = -1;

        AttrCacheTable::setAttrCatEntry(
            relId, attrName, &attrCatEntry
        );

        return E_DISKFULL;
    }

    return ret;
}
int BPlusTree::findLeafToInsert(int rootBlock, Attribute attrVal, int attrType) {

    int blockNum = rootBlock;

    while (StaticBuffer::getStaticBlockType(blockNum) != IND_LEAF) {

        IndInternal internalBlk(blockNum);

        HeadInfo header;
        internalBlk.getHeader(&header);

        int nEntries = header.numEntries;
        int i;

        for (i = 0; i < nEntries; i++) {

            InternalEntry entry;
            internalBlk.getEntry(&entry, i);

            if (compareAttrs(attrVal, entry.attrVal, attrType) <= 0) {
                blockNum = entry.lChild;
                break;
            }
        }

        if (i == nEntries) {
            // No entry has an attribute value >= attrVal.
            InternalEntry entry;
            internalBlk.getEntry(&entry, nEntries - 1);

            blockNum = entry.rChild;
        }
    }

    return blockNum;
}
int BPlusTree::insertIntoLeaf(int relId, char attrName[ATTR_SIZE],
                              int blockNum, Index indexEntry) {

    // Get the attribute catalog entry
    AttrCatEntry attrCatEntry;

    int ret = AttrCacheTable::getAttrCatEntry(
        relId, attrName, &attrCatEntry
    );

    if (ret != SUCCESS)
        return ret;

    // Construct the leaf block
    IndLeaf leaf(blockNum);

    // Get the leaf header
    HeadInfo blockHeader;

    ret = leaf.getHeader(&blockHeader);

    if (ret != SUCCESS)
        return ret;

    int n = blockHeader.numEntries;

    // Store existing entries and the new entry in sorted order
    Index indices[MAX_KEYS_LEAF + 1];

    int i = 0;
    int j = 0;
    bool inserted = false;

    for (i = 0; i < n; i++) {

        Index currentEntry;

        ret = leaf.getEntry(&currentEntry, i);

        if (ret != SUCCESS)
            return ret;

        if (!inserted &&
            compareAttrs(indexEntry.attrVal,
                         currentEntry.attrVal,
                         attrCatEntry.attrType) <= 0) {

            indices[j++] = indexEntry;
            inserted = true;
        }

        indices[j++] = currentEntry;
    }

    // If the new entry belongs at the end
    if (!inserted) {
        indices[j++] = indexEntry;
    }

    // Case 1: Leaf has space
    if (n < MAX_KEYS_LEAF) {

        blockHeader.numEntries++;

        ret = leaf.setHeader(&blockHeader);

        if (ret != SUCCESS)
            return ret;

        for (i = 0; i < n + 1; i++) {

            ret = leaf.setEntry(&indices[i], i);

            if (ret != SUCCESS)
                return ret;
        }

        return SUCCESS;
    }

    // Case 2: Leaf is full; split it
    int newRightBlk = splitLeaf(blockNum, indices);

    if (newRightBlk == E_DISKFULL)
        return E_DISKFULL;

    // If the current leaf has a parent
    if (blockHeader.pblock != -1) {

        InternalEntry internalEntry;

        internalEntry.attrVal =
            indices[MIDDLE_INDEX_LEAF].attrVal;

        internalEntry.lChild = blockNum;
        internalEntry.rChild = newRightBlk;

        ret = insertIntoInternal(
            relId, attrName, blockHeader.pblock, internalEntry
        );

    } else {

        // The root leaf has split; create a new root
        ret = createNewRoot(
            relId,
            attrName,
            indices[MIDDLE_INDEX_LEAF].attrVal,
            blockNum,
            newRightBlk
        );
    }

    if (ret != SUCCESS)
        return ret;

    return SUCCESS;
}
int BPlusTree::splitLeaf(int leafBlockNum, Index indices[]) {

    std::cout<<"split leaf called";
    // Allocate the new right leaf block
    IndLeaf rightBlk;
    IndLeaf leftBlk(leafBlockNum);
    
    int rightBlkNum = rightBlk.getBlockNum();
    int leftBlkNum = leftBlk.getBlockNum();

    if (rightBlkNum == E_DISKFULL)
        return E_DISKFULL;

    // Get headers
    HeadInfo leftBlkHeader, rightBlkHeader;

    int ret = leftBlk.getHeader(&leftBlkHeader);
    if (ret != SUCCESS)
        return ret;

    ret = rightBlk.getHeader(&rightBlkHeader);
    if (ret != SUCCESS)
        return ret;

    // Configure the right block
    rightBlkHeader.numEntries = (MAX_KEYS_LEAF + 1) / 2;
    rightBlkHeader.pblock = leftBlkHeader.pblock;
    rightBlkHeader.lblock = leftBlkNum;
    rightBlkHeader.rblock = leftBlkHeader.rblock;

    ret = rightBlk.setHeader(&rightBlkHeader);
    if (ret != SUCCESS)
        return ret;

    // Configure the left block
    leftBlkHeader.numEntries = (MAX_KEYS_LEAF + 1) / 2;
    leftBlkHeader.rblock = rightBlkNum;

    ret = leftBlk.setHeader(&leftBlkHeader);
    if (ret != SUCCESS)
        return ret;

    // Copy the first half of entries to the left block
    for (int i = 0; i < (MAX_KEYS_LEAF + 1) / 2; i++) {

        ret = leftBlk.setEntry(&indices[i], i);
        if (ret != SUCCESS)
            return ret;
            
    }

    // Copy the second half of entries to the right block
    for (int i = 0; i < (MAX_KEYS_LEAF + 1) / 2; i++) {

        ret = rightBlk.setEntry(
            &indices[i + (MAX_KEYS_LEAF + 1) / 2], i
        );

        if (ret != SUCCESS)
            return ret;
    }
    return rightBlkNum;
}
int BPlusTree::insertIntoInternal(int relId, char attrName[ATTR_SIZE],
                                  int intBlockNum, InternalEntry intEntry) {

    // Get the attribute catalog entry
    AttrCatEntry attrCatEntry;

    int ret = AttrCacheTable::getAttrCatEntry(
        relId, attrName, &attrCatEntry
    );

    if (ret != SUCCESS)
        return ret;

    // Get the internal block
    IndInternal intBlk(intBlockNum);

    HeadInfo blockHeader;

    ret = intBlk.getHeader(&blockHeader);

    if (ret != SUCCESS)
        return ret;

    int n = blockHeader.numEntries;

    // Store existing entries and the new entry
    InternalEntry internalEntries[MAX_KEYS_INTERNAL + 1];

    int i = 0;
    int j = 0;
    bool inserted = false;

    for (i = 0; i < n; i++) {

        InternalEntry currentEntry;

        ret = intBlk.getEntry(&currentEntry, i);

        if (ret != SUCCESS)
            return ret;

        if (!inserted &&
            compareAttrs(intEntry.attrVal,
                         currentEntry.attrVal,
                         attrCatEntry.attrType) <= 0) {

            internalEntries[j++] = intEntry;
            inserted = true;
        }

        internalEntries[j++] = currentEntry;
    }

    // Insert at the end if no earlier position was found
    if (!inserted) {
        internalEntries[j++] = intEntry;
    }

    // Correct the left-child pointer of the following entry
    for (i = 0; i < n + 1; i++) {

        if (compareAttrs(internalEntries[i].attrVal,
                         intEntry.attrVal,
                         attrCatEntry.attrType) == 0 &&
            internalEntries[i].lChild == intEntry.lChild &&
            internalEntries[i].rChild == intEntry.rChild) {

            if (i + 1 < n + 1) {
                internalEntries[i + 1].lChild = intEntry.rChild;
            }

            break;
        }
    }

    // Case 1: The internal block has space
    if (n < MAX_KEYS_INTERNAL) {

        blockHeader.numEntries++;

        ret = intBlk.setHeader(&blockHeader);

        if (ret != SUCCESS)
            return ret;

        for (i = 0; i < n + 1; i++) {

            ret = intBlk.setEntry(&internalEntries[i], i);

            if (ret != SUCCESS)
                return ret;
        }

        return SUCCESS;
    }

    // Case 2: The internal block is full; split it
    int newRightBlk = splitInternal(intBlockNum, internalEntries);

    if (newRightBlk == E_DISKFULL) {

        // Destroy the subtree that has not been connected to the tree
        bPlusDestroy(intEntry.rChild);

        return E_DISKFULL;
    }

    int retVal;

    if (blockHeader.pblock != -1) {

        // Propagate the middle separator to the parent
        InternalEntry parentEntry;

        parentEntry.attrVal =
            internalEntries[MIDDLE_INDEX_INTERNAL].attrVal;

        parentEntry.lChild = intBlockNum;
        parentEntry.rChild = newRightBlk;

        retVal = insertIntoInternal(
            relId,
            attrName,
            blockHeader.pblock,
            parentEntry
        );

    } else {

        // The root has split; create a new root
        retVal = createNewRoot(
            relId,
            attrName,
            internalEntries[MIDDLE_INDEX_INTERNAL].attrVal,
            intBlockNum,
            newRightBlk
        );
    }

    if (retVal != SUCCESS)
        return retVal;

    return SUCCESS;
}
int BPlusTree::splitInternal(int intBlockNum,
                             InternalEntry internalEntries[]) {

    // Allocate the new right internal block
    IndInternal rightBlk;
    IndInternal leftBlk(intBlockNum);

    int rightBlkNum = rightBlk.getBlockNum();
    int leftBlkNum = leftBlk.getBlockNum();

    if (rightBlkNum == E_DISKFULL)
        return E_DISKFULL;

    // Get headers
    HeadInfo leftBlkHeader, rightBlkHeader;

    int ret = leftBlk.getHeader(&leftBlkHeader);
    if (ret != SUCCESS)
        return ret;

    ret = rightBlk.getHeader(&rightBlkHeader);
    if (ret != SUCCESS)
        return ret;

    // Configure the right block
    rightBlkHeader.numEntries = MAX_KEYS_INTERNAL / 2;
    rightBlkHeader.pblock = leftBlkHeader.pblock;

    ret = rightBlk.setHeader(&rightBlkHeader);
    if (ret != SUCCESS)
        return ret;

    // Configure the left block
    leftBlkHeader.numEntries = MAX_KEYS_INTERNAL / 2;

    ret = leftBlk.setHeader(&leftBlkHeader);
    if (ret != SUCCESS)
        return ret;

    // Copy entries 0-49 to the left block
    for (int i = 0; i < MAX_KEYS_INTERNAL / 2; i++) {

        ret = leftBlk.setEntry(&internalEntries[i], i);
        if (ret != SUCCESS)
            return ret;
    }

    // Copy entries 51-100 to the right block
    for (int i = 0; i < MAX_KEYS_INTERNAL / 2; i++) {

        ret = rightBlk.setEntry(
            &internalEntries[i + MIDDLE_INDEX_INTERNAL + 1], i
        );

        if (ret != SUCCESS)
            return ret;
    }

    // Determine the type of the child blocks
    int type = StaticBuffer::getStaticBlockType(
        internalEntries[MIDDLE_INDEX_INTERNAL + 1].lChild
    );

    // Update the parent pointer of every child belonging to the right block
    for (int i = MIDDLE_INDEX_INTERNAL + 1;
         i <= MAX_KEYS_INTERNAL;
         i++) {

        // The left child of the first entry in the right block
        // and the right child of every entry are the unique children.
        if (i == MIDDLE_INDEX_INTERNAL + 1) {

            int childBlockNum = internalEntries[i].lChild;

            BlockBuffer childBlock(childBlockNum);

            HeadInfo childHeader;

            ret = childBlock.getHeader(&childHeader);
            if (ret != SUCCESS)
                return ret;

            childHeader.pblock = rightBlkNum;

            ret = childBlock.setHeader(&childHeader);
            if (ret != SUCCESS)
                return ret;
        }

        int childBlockNum = internalEntries[i].rChild;

        BlockBuffer childBlock(childBlockNum);

        HeadInfo childHeader;

        ret = childBlock.getHeader(&childHeader);
        if (ret != SUCCESS)
            return ret;

        childHeader.pblock = rightBlkNum;

        ret = childBlock.setHeader(&childHeader);
        if (ret != SUCCESS)
            return ret;
    }

    return rightBlkNum;
}
int BPlusTree::createNewRoot(int relId, char attrName[ATTR_SIZE],
                             Attribute attrVal, int lChild, int rChild) {

    // Get the attribute catalog entry
    AttrCatEntry attrCatEntry;

    int ret = AttrCacheTable::getAttrCatEntry(
        relId, attrName, &attrCatEntry
    );

    if (ret != SUCCESS)
        return ret;

    // Allocate a new internal root block
    IndInternal newRootBlk;

    int newRootBlkNum = newRootBlk.getBlockNum();

    if (newRootBlkNum == E_DISKFULL) {

        // Destroy the disconnected right subtree
        bPlusDestroy(rChild);

        return E_DISKFULL;
    }

    // Set the new root header
    HeadInfo rootHeader;

    ret = newRootBlk.getHeader(&rootHeader);
    if (ret != SUCCESS)
        return ret;

    rootHeader.numEntries = 1;

    ret = newRootBlk.setHeader(&rootHeader);
    if (ret != SUCCESS)
        return ret;

    // Create the root entry
    InternalEntry rootEntry;

    rootEntry.lChild = lChild;
    rootEntry.attrVal = attrVal;
    rootEntry.rChild = rChild;

    ret = newRootBlk.setEntry(&rootEntry, 0);
    if (ret != SUCCESS)
        return ret;

    // Update the parent pointer of the left child
    BlockBuffer leftChild(lChild);

    HeadInfo leftHeader;

    ret = leftChild.getHeader(&leftHeader);
    if (ret != SUCCESS)
        return ret;

    leftHeader.pblock = newRootBlkNum;

    ret = leftChild.setHeader(&leftHeader);
    if (ret != SUCCESS)
        return ret;

    // Update the parent pointer of the right child
    BlockBuffer rightChild(rChild);

    HeadInfo rightHeader;

    ret = rightChild.getHeader(&rightHeader);
    if (ret != SUCCESS)
        return ret;

    rightHeader.pblock = newRootBlkNum;

    ret = rightChild.setHeader(&rightHeader);
    if (ret != SUCCESS)
        return ret;

    // Update the root block in the attribute catalog entry
    attrCatEntry.rootBlock = newRootBlkNum;

    ret = AttrCacheTable::setAttrCatEntry(
        relId, attrName, &attrCatEntry
    );

    if (ret != SUCCESS)
        return ret;

    return SUCCESS;
}

int BPlusTree::bPlusDestroy(int rootBlockNum) {

    // Check block number bounds
    if (rootBlockNum < 0 || rootBlockNum >= DISK_BLOCKS) {
        return E_OUTOFBOUND;
    }

    // Get the block type
    int type = StaticBuffer::getStaticBlockType(rootBlockNum);

    if (type == IND_LEAF) {

        // Construct the leaf block
        IndLeaf leaf(rootBlockNum);

        // Release the block
        leaf.releaseBlock();
        return SUCCESS;

    } else if (type == IND_INTERNAL) {

        // Construct the internal block
        IndInternal internalBlk(rootBlockNum);

        // Load the block header
        HeadInfo header;
        int ret = internalBlk.getHeader(&header);

        if (ret != SUCCESS)
            return ret;

        // Destroy children without deleting overlapping children twice
        for (int i = 0; i < header.numEntries; i++) {

            InternalEntry entry;

            ret = internalBlk.getEntry(&entry, i);

            if (ret != SUCCESS)
                return ret;

            // Destroy the left child of the first entry only
            if (i == 0) {
                ret = bPlusDestroy(entry.lChild);

                if (ret != SUCCESS)
                    return ret;
            }

            // Destroy the right child of every entry
            ret = bPlusDestroy(entry.rChild);

            if (ret != SUCCESS)
                return ret;
        }

        // Release the internal block after destroying its children
        internalBlk.releaseBlock();
        return SUCCESS;

    } else {

        // Block is not an index block
        return E_INVALIDBLOCK;
    }
}
