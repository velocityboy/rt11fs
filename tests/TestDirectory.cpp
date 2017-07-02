#include "Block.h"
#include "BlockCache.h"
#include "DirConst.h"
#include "Directory.h"
#include "DirectoryBuilder.h"
#include "MemoryDataSource.h"
#include "FilesystemException.h"
#include "Rad50.h"
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
      Ent {E_MPTY, DirectoryBuilder::REST_OF_DATA},
      Ent {E_EOS},
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
  EXPECT_TRUE(dirp.hasStatus(E_MPTY));
  EXPECT_FALSE(dirp.hasStatus(E_PERM));

  ++dirp;
  EXPECT_TRUE(dirp.hasStatus(E_EOS));

  ++dirp;  
  EXPECT_FALSE(dirp.beforeStart());
  EXPECT_TRUE(dirp.afterEnd());
}

TEST_F(DirectoryTest, GetByName)
{
  auto segments = 8;

  using Ent = DirectoryBuilder::DirEntry;
  vector<vector<Ent>> dirdata = {
    {
      Ent {E_PERM, 2, { 1, 2, 3 }},
      Ent {E_PERM, 2, { 075131, 062000, 075273 }},      // SWAP.SYS
      Ent {E_MPTY, DirectoryBuilder::REST_OF_DATA},
      Ent {E_EOS},
    },
  };

  builder.formatWithEntries(segments, dirdata);

  auto dir = Directory {blockCache.get()};

  auto ent = DirEnt {};
  auto err = dir.getEnt("SWAP.SYS", ent);
  EXPECT_EQ(err, 0);
  EXPECT_EQ(ent.status, E_PERM);
  EXPECT_EQ(ent.length, 2 * Block::SECTOR_SIZE);
  EXPECT_EQ(ent.sector0, 6 + 8 * 2 + 2);

  err = dir.getEnt("NONONO.NOM", ent);
  EXPECT_EQ(err, -ENOENT);
}

TEST_F(DirectoryTest, GetByNameInSecondSegment)
{
  auto segments = 8;

  using Ent = DirectoryBuilder::DirEntry;
  vector<vector<Ent>> dirdata = {
    {
      Ent {E_EOS},
    },
    {
      Ent {E_PERM, 2, { 1, 2, 3 }},
      Ent {E_PERM, 2, { 075131, 062000, 075273 }},      // SWAP.SYS
      Ent {E_MPTY, DirectoryBuilder::REST_OF_DATA},
      Ent {E_EOS},
    },
  };

  builder.formatWithEntries(segments, dirdata);

  auto dir = Directory {blockCache.get()};

  auto ent = DirEnt {};
  auto err = dir.getEnt("SWAP.SYS", ent);
  EXPECT_EQ(err, 0);
  EXPECT_EQ(ent.status, E_PERM);
  EXPECT_EQ(ent.length, 2 * Block::SECTOR_SIZE);
  EXPECT_EQ(ent.sector0, 6 + 8 * 2 + 2);
}

TEST_F(DirectoryTest, GetByRad50)
{
  auto segments = 8;

  using Ent = DirectoryBuilder::DirEntry;
  vector<vector<Ent>> dirdata = {
    {
      Ent {E_PERM, 2, { 1, 2, 3 }},
      Ent {E_PERM, 3, { 075131, 062000, 075273 }},      // SWAP.SYS
      Ent {E_MPTY, DirectoryBuilder::REST_OF_DATA},
      Ent {E_EOS},
    },
  };

  builder.formatWithEntries(segments, dirdata);

  auto dir = Directory {blockCache.get()};

  auto search = Rad50Name { 075131, 062000, 075273 };

  auto dirp = dir.getDirPointer(search);

  EXPECT_FALSE(dirp.beforeStart());
  EXPECT_FALSE(dirp.afterEnd());
  EXPECT_EQ(dirp.getSegment(), 1);
  EXPECT_EQ(dirp.getIndex(), 1);

  auto ent = DirEnt {};
  auto gotent = dir.getEnt(dirp, ent);
  EXPECT_TRUE(gotent);
  EXPECT_EQ(ent.status, E_PERM);
  EXPECT_EQ(ent.length, 3 * Block::SECTOR_SIZE);
  EXPECT_EQ(ent.sector0, 6 + 8 * 2 + 2);

  search = Rad50Name { 075131, 062000, 075274 };

  dirp = dir.getDirPointer(search);

  EXPECT_FALSE(dirp.beforeStart());
  EXPECT_TRUE(dirp.afterEnd());
}

TEST_F(DirectoryTest, GetByRad50InSecondSegment)
{
  auto segments = 8;

  using Ent = DirectoryBuilder::DirEntry;
  vector<vector<Ent>> dirdata = {
    {
      Ent {E_EOS},
    },
    {
      Ent {E_PERM, 2, { 1, 2, 3 }},
      Ent {E_PERM, 2, { 075131, 062000, 075273 }},      // SWAP.SYS
      Ent {E_MPTY, DirectoryBuilder::REST_OF_DATA},
      Ent {E_EOS},
    },
  };

  builder.formatWithEntries(segments, dirdata);

  auto dir = Directory {blockCache.get()};

  auto search = Rad50Name { 075131, 062000, 075273 };

  auto dirp = dir.getDirPointer(search);

  EXPECT_FALSE(dirp.beforeStart());
  EXPECT_FALSE(dirp.afterEnd());
  EXPECT_EQ(dirp.getSegment(), 2);
  EXPECT_EQ(dirp.getIndex(), 1);
}

TEST_F(DirectoryTest, MoveNextFiltered)
{
  auto segments = 8;

  using Ent = DirectoryBuilder::DirEntry;
  vector<vector<Ent>> dirdata = {
    {
      Ent {E_MPTY, 2 },
      Ent {E_PERM, 3, { 075131, 062000, 075273 }},      // SWAP.SYS
      Ent {E_MPTY, DirectoryBuilder::REST_OF_DATA},
      Ent {E_EOS},
    },
  };

  builder.formatWithEntries(segments, dirdata);

  auto dir = Directory {blockCache.get()};

  auto dirp = dir.startScan();

  EXPECT_TRUE(dirp.beforeStart());
  EXPECT_FALSE(dirp.afterEnd());

  auto found = dir.moveNextFiltered(dirp, E_PERM);
  EXPECT_TRUE(found);
  EXPECT_EQ(dirp.getSegment(), 1);
  EXPECT_EQ(dirp.getIndex(), 1);

  found = dir.moveNextFiltered(dirp, E_PERM);
  EXPECT_FALSE(found);
}

TEST_F(DirectoryTest, StatFS)
{
  auto segments = 8;

  using Ent = DirectoryBuilder::DirEntry;
  vector<vector<Ent>> dirdata = {
    {
      Ent {E_MPTY, 2 },
      Ent {E_PERM, 3, { 075131, 062000, 075273 }},      // SWAP.SYS
      Ent {E_MPTY, DirectoryBuilder::REST_OF_DATA},
      Ent {E_EOS}
    },
  };

  builder.formatWithEntries(segments, dirdata);

  auto inodeSpacePerSegment = Block::SECTOR_SIZE * SECTORS_PER_SEGMENT - FIRST_ENTRY_OFFSET;
  auto inodesPerSegment = (inodeSpacePerSegment / ENTRY_LENGTH - 1);
  auto inodes = inodesPerSegment * segments;

  auto dataSectors = sectors - FIRST_SEGMENT_SECTOR - segments * SECTORS_PER_SEGMENT;
  auto availSectors = dataSectors - 3;

  auto dir = Directory {blockCache.get()};

  struct statvfs st;
  auto err = dir.statfs(&st);
  EXPECT_EQ(err, 0);

  EXPECT_EQ(st.f_bfree, availSectors);
  EXPECT_EQ(st.f_bavail, availSectors);
  EXPECT_EQ(st.f_files, inodes);
  EXPECT_EQ(st.f_ffree, inodes - 1);
  EXPECT_EQ(st.f_favail, inodes - 1);
}
}
