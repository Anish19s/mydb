#include <cstdio>
#include <cstring>

#include "Buffer/BlockBuffer.h"
#include "define/constants.h"

int main(int argc, char *argv[]) {

    Disk disk_run;
    StaticBuffer static_buffer;

    // Relation Catalog
    RecBuffer relCatBuffer(RELCAT_BLOCK);

    HeadInfo relCatHeader;

    relCatBuffer.getHeader(&relCatHeader);



    // Traverse all relations
    for(int i = 0; i < relCatHeader.numEntries; i++) {

        Attribute relCatRecord[RELCAT_NO_ATTRS];

        relCatBuffer.getRecord(relCatRecord, i);


        printf("Relation: %s\n",
            relCatRecord[RELCAT_REL_NAME_INDEX].sVal);



        // Traverse Attribute Catalog blocks
        int attrBlock = ATTRCAT_BLOCK;


        while(attrBlock != -1) {

            RecBuffer attrCatBuffer(attrBlock);

            HeadInfo attrCatHeader;

            attrCatBuffer.getHeader(&attrCatHeader);



            // Traverse attributes in this block
            for(int j = 0; j < attrCatHeader.numEntries; j++) {

                Attribute attrCatRecord[ATTRCAT_NO_ATTRS];

                attrCatBuffer.getRecord(attrCatRecord, j);



                // Check if attribute belongs to current relation
                if(strcmp(
                    attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal,
                    relCatRecord[RELCAT_REL_NAME_INDEX].sVal
                ) == 0) {


                    const char *attrType =
                        attrCatRecord[ATTRCAT_ATTR_TYPE_INDEX].nVal == NUMBER
                        ? "NUM"
                        : "STR";


                    printf("  %s : %s\n",
                        attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal,
                        attrType);
                }
            }


            // Move to next attribute catalog block
            attrBlock = attrCatHeader.rblock;
        }


        printf("\n");
    }


    return 0;
}