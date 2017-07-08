// Copyright 2017 Jim Geist. This software is licensed under the 
// MIT license as described in the file LICENSE.txt.

#include "Block.h"
#include "BlockCache.h"
#include "MemoryDataSource.h"
#include "FilesystemException.h"
#include "gtest/gtest.h"

#include <cerrno>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <unistd.h>
#include <vector>

using namespace RT11FS;

using std::make_unique;
using std::unique_ptr;
using std::vector;

namespace {
class BlockCacheTest : public ::testing::Test
{
protected:
  static const int sectors = 16;

  BlockCacheTest()
    : dataSource(make_unique<MemoryDataSource>(sectors * Block::SECTOR_SIZE))
    , blockCache(make_unique<BlockCache>(dataSource.get()))
    , data(dataSource->getData())
  { 
  }

  std::unique_ptr<MemoryDataSource> dataSource;
  std::unique_ptr<BlockCache> blockCache;
  vector<uint8_t> &data;
};

const int BlockCacheTest::sectors;

TEST_F(BlockCacheTest, GetBlock)
{
  for (auto i = 0; i < sectors; i++) {
    data[i * Block::SECTOR_SIZE] = i;
  }

  auto block = blockCache->getBlock(5, 2);
  EXPECT_EQ(block->getSector(), 5);
  EXPECT_EQ(block->getCount(), 2);

  EXPECT_EQ(block->getByte(0), 5);
  EXPECT_EQ(block->getByte(Block::SECTOR_SIZE), 6);
}

TEST_F(BlockCacheTest, GetBlockInvalid)
{
  for (auto i = 0; i < sectors; i++) {
    data[i * Block::SECTOR_SIZE] = i;
  }

  // ask for block that's out of range
  try {
    blockCache->getBlock(sectors, 1);
    FAIL() << "Asking for block out of range did not throw an exception";
  } catch (FilesystemException &ex) {
    EXPECT_EQ(ex.getError(), -EIO);
  } catch (...) {
    FAIL() << "Asking for block out of range threw the wrong exception";
  }

  blockCache->getBlock(1, 3);

  // re-requesting block of different size is an error
  try {
    blockCache->getBlock(1, 1);
    FAIL() << "Asking for size invariant breaking block did not throw an exception";
  } catch (FilesystemException &ex) {
    EXPECT_EQ(ex.getError(), -EINVAL);
  } catch (...) {
    FAIL() << "Asking for size invariant breaking block threw the wrong exception";
  }

  // requesting an overlapping block is an error
  try {
    blockCache->getBlock(3, 1);
    FAIL() << "Asking for overlapping block did not throw an exception";
  } catch (FilesystemException &ex) {
    EXPECT_EQ(ex.getError(), -EINVAL);
  } catch (...) {
    FAIL() << "Asking for overlapping block threw the wrong exception";
  }

  // make sure asking for adjacent blocks does not throw
  blockCache->getBlock(0, 1);
  blockCache->getBlock(4, 1);
}

TEST_F(BlockCacheTest, GetVolumeSectors)
{
  EXPECT_EQ(blockCache->getVolumeSectors(), sectors);
}

TEST_F(BlockCacheTest, ResizeErrors)
{
  // NOTE that block resize is actually tested in BlockTest.
  // Here we only test the error handling in BlockCache

  auto block = blockCache->getBlock(5, 1);

  // should succeed
  blockCache->resizeBlock(block, 2);

  try {
    blockCache->resizeBlock(block, 0);
    FAIL() << "Resizing block to zero did not throw an exception";
  } catch (FilesystemException &ex) {
    EXPECT_EQ(ex.getError(), -EINVAL);
  } catch (...) {
    FAIL() << "Reszing block to zero threw the wrong exception";
  }

  try {
    blockCache->resizeBlock(block, -1);
    FAIL() << "Resizing block to negative did not throw an exception";
  } catch (FilesystemException &ex) {
    EXPECT_EQ(ex.getError(), -EINVAL);
  } catch (...) {
    FAIL() << "Resizing block to negative threw the wrong exception";
  }

  try {
    blockCache->resizeBlock(reinterpret_cast<Block*>(42), 2);
    FAIL() << "Resizing a bad block pointer did not throw an exception";
  } catch (FilesystemException &ex) {
    EXPECT_EQ(ex.getError(), -EINVAL);
  } catch (...) {
    FAIL() << "Resizing a bad block pointer threw the wrong exception";
  }

  auto block2 = blockCache->getBlock(4, 1);

  // try to force overlap
  try {
    blockCache->resizeBlock(block2, 2);
    FAIL() << "Resizing to force an overlap did not throw an exception";
  } catch (FilesystemException &ex) {
    EXPECT_EQ(ex.getError(), -EINVAL);
  } catch (...) {
    FAIL() << "Resizing to force an overlap throw the wrong exception";
  }
}

}
