#include "Block.h"
#include "BlockCache.h"
#include "DirConst.h"
#include "Directory.h"
#include "MemoryDataSource.h"
#include "FilesystemException.h"
#include "gtest/gtest.h"

#include <cerrno>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <unistd.h>

using namespace RT11FS;
using namespace RT11FS::Dir;

using std::out_of_range;
using std::make_unique;

namespace {
class DirectoryTest : public ::testing::Test
{
protected:
  static const int sectors = 256;

  DirectoryTest()
  { 
    dataSource = make_unique<MemoryDataSource>(sectors * Block::SECTOR_SIZE);
    blockCache = make_unique<BlockCache>(dataSource.get());
    data = dataSource->getData();
  }

  auto putWord(int offset, uint16_t word);
  auto formatEmpty(int dirSegments, int extraBytes = 0);

  std::unique_ptr<MemoryDataSource> dataSource;
  std::unique_ptr<BlockCache> blockCache;
  uint8_t *data;
};

auto DirectoryTest::putWord(int offset, uint16_t word)
{
  data[offset] = word & 0xff;
  data[offset + 1] = (word >> 8) & 0xff;
}

auto DirectoryTest::formatEmpty(int dirSegments, int extraBytes)
{
  auto offset = FIRST_SEGMENT_SECTOR * Block::SECTOR_SIZE;
  auto dirSectors = dirSegments * SECTORS_PER_SEGMENT;
  auto firstDataSector = FIRST_SEGMENT_SECTOR + dirSectors;
  auto dataSectors = sectors - firstDataSector;

  // header
  putWord(offset + TOTAL_SEGMENTS, dirSegments);
  putWord(offset + NEXT_SEGMENT, 0);
  putWord(offset + HIGHEST_SEGMENT, 1);
  putWord(offset + EXTRA_BYTES, extraBytes);
  putWord(offset + SEGMENT_DATA_BLOCK, firstDataSector);

  // first entry
  offset += FIRST_ENTRY_OFFSET;
  putWord(offset + STATUS_WORD, E_EOS);
  putWord(offset + FILENAME_WORDS + 0, 0);
  putWord(offset + FILENAME_WORDS + 2, 0);
  putWord(offset + FILENAME_WORDS + 4, 0);
  putWord(offset + TOTAL_LENGTH_WORD, dataSectors);
  data[offset + JOB_BYTE] = 0;
  data[offset + CHANNEL_BYTE] = 0;
  putWord(offset + CREATION_DATE_WORD, 0);
}

TEST_F(DirectoryTest, BasicEnumeration)
{
  auto segments = 8;
  formatEmpty(segments);

  auto dir = Directory {blockCache.get()};

  auto dirp = dir.startScan();
  EXPECT_TRUE(dirp.beforeStart());
  EXPECT_FALSE(dirp.afterEnd());

  auto firstDataSector = FIRST_SEGMENT_SECTOR + segments * SECTORS_PER_SEGMENT;

  ++dirp;
  EXPECT_FALSE(dirp.beforeStart());
  EXPECT_FALSE(dirp.afterEnd());
  EXPECT_EQ(dirp.getDataSector(), firstDataSector);
  EXPECT_EQ(dirp.getWord(TOTAL_LENGTH_WORD), sectors - firstDataSector);
  EXPECT_EQ(dirp.getSegment(), 1);
  EXPECT_EQ(dirp.getIndex(), 0);
  EXPECT_EQ(dirp.offset(0), FIRST_ENTRY_OFFSET);
  EXPECT_TRUE(dirp.hasStatus(E_EOS));
  EXPECT_FALSE(dirp.hasStatus(E_PERM));

  ++dirp;
  EXPECT_FALSE(dirp.beforeStart());
  EXPECT_TRUE(dirp.afterEnd());
}

}
