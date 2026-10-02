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
    int ret = AttrCacheTable::getAttrCatEntry(srcRelId, attr,
                                               &attrCatEntry);

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

    RelCatEntry relCatEntry;
    ret = RelCacheTable::getRelCatEntry(srcRelId, &relCatEntry);

    if (ret != SUCCESS)
        return ret;

    int src_nAttrs = relCatEntry.numAttrs;

    if (strcasecmp(targetRel, "null") == 0 || strcmp(targetRel, "") == 0) {
        /*
         * Print attribute names
         */
        printf("|");

        for (int i = 0; i < src_nAttrs; ++i) {
            AttrCatEntry entry;
            ret = AttrCacheTable::getAttrCatEntry(srcRelId, i, &entry);
            if (ret != SUCCESS) {
                return ret;
            }
            printf(" %s |", entry.attrName);
        }

        printf("\n");

        // Reset search index
        RelCacheTable::resetSearchIndex(srcRelId);
        AttrCacheTable::resetSearchIndex(srcRelId, attr);

        Attribute record[src_nAttrs];

        while (BlockAccess::search(srcRelId, record, attr, attrVal, op) == SUCCESS) {
            printf("|");

            for (int i = 0; i < src_nAttrs; ++i) {
                AttrCatEntry entry;
                ret = AttrCacheTable::getAttrCatEntry(srcRelId, i, &entry);
                if (ret != SUCCESS) {
                    return ret;
                }

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

    for (int i = 0; i < src_nAttrs; i++) {
        AttrCatEntry entry;

        ret = AttrCacheTable::getAttrCatEntry(srcRelId, i, &entry);

        if (ret != SUCCESS)
            return ret;

        strcpy(attr_names[i], entry.attrName);
        attr_types[i] = entry.attrType;
    }

    // Create target relation
    ret = Schema::createRel(targetRel, src_nAttrs,
                            attr_names, attr_types);

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

    // Reset search indices
    RelCacheTable::resetSearchIndex(srcRelId);
    AttrCacheTable::resetSearchIndex(srcRelId, attr);

    // Search and insert matching records
    while (BlockAccess::search(srcRelId, record, attr,
                               attrVal, op) == SUCCESS) {

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