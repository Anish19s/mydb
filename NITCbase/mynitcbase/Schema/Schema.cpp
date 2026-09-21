#include "Schema.h"

#include <cmath>
#include <cstring>


int Schema::openRel(char relName[ATTR_SIZE]) {
  int ret = OpenRelTable::openRel(relName);

  // the OpenRelTable::openRel() function returns the rel-id if successful
  // a valid rel-id will be within the range 0 <= relId < MAX_OPEN and any
  // error codes will be negative
  if(ret >= 0){
    return SUCCESS;
  }

  //otherwise it returns an error message
  return ret;
}

int Schema::closeRel(char relName[ATTR_SIZE]) {
  if (strcmp(relName,RELCAT_RELNAME)==0 || strcmp(relName,ATTRCAT_RELNAME)==0) {
    return E_NOTPERMITTED;
  }

  // this function returns the rel-id of a relation if it is open or
  // E_RELNOTOPEN if it is not. we will implement this later.
  int relId = OpenRelTable::getRelId(relName);

  if (relId==E_RELNOTOPEN) {
    return E_RELNOTOPEN;
  }

  return OpenRelTable::closeRel(relId);
}
int Schema::renameRel(char oldRelName[ATTR_SIZE], char newRelName[ATTR_SIZE]) {
     // Cannot rename the system catalogs
    if (strcmp(oldRelName, RELCAT_RELNAME) == 0 ||
        strcmp(oldRelName, ATTRCAT_RELNAME) == 0 ||
        strcmp(newRelName, RELCAT_RELNAME) == 0 ||
        strcmp(newRelName, ATTRCAT_RELNAME) == 0) {

        return E_NOTPERMITTED;
    }

    // Check if relation is currently open
    int relId = OpenRelTable::getRelId(oldRelName);

    if (relId != E_RELNOTOPEN) {
        return E_RELOPEN;
    }

    // Perform the rename
    int retVal = BlockAccess::renameRelation(oldRelName, newRelName);

    return retVal;
}
int Schema::renameAttr(char *relName, char *oldAttrName, char *newAttrName) {

    // Cannot rename attributes of system catalogs
    if (strcmp(relName, RELCAT_RELNAME) == 0 ||
        strcmp(relName, ATTRCAT_RELNAME) == 0) {

        return E_NOTPERMITTED;
    }

    // Check if relation is currently open
    int relId = OpenRelTable::getRelId(relName);

    if (relId != E_RELNOTOPEN) {
        return E_RELOPEN;
    }

    // Rename the attribute
    int retVal = BlockAccess::renameAttribute(
        relName,
        oldAttrName,
        newAttrName
    );

    return retVal;
}
int Schema::createRel(char relName[], int nAttrs, char attrs[][ATTR_SIZE], int attrtype[]) {

    // declare variable relNameAsAttribute of type Attribute
    Attribute relNameAsAttribute;

    // copy the relName into relNameAsAttribute.sVal
    strcpy(relNameAsAttribute.sVal, relName);

    // declare a variable targetRelId of type RecId
    RecId targetRelId;

    // Reset the searchIndex of RELCAT
    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    char relCatAttrName[ATTR_SIZE];
    strcpy(relCatAttrName, "RelName");

    // Search relation catalog for relation with same name
    targetRelId = BlockAccess::linearSearch(
        RELCAT_RELID,
        relCatAttrName,
        relNameAsAttribute,
        EQ
    );

    // If relation already exists
    if (targetRelId.block != -1 && targetRelId.slot != -1) {
        return E_RELEXIST;
    }

    // Check for duplicate attribute names
    for (int i = 0; i < nAttrs; i++) {
        for (int j = i + 1; j < nAttrs; j++) {

            if (strcmp(attrs[i], attrs[j]) == 0) {
                return E_DUPLICATEATTR;
            }
        }
    }

    /*
     * Create record for relation catalog
     */
    Attribute relCatRecord[RELCAT_NO_ATTRS];

    // Relation name
    strcpy(relCatRecord[RELCAT_REL_NAME_INDEX].sVal, relName);

    // Number of attributes
    relCatRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal = nAttrs;

    // Number of records initially
    relCatRecord[RELCAT_NO_RECORDS_INDEX].nVal = 0;

    // First block
    relCatRecord[RELCAT_FIRST_BLOCK_INDEX].nVal = -1;

    // Last block
    relCatRecord[RELCAT_LAST_BLOCK_INDEX].nVal = -1;

    // Number of slots per block
    relCatRecord[RELCAT_NO_SLOTS_PER_BLOCK_INDEX].nVal =
        2016 / (16 * nAttrs + 1);

    // Insert relation catalog record
    int retVal = BlockAccess::insert(RELCAT_RELID, relCatRecord);

    // If insertion failed
    if (retVal != SUCCESS) {
        return retVal;
    }

    /*
     * Insert attribute catalog records
     */
    for (int i = 0; i < nAttrs; i++) {

        Attribute attrCatRecord[ATTRCAT_NO_ATTRS];

        // Relation name
        strcpy(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal, relName);

        // Attribute name
        strcpy(attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, attrs[i]);

        // Attribute type
        attrCatRecord[ATTRCAT_ATTR_TYPE_INDEX].nVal = attrtype[i];

        // Primary key flag
        attrCatRecord[ATTRCAT_PRIMARY_FLAG_INDEX].nVal = -1;

        // Root block
        attrCatRecord[ATTRCAT_ROOT_BLOCK_INDEX].nVal = -1;

        // Attribute offset
        attrCatRecord[ATTRCAT_OFFSET_INDEX].nVal = i;

        // Insert into attribute catalog
        retVal = BlockAccess::insert(ATTRCAT_RELID, attrCatRecord);

        if (retVal != SUCCESS) {

            // Delete the relation created so far
            Schema::deleteRel(relName);

            return E_DISKFULL;
        }
    }

    return SUCCESS;
}

int Schema::deleteRel(char *relName) {

    // Cannot delete Relation Catalog or Attribute Catalog
    if (strcmp(relName, RELCAT_RELNAME) == 0 ||
        strcmp(relName, ATTRCAT_RELNAME) == 0) {
        return E_NOTPERMITTED;
    }

    // Get relation ID from OpenRelTable
    int relId = OpenRelTable::getRelId(relName);

    // If relation is currently open
    if (relId != E_RELNOTOPEN) {
        return E_RELOPEN;
    }

    // Delete the relation using BlockAccess
    int retVal = BlockAccess::deleteRelation(relName);

    return retVal;
}