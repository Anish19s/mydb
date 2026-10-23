#include "Algebra.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <strings.h>

bool isNumber(char *str) {
    int len;
    float ignore;

    int ret = sscanf(str, "%f %n", &ignore, &len);

    return ret == 1 && len == (int)strlen(str);
}

int Algebra::insert(char relName[ATTR_SIZE], int nAttrs,
                    char record[][ATTR_SIZE]) {

    // RELATIONCAT and ATTRIBUTECAT cannot be modified
    if (strcmp(relName, RELCAT_RELNAME) == 0 ||
        strcmp(relName, ATTRCAT_RELNAME) == 0) {

        return E_NOTPERMITTED;
    }

    // Get relation id
    int relId = OpenRelTable::getRelId(relName);

    // Relation is not open
    if (relId == E_RELNOTOPEN)
        return E_RELNOTOPEN;

    // Get relation catalog entry
    RelCatEntry relCatEntry;

    int retVal = RelCacheTable::getRelCatEntry(
        relId, &relCatEntry
    );

    if (retVal != SUCCESS)
        return retVal;

    // Check number of attributes
    if (relCatEntry.numAttrs != nAttrs)
        return E_NATTRMISMATCH;

    // Array to store converted attribute values
    union Attribute recordValues[nAttrs];

    // Convert char[][] to Attribute[]
    for (int i = 0; i < nAttrs; i++) {

        // Get attribute catalog entry
        AttrCatEntry attrCatEntry;

        retVal = AttrCacheTable::getAttrCatEntry(
            relId, i, &attrCatEntry
        );

        if (retVal != SUCCESS)
            return retVal;

        // Get attribute type
        int type = attrCatEntry.attrType;

        if (type == NUMBER) {

            // Check whether the input is a valid number
            if (isNumber(record[i])) {

                // Convert string to number
                recordValues[i].nVal = atof(record[i]);
            }
            else {
                return E_ATTRTYPEMISMATCH;
            }
        }

        else if (type == STRING) {

            // Copy string into Attribute
            strcpy(recordValues[i].sVal, record[i]);
        }
    }

    // Insert the record
    retVal = BlockAccess::insert(relId, recordValues);

    return retVal;
}


int Algebra::select(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE],
                    char attr[ATTR_SIZE], int op,
                    char strVal[ATTR_SIZE]) {

    // Get source relation ID
    int srcRelId = OpenRelTable::getRelId(srcRel);

    if (srcRelId == E_RELNOTOPEN)
        return E_RELNOTOPEN;

    // Get attribute catalog entry
    AttrCatEntry attrCatEntry;
    int ret = AttrCacheTable::getAttrCatEntry(
        srcRelId, attr, &attrCatEntry
    );

    if (ret != SUCCESS)
        return E_ATTRNOTEXIST;

    /*** Convert strVal to Attribute ***/

    Attribute attrVal;
    int type = attrCatEntry.attrType;

    if (type == NUMBER) {
        if (isNumber(strVal)) {
            attrVal.nVal = atof(strVal);
        }
        else {
            return E_ATTRTYPEMISMATCH;
        }
    }
    else if (type == STRING) {
        strcpy(attrVal.sVal, strVal);
    }

    // Get source relation catalog entry
    RelCatEntry relCatEntry;
    ret = RelCacheTable::getRelCatEntry(srcRelId, &relCatEntry);

    if (ret != SUCCESS)
        return ret;

    int src_nAttrs = relCatEntry.numAttrs;

    /*** Print selected records if targetRel is null ***/

    if (strcasecmp(targetRel, "null") == 0 ||
        strcmp(targetRel, "") == 0) {

        // Print attribute names
        printf("|");

        for (int i = 0; i < src_nAttrs; ++i) {
            AttrCatEntry entry;

            ret = AttrCacheTable::getAttrCatEntry(
                srcRelId, i, &entry
            );

            if (ret != SUCCESS)
                return ret;

            printf(" %s |", entry.attrName);
        }

        printf("\n");

        // Reset search indices
        RelCacheTable::resetSearchIndex(srcRelId);
        AttrCacheTable::resetSearchIndex(srcRelId, attr);

        Attribute record[src_nAttrs];

        // Search matching records
        while (BlockAccess::search(
                   srcRelId, record, attr, attrVal, op
               ) == SUCCESS) {

            printf("|");

            for (int i = 0; i < src_nAttrs; ++i) {
                AttrCatEntry entry;

                ret = AttrCacheTable::getAttrCatEntry(
                    srcRelId, i, &entry
                );

                if (ret != SUCCESS)
                    return ret;

                if (entry.attrType == NUMBER) {
                    printf(" %g |", record[entry.offset].nVal);
                }
                else if (entry.attrType == STRING) {
                    printf(" %s |", record[entry.offset].sVal);
                }
            }

            printf("\n");
        }

        return SUCCESS;
    }

    /*** Creating and opening the target relation ***/

    char attr_names[src_nAttrs][ATTR_SIZE];
    int attr_types[src_nAttrs];

    for (int i = 0; i < src_nAttrs; ++i) {
        AttrCatEntry entry;

        ret = AttrCacheTable::getAttrCatEntry(
            srcRelId, i, &entry
        );

        if (ret != SUCCESS)
            return ret;

        strcpy(attr_names[i], entry.attrName);
        attr_types[i] = entry.attrType;
    }

    // Create target relation
    ret = Schema::createRel(
        targetRel, src_nAttrs, attr_names, attr_types
    );

    if (ret != SUCCESS)
        return ret;

    // Open target relation
    int targetRelId = OpenRelTable::openRel(targetRel);

    if (targetRelId < 0) {
        Schema::deleteRel(targetRel);
        return targetRelId;
    }

    /*** Selecting and inserting records ***/

    Attribute record[src_nAttrs];

    // Reset relation and attribute search indices
    RelCacheTable::resetSearchIndex(srcRelId);
    AttrCacheTable::resetSearchIndex(srcRelId, attr);

    // Search matching records and insert into target relation
    while (BlockAccess::search(
               srcRelId, record, attr, attrVal, op
           ) == SUCCESS) {

        ret = BlockAccess::insert(targetRelId, record);

        if (ret != SUCCESS) {
            Schema::closeRel(targetRel);
            Schema::deleteRel(targetRel);
            return ret;
        }
    }

    // Close target relation
    Schema::closeRel(targetRel);

    return SUCCESS;
}


int Algebra::project(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE]) {

    // Get source relation ID
    int srcRelId = OpenRelTable::getRelId(srcRel);

    if (srcRelId == E_RELNOTOPEN)
        return E_RELNOTOPEN;

    // Get RelCatEntry of source relation
    RelCatEntry relCatEntry;
    int ret = RelCacheTable::getRelCatEntry(srcRelId, &relCatEntry);

    if (ret != SUCCESS)
        return ret;

    // Get number of attributes
    int numAttrs = relCatEntry.numAttrs;

    // Store attribute names and types
    char attrNames[numAttrs][ATTR_SIZE];
    int attrTypes[numAttrs];

    for (int i = 0; i < numAttrs; i++) {
        AttrCatEntry attrCatEntry;

        ret = AttrCacheTable::getAttrCatEntry(srcRelId, i,
                                               &attrCatEntry);

        if (ret != SUCCESS)
            return ret;

        strcpy(attrNames[i], attrCatEntry.attrName);
        attrTypes[i] = attrCatEntry.attrType;
    }

    if (strcasecmp(targetRel, "null") == 0 || strcmp(targetRel, "") == 0) {
        printf("|");
        for (int i = 0; i < numAttrs; ++i) {
            printf(" %s |", attrNames[i]);
        }
        printf("\n");

        RelCacheTable::resetSearchIndex(srcRelId);

        Attribute record[numAttrs];

        while (BlockAccess::project(srcRelId, record) == SUCCESS) {
            printf("|");
            for (int i = 0; i < numAttrs; ++i) {
                if (attrTypes[i] == NUMBER) {
                    printf(" %g |", record[i].nVal);
                } else if (attrTypes[i] == STRING) {
                    printf(" %s |", record[i].sVal);
                }
            }
            printf("\n");
        }

        return SUCCESS;
    }

    /*** Creating and opening the target relation ***/

    // Create target relation
    ret = Schema::createRel(targetRel, numAttrs,
                            attrNames, attrTypes);

    if (ret != SUCCESS)
        return ret;

    // Open target relation
    int targetRelId = OpenRelTable::openRel(targetRel);

    if (targetRelId < 0) {
        Schema::deleteRel(targetRel);
        return targetRelId;
    }

    /*** Inserting projected records into the target relation ***/

    // Reset search index
    RelCacheTable::resetSearchIndex(srcRelId);

    Attribute record[numAttrs];

    // Fetch records and insert into target relation
    while (BlockAccess::project(srcRelId, record) == SUCCESS) {

        int ret = BlockAccess::insert(targetRelId, record);

        if (ret != SUCCESS) {
            Schema::closeRel(targetRel);
            Schema::deleteRel(targetRel);
            return ret;
        }
    }

    // Close target relation
    Schema::closeRel(targetRel);

    return SUCCESS;
}

int Algebra::project(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE],
                     int tar_nAttrs, char tar_Attrs[][ATTR_SIZE]) {

    int srcRelId = OpenRelTable::getRelId(srcRel);

    // Check if source relation is open
    if (srcRelId == E_RELNOTOPEN)
        return E_RELNOTOPEN;

    // Get RelCatEntry of source relation
    RelCatEntry relCatEntry;
    int ret = RelCacheTable::getRelCatEntry(srcRelId, &relCatEntry);

    if (ret != SUCCESS)
        return ret;

    int src_nAttrs = relCatEntry.numAttrs;

    int attr_offset[tar_nAttrs];
    int attr_types[tar_nAttrs];

    /*** Checking if attributes of target are present in source ***/

    for (int i = 0; i < tar_nAttrs; i++) {
        AttrCatEntry attrCatEntry;

        ret = AttrCacheTable::getAttrCatEntry(srcRelId, tar_Attrs[i],
                                               &attrCatEntry);

        if (ret != SUCCESS)
            return E_ATTRNOTEXIST;

        attr_offset[i] = attrCatEntry.offset;
        attr_types[i] = attrCatEntry.attrType;
    }

    if (strcasecmp(targetRel, "null") == 0 || strcmp(targetRel, "") == 0) {
        printf("|");
        for (int i = 0; i < tar_nAttrs; ++i) {
            printf(" %s |", tar_Attrs[i]);
        }
        printf("\n");

        RelCacheTable::resetSearchIndex(srcRelId);

        Attribute record[src_nAttrs];

        while (BlockAccess::project(srcRelId, record) == SUCCESS) {
            printf("|");
            for (int i = 0; i < tar_nAttrs; ++i) {
                if (attr_types[i] == NUMBER) {
                    printf(" %g |", record[attr_offset[i]].nVal);
                } else if (attr_types[i] == STRING) {
                    printf(" %s |", record[attr_offset[i]].sVal);
                }
            }
            printf("\n");
        }

        return SUCCESS;
    }

    /*** Creating and opening the target relation ***/

    ret = Schema::createRel(targetRel, tar_nAttrs,
                            tar_Attrs, attr_types);

    if (ret != SUCCESS)
        return ret;

    int targetRelId = OpenRelTable::openRel(targetRel);

    if (targetRelId < 0) {
        Schema::deleteRel(targetRel);
        return targetRelId;
    }

    /*** Inserting projected records into the target relation ***/

    RelCacheTable::resetSearchIndex(srcRelId);

    Attribute record[src_nAttrs];

    while (BlockAccess::project(srcRelId, record) == SUCCESS) {

        Attribute proj_record[tar_nAttrs];

        for (int attr_iter = 0; attr_iter < tar_nAttrs; attr_iter++) {
            proj_record[attr_iter] = record[attr_offset[attr_iter]];
        }

        ret = BlockAccess::insert(targetRelId, proj_record);

        if (ret != SUCCESS) {
            Schema::closeRel(targetRel);
            Schema::deleteRel(targetRel);
            return ret;
        }
    }

    // Close target relation
    Schema::closeRel(targetRel);

    return SUCCESS;
}
int Algebra::join(char srcRelation1[ATTR_SIZE],
                  char srcRelation2[ATTR_SIZE],
                  char targetRelation[ATTR_SIZE],
                  char attribute1[ATTR_SIZE],
                  char attribute2[ATTR_SIZE]) {

    // ------------------------------------------------------------
    // 1. Get relation IDs
    // ------------------------------------------------------------

    int srcRelId1 = OpenRelTable::getRelId(srcRelation1);
    int srcRelId2 = OpenRelTable::getRelId(srcRelation2);

    if (srcRelId1 == E_RELNOTOPEN || srcRelId2 == E_RELNOTOPEN)
        return E_RELNOTOPEN;


    // ------------------------------------------------------------
    // 2. Get catalog entries of join attributes
    // ------------------------------------------------------------

    AttrCatEntry attrCatEntry1;
    AttrCatEntry attrCatEntry2;

    int retVal = AttrCacheTable::getAttrCatEntry(
        srcRelId1,
        attribute1,
        &attrCatEntry1
    );

    if (retVal == E_ATTRNOTEXIST)
        return E_ATTRNOTEXIST;

    retVal = AttrCacheTable::getAttrCatEntry(
        srcRelId2,
        attribute2,
        &attrCatEntry2
    );

    if (retVal == E_ATTRNOTEXIST)
        return E_ATTRNOTEXIST;


    // ------------------------------------------------------------
    // 3. Join attributes must have same type
    // ------------------------------------------------------------

    if (attrCatEntry1.attrType != attrCatEntry2.attrType)
        return E_ATTRTYPEMISMATCH;


    // ------------------------------------------------------------
    // 4. Check duplicate attribute names
    // ------------------------------------------------------------

    RelCatEntry relCatEntry1;
    RelCatEntry relCatEntry2;

    retVal = RelCacheTable::getRelCatEntry(
        srcRelId1,
        &relCatEntry1
    );

    if (retVal != SUCCESS)
        return retVal;

    retVal = RelCacheTable::getRelCatEntry(
        srcRelId2,
        &relCatEntry2
    );

    if (retVal != SUCCESS)
        return retVal;


    int numOfAttributes1 = relCatEntry1.numAttrs;
    int numOfAttributes2 = relCatEntry2.numAttrs;


    /*
     * Check every attribute in relation 1 against every attribute
     * in relation 2.
     *
     * The join attributes are allowed to have the same name.
     * Any other duplicate name is an error.
     */
    for (int i = 0; i < numOfAttributes1; i++) {

        AttrCatEntry tempAttr1;

        retVal = AttrCacheTable::getAttrCatEntry(
            srcRelId1,
            i,
            &tempAttr1
        );

        if (retVal != SUCCESS)
            return retVal;


        for (int j = 0; j < numOfAttributes2; j++) {

            AttrCatEntry tempAttr2;

            retVal = AttrCacheTable::getAttrCatEntry(
                srcRelId2,
                j,
                &tempAttr2
            );

            if (retVal != SUCCESS)
                return retVal;


            if (strcmp(
                    tempAttr1.attrName,
                    tempAttr2.attrName
                ) == 0) {

                /*
                 * Same name is allowed ONLY for the two
                 * join attributes.
                 */
                if (strcmp(tempAttr1.attrName, attribute1) != 0 ||
                    strcmp(tempAttr2.attrName, attribute2) != 0) {

                    return E_DUPLICATEATTR;
                }
            }
        }
    }


    // ------------------------------------------------------------
    // 5. Ensure B+ tree exists on attribute2
    // ------------------------------------------------------------

    if (attrCatEntry2.rootBlock == -1) {

        retVal = BPlusTree::bPlusCreate(
            srcRelId2,
            attribute2
        );

        if (retVal != SUCCESS)
            return retVal;
    }


    // ------------------------------------------------------------
    // 6. Construct target relation schema
    // ------------------------------------------------------------

    int numOfAttributesInTarget =
        numOfAttributes1 + numOfAttributes2 - 1;

    char targetRelAttrNames[
        numOfAttributesInTarget
    ][ATTR_SIZE];

    int targetRelAttrTypes[
        numOfAttributesInTarget
    ];

    int targetAttrIndex = 0;


    // Copy all attributes from relation 1
    for (int i = 0; i < numOfAttributes1; i++) {

        AttrCatEntry tempAttr;

        retVal = AttrCacheTable::getAttrCatEntry(
            srcRelId1,
            i,
            &tempAttr
        );

        if (retVal != SUCCESS)
            return retVal;

        strcpy(
            targetRelAttrNames[targetAttrIndex],
            tempAttr.attrName
        );

        targetRelAttrTypes[targetAttrIndex] =
            tempAttr.attrType;

        targetAttrIndex++;
    }


    // Copy all attributes from relation 2
    // except attribute2
    for (int i = 0; i < numOfAttributes2; i++) {

        AttrCatEntry tempAttr;

        retVal = AttrCacheTable::getAttrCatEntry(
            srcRelId2,
            i,
            &tempAttr
        );

        if (retVal != SUCCESS)
            return retVal;

        if (i == attrCatEntry2.offset)
            continue;

        strcpy(
            targetRelAttrNames[targetAttrIndex],
            tempAttr.attrName
        );

        targetRelAttrTypes[targetAttrIndex] =
            tempAttr.attrType;

        targetAttrIndex++;
    }


    // ------------------------------------------------------------
    // 7. Create target relation
    // ------------------------------------------------------------

    retVal = Schema::createRel(
        targetRelation,
        numOfAttributesInTarget,
        targetRelAttrNames,
        targetRelAttrTypes
    );

    if (retVal != SUCCESS)
        return retVal;


    // ------------------------------------------------------------
    // 8. Open target relation
    // ------------------------------------------------------------

    int targetRelId =
        OpenRelTable::openRel(targetRelation);

    if (targetRelId == E_CACHEFULL) {

        Schema::deleteRel(targetRelation);

        return E_CACHEFULL;
    }


    // ------------------------------------------------------------
    // 9. Prepare record arrays
    // ------------------------------------------------------------

    Attribute record1[numOfAttributes1];
    Attribute record2[numOfAttributes2];

    Attribute targetRecord[
        numOfAttributesInTarget
    ];


    // ------------------------------------------------------------
    // 10. Get every record from relation 1
    // ------------------------------------------------------------

    while (
        BlockAccess::project(
            srcRelId1,
            record1
        ) == SUCCESS
    ) {

        /*
         * Reset search indexes before searching relation 2
         * for this new record1.
         */
        RelCacheTable::resetSearchIndex(srcRelId2);

        AttrCacheTable::resetSearchIndex(
            srcRelId2,
            attribute2
        );


        // --------------------------------------------------------
        // 11. Find matching records in relation 2
        // --------------------------------------------------------

        while (
            BlockAccess::search(
                srcRelId2,
                record2,
                attribute2,
                record1[attrCatEntry1.offset],
                EQ
            ) == SUCCESS
        ) {

            int targetIndex = 0;


            // Copy relation 1 record
            for (int i = 0; i < numOfAttributes1; i++) {

                targetRecord[targetIndex] =
                    record1[i];

                targetIndex++;
            }


            // Copy relation 2 record except join attribute
            for (int i = 0; i < numOfAttributes2; i++) {

                if (i == attrCatEntry2.offset)
                    continue;

                targetRecord[targetIndex] =
                    record2[i];

                targetIndex++;
            }


            // ----------------------------------------------------
            // 12. Insert joined record
            // ----------------------------------------------------

            retVal = BlockAccess::insert(
                targetRelId,
                targetRecord
            );

            if (retVal != SUCCESS) {

                OpenRelTable::closeRel(targetRelId);

                Schema::deleteRel(targetRelation);

                return retVal;
            }
        }
    }


    // ------------------------------------------------------------
    // 13. Close target relation
    // ------------------------------------------------------------

    OpenRelTable::closeRel(targetRelId);

    return SUCCESS;
}