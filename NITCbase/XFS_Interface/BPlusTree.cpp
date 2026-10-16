#include <cstring>
#include <cstdio>
#include <queue>
#include <iostream>
#include "BPlusTree.h"
#include "define/constants.h"
#include "define/errors.h"
#include "disk_structures.h"
#include "block_access.h"

using namespace std;

BPlusTree::BPlusTree(int relId, char attrName[ATTR_SIZE]) {
	// initialise object instance member fields
	this->relId = relId;
	strcpy(this->attrName, attrName);

	// get the attribute catalog entry of target attribute
	Attribute attrCatEntry[6];
	int flag = getAttrCatEntry(relId, attrName, attrCatEntry);
	// in case attribute does not exist
	if (flag != SUCCESS) {
		this->rootBlock = flag;
		return;
	}

	// check if an index already exists for the attribute or not
	if (attrCatEntry[ATTRCAT_ROOT_BLOCK_INDEX].nval != -1) {
		this->rootBlock = (int) attrCatEntry[ATTRCAT_ROOT_BLOCK_INDEX].nval;
		return;
	}

	// CREATING A NEW B+ TREE

	// getting free block as leaf index/root block
	int root_block = getFreeBlock(IND_LEAF);
	if (root_block == FAILURE) {   //Problem in getting free leaf index block
		this->rootBlock = E_DISKFULL;
		return;
	}

	this->rootBlock = root_block;

	// set header for root block
	HeadInfo headInfo;
	headInfo.blockType = IND_LEAF;
	headInfo.pblock = -1;
	headInfo.lblock = -1;
	headInfo.rblock = -1;
	headInfo.numEntries = 0;
	setHeader(&headInfo, root_block);

	// update AttrCatEntry with root block
	attrCatEntry[ATTRCAT_ROOT_BLOCK_INDEX].nval = root_block;
	if (setAttrCatEntry(relId, attrName, attrCatEntry) != SUCCESS) {
		this->rootBlock = FAILURE;
		return;
	}

	// get num of attrs, record block number from relCatEntry
	Attribute relCatEntry[6];
	if (getRelCatEntry(relId, relCatEntry) != SUCCESS) {
		this->rootBlock = FAILURE;
		return;
	}
	int dataBlock, numAttrs;
	dataBlock = (int) relCatEntry[RELCAT_FIRST_BLOCK_INDEX].nval; //first record block for the relation
	numAttrs = (int) relCatEntry[RELCAT_NO_ATTRIBUTES_INDEX].nval; //num of attributes for the relation

	int attrOffset, attrType;
	attrOffset = (int) attrCatEntry[ATTRCAT_OFFSET_INDEX].nval;
	attrType = (int) attrCatEntry[ATTRCAT_ATTR_TYPE_INDEX].nval;

	Attribute record[numAttrs];

	// inserting index entries for each record in bplus tree
	while (dataBlock != -1) {
		// get header of record block
		HeadInfo header;
		header = getHeader(dataBlock);

		int num_records = header.numEntries;
		int num_slots = header.numSlots;
		unsigned char slotmap[num_slots];
		getSlotmap(slotmap, dataBlock);

		int iter;
		for (iter = 0; iter < num_records; iter++) {

			// get iter th number record from data block
			getRecord(record, dataBlock, iter);

			// get attribute value
			Attribute attrval;
			if (attrType == NUMBER) {
				attrval.nval = record[attrOffset].nval;
			} else if (attrType == STRING) {
				strcpy(attrval.sval, record[attrOffset].sval);
			}

			recId rec_id;
			rec_id.block = dataBlock;
			rec_id.slot = iter;

			int res = bPlusInsert(attrval, rec_id);

			if (res != SUCCESS) {
				bPlusDestroy(root_block);
				this->rootBlock = res;
				return;
			}
		}
		dataBlock = header.rblock; //next data block for the relation
	}
}

int BPlusTree::bPlusInsert(Attribute val, recId recordId) {

	// get attribute catalog entry of target attribute
	Attribute attrCatEntry[6];
	int flag = getAttrCatEntry(relId, attrName, attrCatEntry);
	if (flag != SUCCESS) {
		this->rootBlock = flag;
		return flag;
	}

	//check if B+ Tree exists for attr
	int blockNum = this->rootBlock;
	if (blockNum == -1) {
		this->rootBlock = E_NOINDEX;
		return E_NOINDEX;
	}

	int attrType = (int) attrCatEntry[ATTRCAT_ATTR_TYPE_INDEX].nval;

	int blockType = getBlockType(blockNum);
	HeadInfo blockHeader;
	int num_of_entries, current_entryNumber;

	/******Traverse the B+ Tree to reach the appropriate leaf where insertion can be done******/
	while (blockType != IND_LEAF) {
		blockHeader = getHeader(blockNum);
		num_of_entries = blockHeader.numEntries;
		InternalEntry internalEntry;
		for (current_entryNumber = 0; current_entryNumber < num_of_entries; ++current_entryNumber) {
			internalEntry = getInternalEntry(blockNum, current_entryNumber);
			if (compareAttributes(val, internalEntry.attrVal, attrType) <= 0)
				break;
		}

		if (current_entryNumber == num_of_entries) {
			blockNum = internalEntry.rChild;
		} else {
			blockNum = internalEntry.lChild;
		}
		blockType = getBlockType(blockNum);
	}

	// NOTE : blockNum is the leaf index block to which insertion of val is to be done

	/******Insertion of entry in the appropriate leaf block******/
	blockHeader = getHeader(blockNum);
	num_of_entries = blockHeader.numEntries;

	Index indices[num_of_entries + 1];
	Index current_leafEntry;
	int current_leafEntryIndex = 0;
	flag = 0;
	/* iterate through all the entries in the block and copy them to the array indices
	 * Also insert val at appropriate position in the indices array
	*/
	for (current_entryNumber = 0; current_entryNumber < num_of_entries; ++current_entryNumber) {
		current_leafEntry = getLeafEntry(blockNum, current_entryNumber);

		if (flag == 0) {
			if (compareAttributes(val, current_leafEntry.attrVal, attrType) < 0) {
				if (attrType == NUMBER) {
					indices[current_leafEntryIndex].attrVal.nval = val.nval;
				} else if (attrType == STRING) {
					strcpy(indices[current_leafEntryIndex].attrVal.sval, val.sval);
				}
				indices[current_leafEntryIndex].block = recordId.block;
				indices[current_leafEntryIndex].slot = recordId.slot;
				flag = 1;
				current_leafEntryIndex++;
			}
		}
		if (attrType == NUMBER) {
			indices[current_leafEntryIndex].attrVal.nval = current_leafEntry.attrVal.nval;
		} else if (attrType == STRING) {
			strcpy(indices[current_leafEntryIndex].attrVal.sval, current_leafEntry.attrVal.sval);
		}
		indices[current_leafEntryIndex].block = current_leafEntry.block;
		indices[current_leafEntryIndex].slot = current_leafEntry.slot;
		current_leafEntryIndex++;
	}

	if (num_of_entries == current_leafEntryIndex) {
		if (attrType == NUMBER)
			indices[current_leafEntryIndex].attrVal.nval = val.nval;
		if (attrType == STRING)
			strcpy(indices[current_leafEntryIndex].attrVal.sval, val.sval);
		indices[current_leafEntryIndex].block = recordId.block;
		indices[current_leafEntryIndex].slot = recordId.slot;
		current_leafEntryIndex++;
	}

	//leaf block has not reached max limit
	if (num_of_entries != MAX_KEYS_LEAF) {

		// increment blockHeader.numEntries and set this as header of block
		blockHeader.numEntries = blockHeader.numEntries + 1;
		setHeader(&blockHeader, blockNum);

		// iterate through all the entries of indices array and populate the entries of block with them
		for (int i = 0; i < current_leafEntryIndex; ++i) {
			setLeafEntry(indices[i], blockNum, i);
		}

		return SUCCESS;
	} else { //leaf block is full- need a new leaf to make the entry; split the entries between the two blocks.

		//assign the existing block as the left block in the splitting.
		int leftBlkNum = blockNum;
		// obtain new leaf index block to be used as the right block in the splitting
		int newRightBlkNum = getFreeBlock(IND_LEAF);

		if (newRightBlkNum == FAILURE) {
			//failed to obtain an empty leaf index because the disk is full

			//destroy the existing B+ tree by passing rootBlock member field to bPlusDestroy().
			bPlusDestroy(this->rootBlock);

			//update the rootBlock of attribute catalog entry to -1
			attrCatEntry[ATTRCAT_ROOT_BLOCK_INDEX].nval = -1;
			setAttrCatEntry(relId, attrName, attrCatEntry);

			this->rootBlock = E_DISKFULL;
			return E_DISKFULL;
		}

		// let leftBlkHeader be the header of the left block(which is presently stored in blockHeader)
		struct HeadInfo leftBlkHeader = blockHeader;

		//store the block after leftBlk that appears in the linked list in prevRblock
		int prevRblock = leftBlkHeader.rblock;

		/* Update left block header
		 * - number of entries = 32
		 * - right block = newRightBlkNum
		 */
		leftBlkHeader.numEntries = MIDDLE_INDEX_LEAF + 1;
		leftBlkHeader.rblock = newRightBlkNum;
		setHeader(&leftBlkHeader, leftBlkNum);

		//load the header of newRightBlk in newRightBlkHeader using BlockBuffer::getHeader()
		HeadInfo newRightBlkHeader = getHeader(newRightBlkNum);
		/* Update right block header
		 * - number of entries = 32
		 * - left block = leftBlkNum
		 * - right block = prevRblock
		 * - parent block = parent block of leftBlkNum
		 */
		newRightBlkHeader.blockType = IND_LEAF;
		newRightBlkHeader.numEntries = MIDDLE_INDEX_LEAF + 1;
		newRightBlkHeader.lblock = leftBlkNum;
		newRightBlkHeader.pblock = leftBlkHeader.pblock;
		newRightBlkHeader.rblock = prevRblock;
		setHeader(&newRightBlkHeader, newRightBlkNum);

		// TODO ****** ADD IN ALGO
		if (prevRblock != -1) {
			HeadInfo prevRBlockHeader = getHeader(prevRblock);
			prevRBlockHeader.lblock = newRightBlkNum;
			setHeader(&prevRBlockHeader, prevRblock);
		}

		//store pblock of leftBlk in parBlkNum.
		int parentBlock = leftBlkHeader.pblock;

		// set the first 32 entries of leftBlk as the first 32 entries of indices array
		int indices_iter;
		for (indices_iter = 0; indices_iter <= MIDDLE_INDEX_LEAF; indices_iter++) {
			setLeafEntry(indices[indices_iter], leftBlkNum, indices_iter);
		}
		// set the first 32 entries of newRightBlk as the next 32 entries of indices array
		for (int rBlockIndexIter = 0; rBlockIndexIter <= MIDDLE_INDEX_LEAF; rBlockIndexIter++) {
			setLeafEntry(indices[indices_iter], newRightBlkNum, rBlockIndexIter);
			indices_iter++;
		}
		//********REPLACE
		indices[0].attrVal.nval = 0;
		indices[0].block = 0;
		indices[0].slot = 0;
		for (indices_iter = MIDDLE_INDEX_LEAF + 1; indices_iter < MAX_KEYS_LEAF; indices_iter++) {
			setLeafEntry(indices[0], leftBlkNum, indices_iter);
		}

		/*
		 * store the attribute value of indices[31] in newAttrVal;
		 * this is attribute value which needs to be inserted in the parent block
		 */
		Index leafentry;
		leafentry = getLeafEntry(leftBlkNum, MIDDLE_INDEX_LEAF);
		Attribute newAttrVal;

		if (attrType == NUMBER)
			newAttrVal.nval = leafentry.attrVal.nval;
		if (attrType == STRING)
			strcpy(newAttrVal.sval, leafentry.attrVal.sval);

		bool done = false;

		/******Traverse the internal index blocks of the B+ Tree bottom up making insertions wherever required******/
		//let done indicate whether the insertion is complete or not
		while (!done) {
			if (parentBlock != -1) {
				HeadInfo parentHeader = getHeader(parentBlock);
				InternalEntry internal_entries[parentHeader.numEntries + 1];
				InternalEntry internalEntry;
				flag = 0;
				current_entryNumber = 0;

				/* iterate through all the entries of the parentBlock and copy them to the array internal_entries.
				 * Also insert an InternalEntry entry with attrVal as newAttrval, lChild as leftBlkNum,
				 * and rChild as newRightBlkNum at an appropriate position in the internalEntries array.
				 */
				for (indices_iter = 0; indices_iter < parentHeader.numEntries; ++indices_iter) {
					internalEntry = getInternalEntry(parentBlock, indices_iter);

					// inserting newAttrVal at appropriate place
					if (flag == 0) {
//						if (compareAttributes(newAttrVal, internalEntry.attrVal, attrType) <= 0) {
						if (internalEntry.lChild == leftBlkNum) {
							if (attrType == NUMBER) {
								internal_entries[current_entryNumber].attrVal.nval = newAttrVal.nval;
							} else if (attrType == STRING) {
								strcpy(internal_entries[current_entryNumber].attrVal.sval, newAttrVal.sval);
							}
							internal_entries[current_entryNumber].lChild = leftBlkNum;
							internal_entries[current_entryNumber].rChild = newRightBlkNum;
							flag = 1;
							current_entryNumber++;
						}
					}

					// copy entries of the parentBlock to the array internal_entries
					if (attrType == NUMBER) {
						internal_entries[current_entryNumber].attrVal.nval = internalEntry.attrVal.nval;
					} else if (attrType == STRING) {
						strcpy(internal_entries[current_entryNumber].attrVal.sval, internalEntry.attrVal.sval);
					}
					if (current_entryNumber - 1 >= 0) {
						internal_entries[current_entryNumber].lChild = internal_entries[current_entryNumber - 1].rChild;
					} else {
						internal_entries[current_entryNumber].lChild = internalEntry.lChild;
					}
					internal_entries[current_entryNumber].rChild = internalEntry.rChild;
					current_entryNumber++;
				}
				// TODO : review
				if (flag == 0) //when newattrval is greater than all parentblock enries
				{
					if (attrType == NUMBER) {
						internal_entries[current_entryNumber].attrVal.nval = newAttrVal.nval;
					} else if (attrType == STRING) {
						strcpy(internal_entries[current_entryNumber].attrVal.sval, newAttrVal.sval);
					}
					internal_entries[current_entryNumber].lChild = leftBlkNum;
					internal_entries[current_entryNumber].rChild = newRightBlkNum;
				}

				// parentBlock has not reached max limit.
				if (parentHeader.numEntries != MAX_KEYS_INTERNAL) {
					// increment parheader.numEntries and update it as header of parblk
					parentHeader.numEntries = parentHeader.numEntries + 1;
					setHeader(&parentHeader, parentBlock);

					// iterate through all entries in internalEntries array and populate the entries of parentBlock with them
					for (indices_iter = 0; indices_iter < parentHeader.numEntries; ++indices_iter) {
						setInternalEntry(internal_entries[indices_iter], parentBlock, indices_iter);
					}

					done = true;
				} else //if parent block is full
				{
					int newBlock = getFreeBlock(IND_INTERNAL); // to get a new internalblock

					// disk full
					if (newBlock == FAILURE) {
						// destroy the right subtree, given by newRightBlkNum, build up till now that has not yet been connected to the existing B+ Tree
						bPlusDestroy(newRightBlkNum);
						// destroy the existing B+ tree by passing rootBlock member field
						bPlusDestroy(this->rootBlock);
						// update the rootBlock of attribute catalog entry to -1
						attrCatEntry[ATTRCAT_ROOT_BLOCK_INDEX].nval = -1;
						setAttrCatEntry(relId, attrName, attrCatEntry);
						this->rootBlock = E_DISKFULL;
						return E_DISKFULL;
					}

					//assign new block as the right block in the splitting.
					newRightBlkNum = newBlock;
					//assign parentBlock as the left block in the splitting.
					leftBlkNum = parentBlock;

					//update leftBlkHeader with parheader.
					leftBlkHeader = parentHeader;

					/* Update left block header
					   * - number of entries = 50
					   */
					leftBlkHeader.numEntries = MIDDLE_INDEX_INTERNAL;
					setHeader(&leftBlkHeader, leftBlkNum);

					//load newRightBlkHeader
					newRightBlkHeader = getHeader(newRightBlkNum);

					/* Update right block header
					   * - number of entries = 50
					   * - parent block = parent block of leftBlkNum
					   */
					newRightBlkHeader.blockType = IND_INTERNAL;
					newRightBlkHeader.numEntries = MIDDLE_INDEX_INTERNAL;
					newRightBlkHeader.pblock = leftBlkHeader.pblock;
					setHeader(&newRightBlkHeader, newRightBlkNum);

					// set the first 50 entries of leftBlk as the first 50 entries of internalEntries array
					for (indices_iter = 0; indices_iter < MIDDLE_INDEX_INTERNAL; ++indices_iter) {
						// TODO ::: REVIEW ::::
//						if ((internal_entries[indices_iter].lChild == parentBlock) ||
//						    (internal_entries[indices_iter].rChild == parentBlock)) {
//							InternalEntry entry;
//							entry = getInternalEntry(parentBlock, indices_iter);
//							if (internalEntry.attrVal.nval == entry.attrVal.nval)
//								continue;
//							if (internal_entries[indices_iter].lChild == parentBlock)
//								internal_entries[indices_iter].lChild = entry.lChild;
//							else
//								internal_entries[indices_iter].rChild = entry.rChild;
//						}
						// ---------
						setInternalEntry(internal_entries[indices_iter], leftBlkNum, indices_iter);
					}

					indices_iter = MIDDLE_INDEX_INTERNAL + 1;
					// set the first 50 entries of newRightBlk as the entries from 51 to 100 of internalEntries array
					for (int j = 0; j < MIDDLE_INDEX_INTERNAL; ++j) {
						setInternalEntry(internal_entries[indices_iter], newRightBlkNum, j);
						indices_iter++;
					}

					int childNum;
					//iterate from 50 to 100:
					for (int k = MIDDLE_INDEX_INTERNAL; k <= MAX_KEYS_INTERNAL; ++k) {
						//assign the rchild block of ith index in internalEntries to childNum
						childNum = internal_entries[k].rChild;

						// update pblock of the child block to newRightBlkNum
						HeadInfo childHeader = getHeader(childNum);
						childHeader.pblock = newRightBlkNum;
						setHeader(&childHeader, childNum);
					}

					//update parBlkNum as the pblock of leftBlk.
					parentBlock = leftBlkHeader.pblock;

					/* update newAttrval to the attribute value of 50th entry in the internalEntries array;
					 * this is attribute value which needs to be inserted in the parent block.
					 */
					newAttrVal = internal_entries[MIDDLE_INDEX_INTERNAL].attrVal;

				}
			} else //if parent == -1 i.e root is split now
			{
				int new_root_block = getFreeBlock(IND_INTERNAL);//get new internal block
				// disk full
				if (new_root_block == FAILURE) {
					// destroy the right subtree, given by newRightBlkNum, build up till now that has not yet been connected to the existing B+ Tree
					bPlusDestroy(newRightBlkNum);
					// destroy the existing B+ tree by passing rootBlock member field
					bPlusDestroy(this->rootBlock);
					// update the rootBlock of attribute catalog entry to -1
					attrCatEntry[ATTRCAT_ROOT_BLOCK_INDEX].nval = -1;
					setAttrCatEntry(relId, attrName, attrCatEntry);
					this->rootBlock = E_DISKFULL;
					return E_DISKFULL;
				}

				/* add the struct InternalEntry entry with
				 *     lChild as leftBlkNum, attrVal as newAttrval, and rChild as newRightBlkNum
				 * as the first entry to new_root_block
				*/
				InternalEntry rootEntry;
				if (attrType == NUMBER)
					rootEntry.attrVal.nval = newAttrVal.nval;
				else if (attrType == STRING)
					strcpy(rootEntry.attrVal.sval, newAttrVal.sval);
				rootEntry.lChild = leftBlkNum;
				rootEntry.rChild = newRightBlkNum;
				setInternalEntry(rootEntry, new_root_block, 0);

				//update number of entries in newRootBlk as 1
				HeadInfo newRootHeader;
				HeadInfo header1 = getHeader(leftBlkNum);
				HeadInfo header2 = getHeader(newRightBlkNum);
				newRootHeader.numEntries = 1;
				newRootHeader.blockType = IND_INTERNAL;
				newRootHeader.pblock = -1;
				header1.pblock = new_root_block;
				header2.pblock = new_root_block;
				setHeader(&newRootHeader, new_root_block);
				setHeader(&header1, leftBlkNum);
				setHeader(&header2, newRightBlkNum);

				attrCatEntry[ATTRCAT_ROOT_BLOCK_INDEX].nval = new_root_block;
				setAttrCatEntry(relId, attrName, attrCatEntry);

				this->rootBlock = new_root_block;
				done = true;
			}
		}
	}
	return SUCCESS;
}
RecId BPlusTree::bPlusSearch(int relId, char attrName[ATTR_SIZE],
                             Attribute attrVal, int op) {

    IndexId searchIndex;
    int ret = AttrCacheTable::getSearchIndex(relId, attrName, &searchIndex);

    if (ret != SUCCESS)
        return RecId{-1, -1};

    AttrCatEntry attrCatEntry;
    ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);

    if (ret != SUCCESS)
        return RecId{-1, -1};

    int block, index;

    if (searchIndex.block == -1 && searchIndex.index == -1) {
        block = attrCatEntry.rootBlock;
        index = 0;

        if (block == -1)
            return RecId{-1, -1};
    } else {
        block = searchIndex.block;
        index = searchIndex.index + 1;

        IndLeaf leaf(block);
        HeadInfo leafHead;

        ret = leaf.getHeader(&leafHead);
        if (ret != SUCCESS)
            return RecId{-1, -1};

        if (index >= leafHead.numEntries) {
            block = leafHead.rblock;
            index = 0;

            if (block == -1)
                return RecId{-1, -1};
        }
    }

    while (StaticBuffer::getStaticBlockType(block) == IND_INTERNAL) {
        IndInternal internalBlk(block);
        HeadInfo intHead;

        ret = internalBlk.getHeader(&intHead);
        if (ret != SUCCESS)
            return RecId{-1, -1};

        InternalEntry intEntry;
        bool found = false;

        if (op == NE || op == LT || op == LE) {
            ret = internalBlk.getEntry(&intEntry, 0);
            if (ret != SUCCESS)
                return RecId{-1, -1};

            block = intEntry.lChild;
        } else {
            for (int i = 0; i < intHead.numEntries; i++) {
                ret = internalBlk.getEntry(&intEntry, i);
                if (ret != SUCCESS)
                    return RecId{-1, -1};

                int cmpVal = compareAttrs(
                    intEntry.attrVal,
                    attrVal,
                    attrCatEntry.attrType
                );

                if ((op == EQ || op == GE) && cmpVal >= 0) {
                    block = intEntry.lChild;
                    found = true;
                    break;
                }

                if (op == GT && cmpVal > 0) {
                    block = intEntry.lChild;
                    found = true;
                    break;
                }
            }

            if (!found) {
                ret = internalBlk.getEntry(
                    &intEntry, intHead.numEntries - 1
                );

                if (ret != SUCCESS)
                    return RecId{-1, -1};

                block = intEntry.rChild;
            }
        }
    }

    while (block != -1) {
        IndLeaf leafBlk(block);
        HeadInfo leafHead;

        ret = leafBlk.getHeader(&leafHead);
        if (ret != SUCCESS)
            return RecId{-1, -1};

        Index leafEntry;

        while (index < leafHead.numEntries) {
            ret = leafBlk.getEntry(&leafEntry, index);
            if (ret != SUCCESS)
                return RecId{-1, -1};

            int cmpVal = compareAttrs(
                leafEntry.attrVal,
                attrVal,
                attrCatEntry.attrType
            );

            if ((op == EQ && cmpVal == 0) ||
                (op == LE && cmpVal <= 0) ||
                (op == LT && cmpVal < 0) ||
                (op == GT && cmpVal > 0) ||
                (op == GE && cmpVal >= 0) ||
                (op == NE && cmpVal != 0)) {

                IndexId newSearchIndex{block, index};

                AttrCacheTable::setSearchIndex(
                    relId, attrName, &newSearchIndex
                );

                return RecId{leafEntry.block, leafEntry.slot};
            }

            if ((op == EQ || op == LE || op == LT) && cmpVal > 0)
                return RecId{-1, -1};

            index++;
        }

        if (op != NE)
            break;

        block = leafHead.rblock;
        index = 0;
    }

    return RecId{-1, -1};
}
int BPlusTree::getRootBlock() {
	return this->rootBlock;
}

int BPlusTree::bPlusDestroy(int blockNum) {
	HeadInfo header;

	// if the block_num lies outside valid range
	if (blockNum < 0 || blockNum >= DISK_BLOCKS) {
		return E_OUTOFBOUND;
	}
	header = getHeader(blockNum);
	int block_type = getBlockType(blockNum);

	if (block_type == IND_INTERNAL) {
		// if block is internal node remove all children before removing it
		int num_entries;
		num_entries = header.numEntries;

		/*
		 * iterate through all the entries of the internalBlk and
		 * destroy the lChild of the first entry and
		 * rChild of all entries using BPlusTree::bPlusDestroy().
		 * (take care not to delete overlapping children more than once)
		 */
		int iter = 0;
		InternalEntry internal_entry = getInternalEntry(blockNum, iter);
		bPlusDestroy(internal_entry.lChild);
		for (iter = 0; iter < num_entries; iter++) {
			// get the internal index block entries
			internal_entry = getInternalEntry(blockNum, iter);
			bPlusDestroy(internal_entry.rChild);
		}
		deleteBlock(blockNum);
	} else if (block_type == IND_LEAF) {
		deleteBlock(blockNum);
	} else {
		//if the block is not index block
		return E_INVALIDBLOCK;
	}
	return SUCCESS;
}