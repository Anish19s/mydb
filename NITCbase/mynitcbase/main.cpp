#include "Buffer/StaticBuffer.h"
#include "Cache/OpenRelTable.h"
#include "Disk_Class/Disk.h"
#include "FrontendInterface/FrontendInterface.h"
#include <cstring>
#include <iostream>

int main(int argc, char *argv[]) {
  /* Initialize the Run Copy of Disk */
  Disk disk_run;
  // StaticBuffer buffer;

  unsigned char buffer[BLOCK_SIZE];
//BLOCK_SIZE is a constant that has value 2048
  Disk::readBlock(buffer, 7000);

  char message[]="hello";
  memcpy(buffer+20,message,6);
  Disk::writeBlock(buffer,7000);
  // OpenRelTable cache;

  unsigned char buffer2[BLOCK_SIZE];
  char messg2[6];
  Disk::readBlock(buffer2,7000);
  memcpy(messg2,buffer2+20,6);

  std::cout<<messg2;
  unsigned char buffer3[BLOCK_SIZE];

  for (int block = 0; block <= 3; block++) {
      Disk::readBlock(buffer3, block);

      std::cout << "Block " << block << ":\n";

      for (int i = 0; i < BLOCK_SIZE; i++) {
          std::cout << (int)buffer3[i] << " ";
      }

      std::cout << "\n\n";
  }
  return 0;
  //return FrontendInterface::handleFrontend(argc, argv);
}