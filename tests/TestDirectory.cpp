#include "Block.h"
#include "BlockCache.h"
#include "DirConst.h"
#include "Directory.h"
#include "DirectoryBuilder.h"
#include "MemoryDataSource.h"
#include "FilesystemException.h"
#include "gtest/gtest.h"

#include <array>
#include <cerrno>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <unistd.h>
#include <vector>

using namespace RT11FS;
using namespace RT11FS::Dir;

using std::array;
using std::copy;
using std::out_of_range;
using std::make_unique;
using std::vector;

namespace {

class DirectoryTest : public ::testing::Test
{
protected:
  static const int sectors = 256;

  DirectoryTest()
    : dataSource(make_unique<MemoryDataSource>(sectors * Block::SECTOR_SIZE))
    , blockCache(make_unique<BlockCache>(dataSource.get()))
    , data(dataSource->getData())
    , builder(*dataSource.get())
  { 
  }

  std::unique_ptr<MemoryDataSource> dataSource;
  std::unique_ptr<BlockCache> blockCache;
  std::vector<uint8_t> &data;
  DirectoryBuilder builder;
};

TEST_F(DirectoryTest, BasicEnumeration)
{
  auto segments = 8;

  using Ent = DirectoryBuilder::DirEntry;
  vector<vector<Ent>> dirdata = {
    {
      Ent {E_PERM, 2, { 1, 2, 3 }},
      Ent {E_EOS, DirectoryBuilder::REST_OF_DATA}
    },
  };

  builder.formatWithEntries(segments, dirdata);

  auto dir = Directory {blockCache.get()};

  auto dirp = dir.startScan();
  EXPECT_TRUE(dirp.beforeStart());
  EXPECT_FALSE(dirp.afterEnd());

  auto firstDataSector = FIRST_SEGMENT_SECTOR + segments * SECTORS_PER_SEGMENT;

  ++dirp;
  EXPECT_FALSE(dirp.beforeStart());
  EXPECT_FALSE(dirp.afterEnd());

  EXPECT_EQ(dirp.getDataSector(), firstDataSector);
  EXPECT_EQ(dirp.getWord(TOTAL_LENGTH_WORD), 2);
  EXPECT_EQ(dirp.getSegment(), 1);
  EXPECT_EQ(dirp.getIndex(), 0);
  EXPECT_EQ(dirp.offset(0), FIRST_ENTRY_OFFSET);
  EXPECT_FALSE(dirp.hasStatus(E_EOS));
  EXPECT_TRUE(dirp.hasStatus(E_PERM));

  ++dirp;
  EXPECT_EQ(dirp.getDataSector(), firstDataSector + 2);
  EXPECT_EQ(dirp.getWord(TOTAL_LENGTH_WORD), sectors - firstDataSector - 2);
  EXPECT_EQ(dirp.getSegment(), 1);
  EXPECT_EQ(dirp.getIndex(), 1);
  EXPECT_EQ(dirp.offset(0), FIRST_ENTRY_OFFSET + ENTRY_LENGTH);
  EXPECT_TRUE(dirp.hasStatus(E_EOS));
  EXPECT_FALSE(dirp.hasStatus(E_PERM));

  ++dirp;
  EXPECT_FALSE(dirp.beforeStart());
  EXPECT_TRUE(dirp.afterEnd());
}

}
