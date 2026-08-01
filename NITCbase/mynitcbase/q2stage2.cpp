#include <cstdio>
#include <cstring>

#include "Buffer/BlockBuffer.h"
#include "define/constants.h"


int main(int argc, char *argv[]) {

    Disk disk_run;


    // -----------------------------
    // Update Attribute Catalog
    // -----------------------------

    int attrBlock = ATTRCAT_BLOCK;

    bool updated = false;


    while(attrBlock != -1 && !updated) {


        RecBuffer attrCatBuffer(attrBlock);

        HeadInfo attrCatHeader;

        attrCatBuffer.getHeader(&attrCatHeader);



        for(int i = 0; i < attrCatHeader.numEntries; i++) {


            Attribute attrCatRecord[ATTRCAT_NO_ATTRS];


            attrCatBuffer.getRecord(attrCatRecord, i);



            // Find Students.Branch
            if(strcmp(
                attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal,
                "Students"
            ) == 0
            &&
            strcmp(
                attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal,
                "Branch"
            ) == 0) {


                // Rename Branch -> Batch
                strcpy(
                    attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal,
                    "Batch"
                );


                // Write back to disk
                attrCatBuffer.setRecord(
                    attrCatRecord,
                    i
                );


                updated = true;

                break;
            }
        }


        // Go to next attribute catalog block
        attrBlock = attrCatHeader.rblock;
    }



    if(updated)
        printf("Attribute updated successfully\n");
    else
        printf("Attribute not found\n");



    // -----------------------------
    // Print updated Students schema
    // -----------------------------


    printf("\nUpdated Schema:\n\n");


    RecBuffer relCatBuffer(RELCAT_BLOCK);


    HeadInfo relCatHeader;

    relCatBuffer.getHeader(&relCatHeader);



    for(int i = 0; i < relCatHeader.numEntries; i++) {


        Attribute relCatRecord[RELCAT_NO_ATTRS];


        relCatBuffer.getRecord(relCatRecord, i);



        if(strcmp(
            relCatRecord[RELCAT_REL_NAME_INDEX].sVal,
            "Students"
        ) == 0) {


            printf("Relation: %s\n",
                relCatRecord[RELCAT_REL_NAME_INDEX].sVal);



            int attrBlock = ATTRCAT_BLOCK;



            while(attrBlock != -1) {


                RecBuffer attrCatBuffer(attrBlock);


                HeadInfo attrCatHeader;

                attrCatBuffer.getHeader(&attrCatHeader);



                for(int j = 0; j < attrCatHeader.numEntries; j++) {


                    Attribute attrCatRecord[ATTRCAT_NO_ATTRS];


                    attrCatBuffer.getRecord(
                        attrCatRecord,
                        j
                    );



                    if(strcmp(
                        attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal,
                        "Students"
                    ) == 0) {


                        const char *type =
                            attrCatRecord[ATTRCAT_ATTR_TYPE_INDEX].nVal == NUMBER
                            ? "NUM"
                            : "STR";



                        printf("  %s : %s\n",
                            attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal,
                            type);
                    }
                }


                attrBlock = attrCatHeader.rblock;
            }
        }
    }


    return 0;
}