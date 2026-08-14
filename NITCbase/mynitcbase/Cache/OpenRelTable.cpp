#include "OpenRelTable.h"

#include <cstring>
#include <cstdlib>

OpenRelTableMetaInfo OpenRelTable::tableMetaInfo[MAX_OPEN];


OpenRelTable::OpenRelTable() {

    // Initialize all entries
    for (int i = 0; i < MAX_OPEN; i++) {
        tableMetaInfo[i].free = true;
        tableMetaInfo[i].relName[0] = '\0';

        RelCacheTable::relCache[i] = nullptr;
        AttrCacheTable::attrCache[i] = nullptr;
    }


    /************ Setting up Relation Cache entries ************/

    RecBuffer relCatBlock(RELCAT_BLOCK);

    Attribute relCatRecord[RELCAT_NO_ATTRS];


    // -------- Relation Catalog --------

    relCatBlock.getRecord(
        relCatRecord,
        RELCAT_SLOTNUM_FOR_RELCAT
    );

    RelCacheEntry relCacheEntry;

    RelCacheTable::recordToRelCatEntry(
        relCatRecord,
        &relCacheEntry.relCatEntry
    );

    relCacheEntry.dirty = false;

    relCacheEntry.recId.block = RELCAT_BLOCK;
    relCacheEntry.recId.slot = RELCAT_SLOTNUM_FOR_RELCAT;

    relCacheEntry.searchIndex.block = -1;
    relCacheEntry.searchIndex.slot = -1;

    RelCacheTable::relCache[RELCAT_RELID] =
        (RelCacheEntry *)malloc(sizeof(RelCacheEntry));

    *(RelCacheTable::relCache[RELCAT_RELID]) =
        relCacheEntry;


    // -------- Attribute Catalog --------

    relCatBlock.getRecord(
        relCatRecord,
        RELCAT_SLOTNUM_FOR_ATTRCAT
    );

    RelCacheEntry attrRelCacheEntry;

    RelCacheTable::recordToRelCatEntry(
        relCatRecord,
        &attrRelCacheEntry.relCatEntry
    );

    attrRelCacheEntry.dirty = false;

    attrRelCacheEntry.recId.block = RELCAT_BLOCK;
    attrRelCacheEntry.recId.slot = RELCAT_SLOTNUM_FOR_ATTRCAT;

    attrRelCacheEntry.searchIndex.block = -1;
    attrRelCacheEntry.searchIndex.slot = -1;

    RelCacheTable::relCache[ATTRCAT_RELID] =
        (RelCacheEntry *)malloc(sizeof(RelCacheEntry));

    *(RelCacheTable::relCache[ATTRCAT_RELID]) =
        attrRelCacheEntry;


    /************ Setting up Attribute Cache entries ************/

    RecBuffer attrCatBlock(ATTRCAT_BLOCK);

    Attribute attrCatRecord[ATTRCAT_NO_ATTRS];


    // -------- Relation Catalog attributes --------

    AttrCacheEntry *relAttrHead = nullptr;
    AttrCacheEntry *prev = nullptr;

    for (int i = 0; i < RELCAT_NO_ATTRS; i++) {

        attrCatBlock.getRecord(
            attrCatRecord,
            i
        );

        AttrCacheEntry *entry =
            (AttrCacheEntry *)malloc(sizeof(AttrCacheEntry));

        AttrCacheTable::recordToAttrCatEntry(
            attrCatRecord,
            &entry->attrCatEntry
        );

        entry->dirty = false;

        entry->recId.block = ATTRCAT_BLOCK;
        entry->recId.slot = i;

        entry->searchIndex.block = -1;
        entry->searchIndex.index = -1;

        entry->next = nullptr;

        if (relAttrHead == nullptr)
            relAttrHead = entry;
        else
            prev->next = entry;

        prev = entry;
    }

    AttrCacheTable::attrCache[RELCAT_RELID] =
        relAttrHead;


    // -------- Attribute Catalog attributes --------

    AttrCacheEntry *attrAttrHead = nullptr;
    prev = nullptr;

    for (int i = 0; i < ATTRCAT_NO_ATTRS; i++) {

        attrCatBlock.getRecord(
            attrCatRecord,
            RELCAT_NO_ATTRS + i
        );

        AttrCacheEntry *entry =
            (AttrCacheEntry *)malloc(sizeof(AttrCacheEntry));

        AttrCacheTable::recordToAttrCatEntry(
            attrCatRecord,
            &entry->attrCatEntry
        );

        entry->dirty = false;

        entry->recId.block = ATTRCAT_BLOCK;
        entry->recId.slot = RELCAT_NO_ATTRS + i;

        entry->searchIndex.block = -1;
        entry->searchIndex.index = -1;

        entry->next = nullptr;

        if (attrAttrHead == nullptr)
            attrAttrHead = entry;
        else
            prev->next = entry;

        prev = entry;
    }

    AttrCacheTable::attrCache[ATTRCAT_RELID] =
        attrAttrHead;


    /************ Setting up tableMetaInfo ************/

    tableMetaInfo[RELCAT_RELID].free = false;

    strcpy(
        tableMetaInfo[RELCAT_RELID].relName,
        RELCAT_RELNAME
    );


    tableMetaInfo[ATTRCAT_RELID].free = false;

    strcpy(
        tableMetaInfo[ATTRCAT_RELID].relName,
        ATTRCAT_RELNAME
    );
}


/**************************************************************/


OpenRelTable::~OpenRelTable() {

    // Close all open user relations
    for (int i = 2; i < MAX_OPEN; i++) {

        if (!tableMetaInfo[i].free) {
            OpenRelTable::closeRel(i);
        }
    }


    // Free relation cache entries for catalogs

    if (RelCacheTable::relCache[RELCAT_RELID] != nullptr) {
        free(RelCacheTable::relCache[RELCAT_RELID]);
        RelCacheTable::relCache[RELCAT_RELID] = nullptr;
    }

    if (RelCacheTable::relCache[ATTRCAT_RELID] != nullptr) {
        free(RelCacheTable::relCache[ATTRCAT_RELID]);
        RelCacheTable::relCache[ATTRCAT_RELID] = nullptr;
    }


    // Free attribute cache lists for catalogs

    for (int i = 0; i < 2; i++) {

        AttrCacheEntry *entry =
            AttrCacheTable::attrCache[i];

        while (entry != nullptr) {

            AttrCacheEntry *temp = entry;

            entry = entry->next;

            free(temp);
        }

        AttrCacheTable::attrCache[i] = nullptr;
    }
}


/**************************************************************/


int OpenRelTable::getFreeOpenRelTableEntry() {

    for (int i = 2; i < MAX_OPEN; i++) {

        if (tableMetaInfo[i].free) {
            return i;
        }
    }

    return E_CACHEFULL;
}


/**************************************************************/


int OpenRelTable::getRelId(char relName[ATTR_SIZE]) {

    for (int i = 0; i < MAX_OPEN; i++) {

        if (!tableMetaInfo[i].free &&
            strcmp(
                tableMetaInfo[i].relName,
                relName
            ) == 0) {

            return i;
        }
    }

    return E_RELNOTOPEN;
}


/**************************************************************/


int OpenRelTable::openRel(char relName[ATTR_SIZE]) {

    // Check whether relation is already open

    int relId = OpenRelTable::getRelId(relName);

    if (relId != E_RELNOTOPEN) {
        return relId;
    }


    // Get a free entry

    relId = OpenRelTable::getFreeOpenRelTableEntry();

    if (relId == E_CACHEFULL) {
        return E_CACHEFULL;
    }


    // Search RELATIONCAT for the relation

    Attribute relNameAttr;
    strcpy(relNameAttr.sVal, relName);

    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    char relCatAttrName[ATTR_SIZE];
    strcpy(relCatAttrName, RELCAT_ATTR_RELNAME);

    RecId recId = BlockAccess::linearSearch(
        RELCAT_RELID,
        relCatAttrName,
        relNameAttr,
        EQ
    );


    if (recId.block == -1 && recId.slot == -1) {
        return E_RELNOTEXIST;
    }


    // Read relation catalog record

    RecBuffer relCatBlock(RELCAT_BLOCK);

    Attribute relCatRecord[RELCAT_NO_ATTRS];

    relCatBlock.getRecord(
        relCatRecord,
        recId.slot
    );


    // Create relation cache entry

    RelCacheEntry *relCacheEntry =
        (RelCacheEntry *)malloc(sizeof(RelCacheEntry));

    RelCacheTable::recordToRelCatEntry(
        relCatRecord,
        &relCacheEntry->relCatEntry
    );

    relCacheEntry->dirty = false;

    relCacheEntry->recId = recId;

    relCacheEntry->searchIndex.block = -1;
    relCacheEntry->searchIndex.slot = -1;

    RelCacheTable::relCache[relId] =
        relCacheEntry;


    // Search ATTRIBUTECAT for attributes of this relation

    AttrCacheEntry *head = nullptr;
    AttrCacheEntry *prev = nullptr;

    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

    char attrCatAttrName[ATTR_SIZE];
    strcpy(attrCatAttrName, ATTRCAT_ATTR_RELNAME);

    for (int i = 0;
         i < relCacheEntry->relCatEntry.numAttrs;
         i++) {

       RecId attrRecId = BlockAccess::linearSearch(
            ATTRCAT_RELID,
            attrCatAttrName,
            relNameAttr,
            EQ
        );

        if (attrRecId.block == -1 &&
            attrRecId.slot == -1) {
            break;
        }


        RecBuffer attrCatBlock(attrRecId.block);

        Attribute attrCatRecord[ATTRCAT_NO_ATTRS];

        attrCatBlock.getRecord(
            attrCatRecord,
            attrRecId.slot
        );

        AttrCacheEntry *entry =
            (AttrCacheEntry *)malloc(sizeof(AttrCacheEntry));

        AttrCacheTable::recordToAttrCatEntry(
            attrCatRecord,
            &entry->attrCatEntry
        );

        entry->dirty = false;

        entry->recId = attrRecId;

        entry->searchIndex.block = -1;
        entry->searchIndex.index = -1;

        entry->next = nullptr;


        if (head == nullptr)
            head = entry;
        else
            prev->next = entry;

        prev = entry;
    }


    AttrCacheTable::attrCache[relId] = head;


    // Mark relation as open

    tableMetaInfo[relId].free = false;

    strcpy(
        tableMetaInfo[relId].relName,
        relName
    );


    return relId;
}


/**************************************************************/


int OpenRelTable::closeRel(int relId) {

    // Catalogs cannot be closed

    if (relId == RELCAT_RELID ||
        relId == ATTRCAT_RELID) {

        return E_NOTPERMITTED;
    }


    // Check relId

    if (relId < 0 || relId >= MAX_OPEN) {
        return E_OUTOFBOUND;
    }


    // Check whether relation is open

    if (tableMetaInfo[relId].free) {
        return E_RELNOTOPEN;
    }


    // Free relation cache

    if (RelCacheTable::relCache[relId] != nullptr) {

        free(RelCacheTable::relCache[relId]);

        RelCacheTable::relCache[relId] = nullptr;
    }


    // Free attribute cache linked list

    AttrCacheEntry *entry =
        AttrCacheTable::attrCache[relId];

    while (entry != nullptr) {

        AttrCacheEntry *temp = entry;

        entry = entry->next;

        free(temp);
    }

    AttrCacheTable::attrCache[relId] = nullptr;


    // Mark entry as free

    tableMetaInfo[relId].free = true;
    tableMetaInfo[relId].relName[0] = '\0';


    return SUCCESS;
}