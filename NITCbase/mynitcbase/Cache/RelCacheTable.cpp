#include "RelCacheTable.h"

#include <cstring>
RelCacheEntry* RelCacheTable::relCache[MAX_OPEN];

/*
Get the relation catalog entry for the relation with rel-id `relId` from the cache
NOTE: this function expects the caller to allocate memory for `*relCatBuf`
*/
int RelCacheTable::getRelCatEntry(int relId, RelCatEntry* relCatBuf) {
  if (relId < 0 || relId >= MAX_OPEN) {
    return E_OUTOFBOUND;
  }

  // if there's no entry at the rel-id
  if (relCache[relId] == nullptr) {
    return E_RELNOTOPEN;
  }

  // copy the value to the relCatBuf argument
  *relCatBuf = relCache[relId]->relCatEntry;

  return SUCCESS;
}

/* Converts a relation catalog record to RelCatEntry struct
    We get the record as Attribute[] from the BlockBuffer.getRecord() function.
    This function will convert that to a struct RelCatEntry type.
NOTE: this function expects the caller to allocate memory for `*relCatEntry`
*/
void RelCacheTable::recordToRelCatEntry(union Attribute record[RELCAT_NO_ATTRS],
                                        RelCatEntry* relCatEntry) {
  strcpy(relCatEntry->relName, record[RELCAT_REL_NAME_INDEX].sVal);
 relCatEntry->numAttrs =
      (int)record[RELCAT_NO_ATTRIBUTES_INDEX].nVal;

  relCatEntry->numRecs =
      (int)record[RELCAT_NO_RECORDS_INDEX].nVal;

  relCatEntry->firstBlk =
      (int)record[RELCAT_FIRST_BLOCK_INDEX].nVal;

  relCatEntry->lastBlk =
      (int)record[RELCAT_LAST_BLOCK_INDEX].nVal;

  relCatEntry->numSlotsPerBlk =
      (int)record[RELCAT_NO_SLOTS_PER_BLOCK_INDEX].nVal;
}
/*
will return the searchIndex for the relation corresponding to relId

NOTE: this function expects the caller to allocate memory for *searchIndex
*/
int RelCacheTable::getSearchIndex(int relId, RecId* searchIndex) {

    // check if relId is within valid range
    if (relId < 0 || relId >= MAX_OPEN)
        return E_OUTOFBOUND;

    // check if relation is open
    if (relCache[relId] == nullptr)
        return E_RELNOTOPEN;

    // copy searchIndex of relation cache entry
    *searchIndex = relCache[relId]->searchIndex;

    return SUCCESS;
}


// sets the searchIndex for the relation corresponding to relId
int RelCacheTable::setSearchIndex(int relId, RecId* searchIndex) {

    // check if relId is within valid range
    if (relId < 0 || relId >= MAX_OPEN)
        return E_OUTOFBOUND;

    // check if relation is open
    if (relCache[relId] == nullptr)
        return E_RELNOTOPEN;

    // update searchIndex
    relCache[relId]->searchIndex = *searchIndex;

    return SUCCESS;
}


int RelCacheTable::resetSearchIndex(int relId) {

    RecId searchIndex;

    // set search index to invalid record {-1,-1}
    searchIndex.block = -1;
    searchIndex.slot = -1;

    return setSearchIndex(relId, &searchIndex);
}
int RelCacheTable::setRelCatEntry(int relId, RelCatEntry *relCatBuf) {

    // Check if relId is outside valid range
    if (relId < 0 || relId >= MAX_OPEN) {
        return E_OUTOFBOUND;
    }

    // Check if the relation cache entry is free
    if (relCache[relId] == nullptr) {
        return E_RELNOTOPEN;
    }

    // Copy the relation catalog entry
    relCache[relId]->relCatEntry = *relCatBuf;

    // Mark the cache entry as dirty
    relCache[relId]->dirty = true;

    return SUCCESS;
}
void RelCacheTable::relCatEntryToRecord(
    RelCatEntry *relCatBuf,
    union Attribute record[RELCAT_NO_ATTRS]) {

    strcpy(record[0].sVal, relCatBuf->relName);

    record[1].nVal = relCatBuf->numAttrs;
    record[2].nVal = relCatBuf->numRecs;
    record[3].nVal = relCatBuf->firstBlk;
    record[4].nVal = relCatBuf->lastBlk;
    record[5].nVal = relCatBuf->numSlotsPerBlk;
}