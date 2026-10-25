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

  
  

  //============================ PRINT ALL RELNS ===========================
  // RecBuffer relCatBuffer(RELCAT_BLOCK);
  // HeadInfo relCatHeader;
  // relCatBuffer.getHeader(&relCatHeader);


  // unsigned char slotmap[relCatHeader.numSlots];
  // relCatBuffer.getSlotMap(slotmap);

  // for(int relslot=0;relslot<relCatHeader.numSlots;relslot++){
  //   if(slotmap[relslot]==SLOT_OCCUPIED){
  //     Attribute relrecord[RELCAT_NO_ATTRS];
  //     relCatBuffer.getRecord(relrecord,relslot);

  //     char * relname=relrecord[RELCAT_REL_NAME_INDEX].sVal;

  //     printf("Relation:%s\n",relname);


  //     for(int attrblk=ATTRCAT_BLOCK;attrblk!=-1;){
  //       RecBuffer attrCatbuffer(attrblk);
  //       HeadInfo attrCatHeader;
  //       attrCatbuffer.getHeader(&attrCatHeader);
        
  //       unsigned char attrslotmap[attrCatHeader.numSlots];
  //       attrCatbuffer.getSlotMap(attrslotmap);
        

  //       for(int attrslot=0;attrslot<attrCatHeader.numSlots;attrslot++){
  //           if(attrslotmap[attrslot]==SLOT_OCCUPIED){
  //             Attribute attrrecord[RELCAT_NO_ATTRS];
  //             attrCatbuffer.getRecord(attrrecord,attrslot);

  //             if(strcmp(relname,attrrecord[ATTRCAT_REL_NAME_INDEX].sVal)==0){
  //                 const char* typestr=(attrrecord[ATTRCAT_ATTR_TYPE_INDEX].nVal==NUMBER)?"NUM":"STR";
  //                 printf("%s:%s\n",attrrecord[ATTRCAT_ATTR_NAME_INDEX].sVal,typestr);
  //             }
  //           }
  //       }
  //       attrblk=attrCatHeader.rblock;
  //     }
  //   }
  // }








  //=====================printing message=================================

  // unsigned char buffer[BLOCK_SIZE];
  // Disk::readBlock(buffer,7000);
  // char message[]="hello";
  // memcpy(buffer+20,message,6);
  // Disk::writeBlock(buffer,7000);

  // unsigned char buffer2[BLOCK_SIZE];
  // char mssg2[6];
  // Disk::readBlock(mssg2,7000);
  // memcpy(mssg2,buffer+20,6);
  // std::cout<<mssg;


// ==================BLOCK ALLOC MAP========================
  
  // unsigned char buffer3[BLOCK_SIZE];

  // for (int block = 0; block <= 3; block++) {
  //     Disk::readBlock(buffer3, block);
  //     std::cout << "Block " << block << ":\n";
  //     for (int i = 0; i < BLOCK_SIZE; i++) {
  //         std::cout << (int)buffer3[i] << " ";
  //     }
  //     std::cout << "\n\n";
  // }

  




  // unsigned char buffer3[BLOCK_SIZE];
  // for(int block=0;block<=3;block++){
  //   Disk::readBlock(buffer3,block);
  //   std::cout<<"block:"<<block<<"\n";
  //   for(int j=0;j<BLOCK_SIZE;j++){
  //     std::cout<<(int)buffer3[j];
  //   }
  // }

  return FrontendInterface::handleFrontend(argc, argv);
}