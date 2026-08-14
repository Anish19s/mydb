#include "Disk.h"

#include <fstream>
#include <iostream>

#include "../define/constants.h"

/*
 * Used to make a temporary copy of the disk contents before the starting of a new session.
 * This ensures that if the system has a forced shutdown during the course of the session,
 * the previous state of the disk is not lost.
 */
Disk::Disk() {
    std::ifstream src(DISK_PATH, std::ios::binary | std::ios::ate);

    if (!src || src.tellg() != DISK_SIZE) {
        std::cerr << "Error: Invalid disk file!\n";
        return;
    }

    src.seekg(0);
    std::ofstream dst(DISK_RUN_COPY_PATH,
                      std::ios::binary | std::ios::trunc);
    dst << src.rdbuf();
}

Disk::~Disk() {
    std::ifstream src(DISK_RUN_COPY_PATH,
                      std::ios::binary | std::ios::ate);

    if (!src || src.tellg() != DISK_SIZE) {
        std::cerr << "Error: Run copy corrupted. Not saving.\n";
        return;
    }

    src.seekg(0);
    std::ofstream dst(DISK_PATH,
                      std::ios::binary | std::ios::trunc);
    dst << src.rdbuf();
}

/*
 * Used to Read a specified block from disk
 * block - Memory pointer of the buffer to which the block contents is to be loaded/read.
 *         (MUST be Allocated by caller)
 * blockNum - Block number of the disk block to be read.
 */
int Disk::readBlock(unsigned char *block, int blockNum) {
  FILE *disk = fopen(DISK_RUN_COPY_PATH, "rb");
  if (blockNum < 0 || blockNum > DISK_BLOCKS - 1) {
    return E_OUTOFBOUND;
  }
  const int offset = blockNum * BLOCK_SIZE;
  fseek(disk, offset, SEEK_SET);
  fread(block, BLOCK_SIZE, 1, disk);
  fclose(disk);
  return SUCCESS;
}

/*
 * Used to Write a specified block from disk
 * block - Memory pointer of the buffer to which contain the contents to be written.
 *         (MUST be Allocated by caller)
 * blockNum - Block number of the disk block to be written into.
 */
int Disk::writeBlock(unsigned char *block, int blockNum) {
  FILE *disk = fopen(DISK_RUN_COPY_PATH, "rb+");
  if (blockNum < 0 || blockNum > DISK_BLOCKS - 1) {
    return E_OUTOFBOUND;
  }
  const int offset = blockNum * BLOCK_SIZE;
  fseek(disk, offset, SEEK_SET);
  fwrite(block, BLOCK_SIZE, 1, disk);
  fclose(disk);
  return SUCCESS;
}