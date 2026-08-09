#include "AttrCacheTable.h"

#include <cstring>
AttrCacheEntry* AttrCacheTable::attrCache[MAX_OPEN];

/* returns the attrOffset-th attribute for the relation corresponding to relId
NOTE: this function expects the caller to allocate memory for `*attrCatBuf`
*/
int AttrCacheTable::getAttrCatEntry(int relId, int attrOffset, AttrCatEntry* attrCatBuf) {

  // check if 0 <= relId < MAX_OPEN and return E_OUTOFBOUND otherwise
  if (relId < 0 || relId >= MAX_OPEN) {
    return E_OUTOFBOUND;
  }

  // check if attrCache[relId] == nullptr and return E_RELNOTOPEN if true
  if (attrCache[relId] == nullptr) {
    return E_RELNOTOPEN;
  }

  // traverse the linked list of attribute cache entries
  for (AttrCacheEntry* entry = attrCache[relId]; entry != nullptr; entry = entry->next) {

    if (entry->attrCatEntry.offset == attrOffset) {

      // copy entry->attrCatEntry to *attrCatBuf and return SUCCESS
      *attrCatBuf = entry->attrCatEntry;
      return SUCCESS;
    }
  }

  // there is no attribute at this offset
  return E_ATTRNOTEXIST;
}


/* Converts an attribute catalog record to AttrCatEntry struct
*/
void AttrCacheTable::recordToAttrCatEntry(union Attribute record[ATTRCAT_NO_ATTRS],
                                          AttrCatEntry* attrCatEntry) {

  strcpy(attrCatEntry->relName, record[ATTRCAT_REL_NAME_INDEX].sVal);

  strcpy(attrCatEntry->attrName, record[ATTRCAT_ATTR_NAME_INDEX].sVal);

  attrCatEntry->offset =
      (int)record[ATTRCAT_OFFSET_INDEX].nVal;

  attrCatEntry->attrType =
      (int)record[ATTRCAT_ATTR_TYPE_INDEX].nVal;

  attrCatEntry->primaryFlag =
      (int)record[ATTRCAT_PRIMARY_FLAG_INDEX].nVal;

  attrCatEntry->rootBlock =
      (int)record[ATTRCAT_ROOT_BLOCK_INDEX].nVal;
}
/*
returns the attribute with name `attrName` for the relation corresponding to relId

NOTE: this function expects the caller to allocate memory for *attrCatBuf
*/
int AttrCacheTable::getAttrCatEntry(
    int relId,
    char attrName[ATTR_SIZE],
    AttrCatEntry* attrCatBuf) {

    // check if relId is within valid range
    if (relId < 0 || relId >= MAX_OPEN)
        return E_OUTOFBOUND;

    // check if relation is open
    if (attrCache[relId] == nullptr)
        return E_RELNOTOPEN;

    // traverse the linked list of attribute cache entries
    AttrCacheEntry* curr = attrCache[relId];

    while (curr != nullptr) {

        // compare attribute names
        if (strcmp(curr->attrCatEntry.attrName, attrName) == 0) {

            // copy the attribute catalog entry
            *attrCatBuf = curr->attrCatEntry;
            return SUCCESS;
        }

        curr = curr->next;
    }

    // attribute not found
    return E_ATTRNOTEXIST;
}