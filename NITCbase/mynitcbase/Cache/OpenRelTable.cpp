#include "OpenRelTable.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include "../Buffer/BlockBuffer.h"
#include "../define/constants.h"


OpenRelTable::OpenRelTable() {

  // initialize relCache and attrCache with nullptr
  for (int i = 0; i < MAX_OPEN; ++i) {
    RelCacheTable::relCache[i] = nullptr;
    AttrCacheTable::attrCache[i] = nullptr;
  }


  /************ Setting up Relation Cache entries ************/

  // -------- Relation Catalog --------

  RecBuffer relCatBlock(RELCAT_BLOCK);

  Attribute relCatRecord[RELCAT_NO_ATTRS];
  relCatBlock.getRecord(relCatRecord, RELCAT_SLOTNUM_FOR_RELCAT);

  RelCacheEntry relCacheEntry;

  RelCacheTable::recordToRelCatEntry(
      relCatRecord,
      &relCacheEntry.relCatEntry
  );

  relCacheEntry.recId.block = RELCAT_BLOCK;
  relCacheEntry.recId.slot = RELCAT_SLOTNUM_FOR_RELCAT;


  RelCacheTable::relCache[RELCAT_RELID] =
      (RelCacheEntry*)malloc(sizeof(RelCacheEntry));

  *(RelCacheTable::relCache[RELCAT_RELID]) = relCacheEntry;



  // -------- Attribute Catalog --------

  relCatBlock.getRecord(relCatRecord, RELCAT_SLOTNUM_FOR_ATTRCAT);

  RelCacheEntry attrRelCacheEntry;

  RelCacheTable::recordToRelCatEntry(
      relCatRecord,
      &attrRelCacheEntry.relCatEntry
  );

  attrRelCacheEntry.recId.block = RELCAT_BLOCK;
  attrRelCacheEntry.recId.slot = RELCAT_SLOTNUM_FOR_ATTRCAT;


  RelCacheTable::relCache[ATTRCAT_RELID] =
      (RelCacheEntry*)malloc(sizeof(RelCacheEntry));

  *(RelCacheTable::relCache[ATTRCAT_RELID]) = attrRelCacheEntry;

    /************ Setting up Students relation in Relation Cache ************/


  Attribute studentRecord[RELCAT_NO_ATTRS];

  // Students entry is in slot 2
  relCatBlock.getRecord(studentRecord, 2);

  RelCacheEntry studentRelCacheEntry;

  RelCacheTable::recordToRelCatEntry(
          studentRecord,
          &studentRelCacheEntry.relCatEntry
  );

  studentRelCacheEntry.recId.block = RELCAT_BLOCK;
  studentRelCacheEntry.recId.slot = 2;


  RelCacheTable::relCache[2] =
      (RelCacheEntry*)malloc(sizeof(RelCacheEntry));

  *(RelCacheTable::relCache[2]) = studentRelCacheEntry;

  printf("Students relName = %s\n",
        RelCacheTable::relCache[2]->relCatEntry.relName);

  printf("Students numAttrs = %d\n",
        RelCacheTable::relCache[2]->relCatEntry.numAttrs);

  printf("Students numRecs = %d\n",
        RelCacheTable::relCache[2]->relCatEntry.numRecs);

  printf("Students firstBlk = %d\n",
        RelCacheTable::relCache[2]->relCatEntry.firstBlk);

  printf("Students lastBlk = %d\n",
        RelCacheTable::relCache[2]->relCatEntry.lastBlk);
  /************ Setting up Attribute cache entries ************/


  RecBuffer attrCatBlock(ATTRCAT_BLOCK);

  Attribute attrCatRecord[ATTRCAT_NO_ATTRS];


  // -------- Attributes of Relation Catalog --------

  AttrCacheEntry *relAttrHead = nullptr;
  AttrCacheEntry *prev = nullptr;


  for (int i = 0; i < RELCAT_NO_ATTRS; i++) {

    attrCatBlock.getRecord(attrCatRecord, i);


    AttrCacheEntry *entry =
        (AttrCacheEntry*)malloc(sizeof(AttrCacheEntry));


    AttrCacheTable::recordToAttrCatEntry(
        attrCatRecord,
        &entry->attrCatEntry
    );


    entry->recId.block = ATTRCAT_BLOCK;
    entry->recId.slot = i;

    entry->next = nullptr;


    if (relAttrHead == nullptr)
      relAttrHead = entry;
    else
      prev->next = entry;


    prev = entry;
  }


  AttrCacheTable::attrCache[RELCAT_RELID] = relAttrHead;



  // -------- Attributes of Attribute Catalog --------

  AttrCacheEntry *attrAttrHead = nullptr;
  prev = nullptr;


  for (int i = 0; i < ATTRCAT_NO_ATTRS; i++) {

    // Attribute catalog entries start from slot 6
    attrCatBlock.getRecord(attrCatRecord,
                           RELCAT_NO_ATTRS + i);


    AttrCacheEntry *entry =
        (AttrCacheEntry*)malloc(sizeof(AttrCacheEntry));


    AttrCacheTable::recordToAttrCatEntry(
        attrCatRecord,
        &entry->attrCatEntry
    );


    entry->recId.block = ATTRCAT_BLOCK;
    entry->recId.slot = RELCAT_NO_ATTRS + i;

    entry->next = nullptr;


    if (attrAttrHead == nullptr)
      attrAttrHead = entry;
    else
      prev->next = entry;


    prev = entry;
  }


  AttrCacheTable::attrCache[ATTRCAT_RELID] = attrAttrHead;
  // -------- Attributes of Students --------

AttrCacheEntry *studentHead = nullptr;
prev = nullptr;

for (int i = 12; i <= 15; i++) {

    attrCatBlock.getRecord(attrCatRecord, i);

    AttrCacheEntry *entry =
        (AttrCacheEntry*)malloc(sizeof(AttrCacheEntry));

    AttrCacheTable::recordToAttrCatEntry(
        attrCatRecord,
        &entry->attrCatEntry
    );

    entry->recId.block = ATTRCAT_BLOCK;
    entry->recId.slot = i;
    entry->next = nullptr;

    if (studentHead == nullptr)
        studentHead = entry;
    else
        prev->next = entry;

    prev = entry;
}

AttrCacheTable::attrCache[2] = studentHead;
}



OpenRelTable::~OpenRelTable() {

  // free relation cache entries

  for (int i = 0; i < MAX_OPEN; i++) {

    if (RelCacheTable::relCache[i] != nullptr) {
      free(RelCacheTable::relCache[i]);
      RelCacheTable::relCache[i] = nullptr;
    }


    // free attribute cache linked list

    AttrCacheEntry *entry = AttrCacheTable::attrCache[i];

    while (entry != nullptr) {

      AttrCacheEntry *temp = entry;
      entry = entry->next;

      free(temp);
    }

    AttrCacheTable::attrCache[i] = nullptr;
  }
}
int OpenRelTable::getRelId(char relName[ATTR_SIZE]) {

    if (strcmp(relName, RELCAT_RELNAME) == 0) {
        return RELCAT_RELID;
    }

    if (strcmp(relName, ATTRCAT_RELNAME) == 0) {
        return ATTRCAT_RELID;
    }

    if (strcmp(relName, "Students") == 0) {
        return 2;
    }

    return E_RELNOTOPEN;
}