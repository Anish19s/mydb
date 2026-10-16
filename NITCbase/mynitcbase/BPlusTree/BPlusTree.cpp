#include "BPlusTree.h"
#include <cstring>

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
    return SUCCESS;
}

int BPlusTree::bPlusInsert(int relId, char attrName[ATTR_SIZE], union Attribute attrVal, RecId recordId) {
    return SUCCESS;
}

int BPlusTree::bPlusDestroy(int rootBlockNum) {
    return SUCCESS;
}
