#include "Algebra.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

bool isNumber(char *str) {
    int len;
    float ignore;

    int ret = sscanf(str, "%f %n", &ignore, &len);

    return ret == 1 && len == strlen(str);
}

int Algebra::select(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE],
                    char attr[ATTR_SIZE], int op,
                    char strVal[ATTR_SIZE]) {

  //  printf("DEBUG: srcRel='%s', targetRel='%s', attr='%s', op=%d, value='%s'\n",
    //    srcRel, targetRel, attr, op, strVal);

    int srcRelId = OpenRelTable::getRelId(srcRel);

    if (srcRelId == E_RELNOTOPEN) {
        return E_RELNOTOPEN;
    }

    // Get attribute catalog entry for the attribute
    AttrCatEntry attrCatEntry;

//printf("DEBUG: srcRelId=%d attr='%s'\n", srcRelId, attr);

int ret = AttrCacheTable::getAttrCatEntry(
    srcRelId,
    attr,
    &attrCatEntry
);

// printf("DEBUG: getAttrCatEntry returned %d\n", ret);

if (ret != SUCCESS) {
    return E_ATTRNOTEXIST;
}

    // Convert strVal into an Attribute
    int type = attrCatEntry.attrType;

    Attribute attrVal;

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

    // Reset search index so that search starts from first record
    RelCacheTable::resetSearchIndex(srcRelId);

    // Get relation catalog entry
    RelCatEntry relCatEntry;

    ret = RelCacheTable::getRelCatEntry(srcRelId, &relCatEntry);

    if (ret != SUCCESS) {
        return ret;
    }

    /*
     * Print attribute names
     */
    printf("|");

    for (int i = 0; i < relCatEntry.numAttrs; ++i) {

        AttrCatEntry attrCatEntry;

        ret = AttrCacheTable::getAttrCatEntry(
            srcRelId,
            i,
            &attrCatEntry
        );

        if (ret != SUCCESS) {
            return ret;
        }

        printf(" %s |", attrCatEntry.attrName);
    }

    printf("\n");

    /*
     * Search and print matching records
     */
    while (true) {

        RecId searchRes =
            BlockAccess::linearSearch(
                srcRelId,
                attr,
                attrVal,
                op
            );

        // No more matching records
        if (searchRes.block == -1 && searchRes.slot == -1) {
            break;
        }

        /*
         * Get the block containing the record
         */
        RecBuffer recBuffer(searchRes.block);

        /*
         * Get the record
         */
        Attribute record[relCatEntry.numAttrs];

        ret = recBuffer.getRecord(record, searchRes.slot);

        if (ret != SUCCESS) {
            return ret;
        }

        /*
         * Print record
         */
        printf("|");

        for (int i = 0; i < relCatEntry.numAttrs; ++i) {

            AttrCatEntry attrCatEntry;

            ret = AttrCacheTable::getAttrCatEntry(
                srcRelId,
                i,
                &attrCatEntry
            );

            if (ret != SUCCESS) {
                return ret;
            }

            if (attrCatEntry.attrType == NUMBER) {
                printf(" %g |", record[attrCatEntry.offset].nVal);
            }
            else if (attrCatEntry.attrType == STRING) {
                printf(" %s |", record[attrCatEntry.offset].sVal);
            }
        }

        printf("\n");
    }

    return SUCCESS;
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
