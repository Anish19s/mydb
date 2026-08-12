#include <cstdio>
#include <cstring>
#include <iostream>
#include "Buffer/BlockBuffer.h"
#include "define/constants.h"
#include "FrontendInterface/FrontendInterface.h"
#include "Cache/RelCacheTable.h"
#include "Cache/AttrCacheTable.h"
#include "Cache/OpenRelTable.h"

int main(int argc, char *argv[]) {

  Disk disk_run;
  StaticBuffer buffer;
  OpenRelTable cache;

  for (int i = 0; i <= 2; i++) {

    RelCatEntry relCatEntry;

    // Get relation catalog entry
    RelCacheTable::getRelCatEntry(i, &relCatEntry);

    printf("Relation: %s\n", relCatEntry.relName);

    // Get all attribute catalog entries
    for (int j = 0; j < relCatEntry.numAttrs; j++) {

      AttrCatEntry attrCatEntry;

      AttrCacheTable::getAttrCatEntry(i, j, &attrCatEntry);

      printf("  %s: ", attrCatEntry.attrName);

      if (attrCatEntry.attrType == STRING) {
        printf("STRING\n");
      }
      else if (attrCatEntry.attrType == NUMBER) {
        printf("NUMBER\n");
      }
    }
  }

  return 0;
}