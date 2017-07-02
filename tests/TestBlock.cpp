#include "Block.h"
#include "MemoryDataSource.h"
#include "FilesystemException.h"
#include "gtest/gtest.h"

#include <array>
#include <cerrno>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <unistd.h>

using namespace RT11FS;

using std::array;
using std::out_of_range;
using std::make_unique;

namespace {
class BlockTest : public ::testing::Test
{
};

TEST(Block, BlockBasics)
{
  auto size = 3 * Block::SECTOR_SIZE;
  auto dataSource = make_unique<MemoryDataSource>(size);
  auto data = dataSource->getData();

  for (auto i = 0; i < size; i++) {
    data[i] = i & 0xff;
  }

  auto const testSector = 2;
  auto const testWord = uint16_t {12345};

  data[testSector * Block::SECTOR_SIZE] = testWord & 0xff;
  data[testSector * Block::SECTOR_SIZE + 1] = (testWord >> 8) & 0xff;

  auto block = Block {2, 1};

  block.read(dataSource.get());

  EXPECT_EQ(block.getSector(), testSector);
  EXPECT_EQ(block.getCount(), 1);

  // test the write sector being read
  auto dataOut = array<char, Block::SECTOR_SIZE> {};
  block.copyOut(0, dataOut.size(), &dataOut[0]);

  EXPECT_EQ(memcmp(&data[testSector * Block::SECTOR_SIZE], &dataOut[0], Block::SECTOR_SIZE), 0);

  // test word extraction
  EXPECT_EQ(block.extractWord(0), testWord);

  // test setting data
  EXPECT_FALSE(block.isDirty());

  auto byte = block.extractWord(2) & 0xff;
  EXPECT_NE(byte, 42);
  block.setByte(2, 42);
  byte = block.extractWord(2) & 0xff;
  EXPECT_EQ(byte, 42);
  EXPECT_TRUE(block.isDirty());

  EXPECT_NE(block.extractWord(2), testWord);
  block.setWord(2, testWord);
  EXPECT_EQ(block.extractWord(2), testWord);

  // re-reading the block should clear the dirty flag
  block.read(dataSource.get());
  EXPECT_FALSE(block.isDirty());

  // copying data around
  EXPECT_EQ(block.extractWord(0), testWord);
  EXPECT_NE(block.extractWord(4), testWord);
  auto nextWord = block.extractWord(6);
  block.copyWithinBlock(0, 4, sizeof(uint16_t));
  EXPECT_EQ(block.extractWord(4), testWord);
  EXPECT_EQ(block.extractWord(6), nextWord);
  EXPECT_TRUE(block.isDirty());

  // copying data between blocks
  auto otherBlock = Block {0, 1};
  otherBlock.read(dataSource.get());

  nextWord = otherBlock.extractWord(4);
  EXPECT_NE(otherBlock.extractWord(2), testWord);
  EXPECT_FALSE(otherBlock.isDirty());
  otherBlock.copyFromOtherBlock(&block, 0, 2, sizeof(uint16_t));
  EXPECT_EQ(otherBlock.extractWord(2), testWord);
  EXPECT_EQ(otherBlock.extractWord(4), nextWord);
  EXPECT_TRUE(otherBlock.isDirty());

  // reference count
  EXPECT_EQ(block.addRef(), 1);
  EXPECT_EQ(block.addRef(), 2);
  EXPECT_EQ(block.release(), 1);
  EXPECT_EQ(block.release(), 0);

  // expect out of range if access past end
  try {
    otherBlock.extractWord(Block::SECTOR_SIZE);
    FAIL() << "Expected std::out_of_range";
  } catch (out_of_range) {    
  } catch (...) {    
    FAIL() << "Expected std::out_of_range";
  }

  // if we resize the block, it should work
  otherBlock.resize(3, dataSource.get());
  EXPECT_EQ(otherBlock.extractWord(2 * Block::SECTOR_SIZE), testWord);

  // expect IO exception if reading past end
  auto invalidBlock = Block{3, 1};
  try {
    invalidBlock.read(dataSource.get());    
    FAIL() << "Expected filesystem exception";
  } catch (FilesystemException &ex) {
    EXPECT_EQ(ex.getError(), -EIO);
  } catch (...) {
    FAIL() << "Expected filesystem exception";
  }
}

}
