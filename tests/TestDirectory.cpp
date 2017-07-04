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
#include <cstdlib>
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

  static auto segmentsPerEntry(int extraBytes = 0) 
  {
    return (SECTORS_PER_SEGMENT * Block::SECTOR_SIZE - FIRST_ENTRY_OFFSET) / (ENTRY_LENGTH + extraBytes);
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

  auto dir = Directory {blockCache.get()};

  auto inodesPerSegment =  segmentsPerEntry() - 1;
  auto inodes = inodesPerSegment * segments;

  auto dataSectors = sectors - FIRST_SEGMENT_SECTOR - segments * SECTORS_PER_SEGMENT;
  auto availSectors = dataSectors - 3;


  struct statvfs st;
  auto err = dir.statfs(&st);
  EXPECT_EQ(err, 0);

  EXPECT_EQ(st.f_bfree, availSectors);
  EXPECT_EQ(st.f_bavail, availSectors);
  EXPECT_EQ(st.f_files, inodes);
  EXPECT_EQ(st.f_ffree, inodes - 1);
  EXPECT_EQ(st.f_favail, inodes - 1);
}

TEST_F(DirectoryTest, TruncateShrinkSimple)
{
  auto segments = 8;
  auto swapFilename = Rad50Name { 075131, 062000, 075273 };   // SWAP.SYS

  using Ent = DirectoryBuilder::DirEntry;
  vector<vector<Ent>> dirdata = {
    {
      Ent {E_MPTY, 2 },
      Ent {E_PERM, 3, swapFilename },      
      Ent {E_MPTY, DirectoryBuilder::REST_OF_DATA},
      Ent {E_EOS}
    },
  };

  builder.formatWithEntries(segments, dirdata);

  auto dir = Directory {blockCache.get()};
  auto dirp = dir.getDirPointer(swapFilename);
  auto tailp = dirp.next();

  auto tailLength = tailp.getWord(TOTAL_LENGTH_WORD);

  EXPECT_EQ(dirp.getIndex(), 1);

  EXPECT_EQ(dir.truncate(dirp, 0), 0);

  EXPECT_EQ(dirp.getSegment(), 1);
  EXPECT_EQ(dirp.getIndex(), 1);

  EXPECT_EQ(dirp.getWord(STATUS_WORD), E_PERM);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 0), swapFilename[0]);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 2), swapFilename[1]);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 4), swapFilename[2]);
  EXPECT_EQ(dirp.getWord(TOTAL_LENGTH_WORD), 0);
  EXPECT_EQ(dirp.getByte(JOB_BYTE), 0);
  EXPECT_EQ(dirp.getByte(CHANNEL_BYTE), 0);

  EXPECT_EQ(tailp.getWord(STATUS_WORD), E_MPTY);
  EXPECT_EQ(tailp.getWord(TOTAL_LENGTH_WORD), tailLength + 3);

  ++tailp;

  EXPECT_EQ(tailp.getWord(STATUS_WORD), E_EOS);    
}

TEST_F(DirectoryTest, TruncateGrowSimple)
{
  auto segments = 8;
  auto swapFilename = Rad50Name { 075131, 062000, 075273 };   // SWAP.SYS

  using Ent = DirectoryBuilder::DirEntry;
  vector<vector<Ent>> dirdata = {
    {
      Ent {E_MPTY, 2 },
      Ent {E_PERM, 3, swapFilename },      
      Ent {E_MPTY, DirectoryBuilder::REST_OF_DATA},
      Ent {E_EOS}
    },
  };

  builder.formatWithEntries(segments, dirdata);

  auto dir = Directory {blockCache.get()};
  auto dirp = dir.getDirPointer(swapFilename);
  auto tailp = dirp.next();

  auto tailLength = tailp.getWord(TOTAL_LENGTH_WORD);

  EXPECT_EQ(dirp.getIndex(), 1);

  EXPECT_EQ(dir.truncate(dirp, 6 * Block::SECTOR_SIZE), 0);

  EXPECT_EQ(dirp.getSegment(), 1);
  EXPECT_EQ(dirp.getIndex(), 1);

  EXPECT_EQ(dirp.getWord(STATUS_WORD), E_PERM);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 0), swapFilename[0]);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 2), swapFilename[1]);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 4), swapFilename[2]);
  EXPECT_EQ(dirp.getWord(TOTAL_LENGTH_WORD), 6);
  EXPECT_EQ(dirp.getByte(JOB_BYTE), 0);
  EXPECT_EQ(dirp.getByte(CHANNEL_BYTE), 0);

  EXPECT_EQ(tailp.getWord(STATUS_WORD), E_MPTY);
  EXPECT_EQ(tailp.getWord(TOTAL_LENGTH_WORD), tailLength - 3);

  ++tailp;

  EXPECT_EQ(tailp.getWord(STATUS_WORD), E_EOS);    
}

TEST_F(DirectoryTest, TruncateGrowSizeRounding)
{
  auto segments = 8;
  auto swapFilename = Rad50Name { 075131, 062000, 075273 };   // SWAP.SYS

  using Ent = DirectoryBuilder::DirEntry;
  vector<vector<Ent>> dirdata = {
    {
      Ent {E_MPTY, 2 },
      Ent {E_PERM, 3, swapFilename },      
      Ent {E_MPTY, DirectoryBuilder::REST_OF_DATA},
      Ent {E_EOS}
    },
  };

  builder.formatWithEntries(segments, dirdata);

  auto dir = Directory {blockCache.get()};
  auto dirp = dir.getDirPointer(swapFilename);
  auto tailp = dirp.next();

  auto tailLength = tailp.getWord(TOTAL_LENGTH_WORD);

  EXPECT_EQ(dirp.getIndex(), 1);

  EXPECT_EQ(dir.truncate(dirp, 5 * Block::SECTOR_SIZE + 1), 0);

  EXPECT_EQ(dirp.getSegment(), 1);
  EXPECT_EQ(dirp.getIndex(), 1);

  EXPECT_EQ(dirp.getWord(STATUS_WORD), E_PERM);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 0), swapFilename[0]);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 2), swapFilename[1]);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 4), swapFilename[2]);
  EXPECT_EQ(dirp.getWord(TOTAL_LENGTH_WORD), 6);
  EXPECT_EQ(dirp.getByte(JOB_BYTE), 0);
  EXPECT_EQ(dirp.getByte(CHANNEL_BYTE), 0);

  EXPECT_EQ(tailp.getWord(STATUS_WORD), E_MPTY);
  EXPECT_EQ(tailp.getWord(TOTAL_LENGTH_WORD), tailLength - 3);

  ++tailp;

  EXPECT_EQ(tailp.getWord(STATUS_WORD), E_EOS);    
}

TEST_F(DirectoryTest, TruncateShrinkWithInsert)
{
  auto segments = 8;
  auto swapFilename = Rad50Name { 075131, 062000, 075273 };   // SWAP.SYS

  using Ent = DirectoryBuilder::DirEntry;
  vector<vector<Ent>> dirdata = {
    {
      Ent {E_MPTY, 2 },
      Ent {E_PERM, 3, swapFilename },
      // code will have to insert a free block right here.
      Ent {E_PERM, 5, Rad50Name {1, 2, 3}},      
      Ent {E_MPTY, DirectoryBuilder::REST_OF_DATA},
      Ent {E_EOS}
    },
  };

  builder.formatWithEntries(segments, dirdata);

  auto dir = Directory {blockCache.get()};
  auto dirp = dir.getDirPointer(swapFilename);
  auto nextp = dirp.next();
  auto nextSector = nextp.getDataSector();
  auto tailp = nextp.next();
  auto tailSectors = tailp.getWord(TOTAL_LENGTH_WORD);

  EXPECT_EQ(dir.truncate(dirp, 0), 0);

  EXPECT_EQ(dirp.getSegment(), 1);
  EXPECT_EQ(dirp.getIndex(), 1);

  // dirp should point to an entry that just has the length changed
  EXPECT_EQ(dirp.getWord(STATUS_WORD), E_PERM);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 0), swapFilename[0]);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 2), swapFilename[1]);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 4), swapFilename[2]);
  EXPECT_EQ(dirp.getWord(TOTAL_LENGTH_WORD), 0);
  EXPECT_EQ(dirp.getByte(JOB_BYTE), 0);
  EXPECT_EQ(dirp.getByte(CHANNEL_BYTE), 0);

  ++dirp;

  // the next entry should be a new free space entry with the right size
  EXPECT_EQ(dirp.getWord(STATUS_WORD), E_MPTY);
  EXPECT_EQ(dirp.getWord(TOTAL_LENGTH_WORD), 3);
  EXPECT_EQ(dirp.getByte(JOB_BYTE), 0);
  EXPECT_EQ(dirp.getByte(CHANNEL_BYTE), 0);

  ++dirp;

  // next should be the real file that originally followed SWAP.SYS
  EXPECT_EQ(dirp.getDataSector(), nextSector);
  EXPECT_EQ(dirp.getWord(STATUS_WORD), E_PERM);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 0), 1);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 2), 2);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 4), 3);
  EXPECT_EQ(dirp.getWord(TOTAL_LENGTH_WORD), 5);
  EXPECT_EQ(dirp.getByte(JOB_BYTE), 0);
  EXPECT_EQ(dirp.getByte(CHANNEL_BYTE), 0);

  ++dirp;

  // next should be the rest-of-space entry, which should not have changed length
  EXPECT_EQ(dirp.getWord(STATUS_WORD), E_MPTY);
  EXPECT_EQ(dirp.getWord(TOTAL_LENGTH_WORD), tailSectors);
  EXPECT_EQ(dirp.getByte(JOB_BYTE), 0);
  EXPECT_EQ(dirp.getByte(CHANNEL_BYTE), 0);

  ++dirp;

  // and finally we should still have the end of segment marker
  EXPECT_EQ(dirp.getWord(STATUS_WORD), E_EOS);
}

TEST_F(DirectoryTest, TruncateGrowWithMove)
{
  auto segments = 8;
  auto swapFilename = Rad50Name { 075131, 062000, 075273 };   // SWAP.SYS

  using Ent = DirectoryBuilder::DirEntry;
  vector<vector<Ent>> dirdata = {
    {
      Ent {E_MPTY, 2 },
      Ent {E_PERM, 3, swapFilename },
      Ent {E_PERM, 5, Rad50Name {1, 2, 3}},      
      // swap file will move here
      Ent {E_MPTY, DirectoryBuilder::REST_OF_DATA},
      Ent {E_EOS}
    },
  };

  builder.formatWithEntries(segments, dirdata);

  auto dir = Directory {blockCache.get()};
  auto dirp = dir.getDirPointer(swapFilename);
  auto nextp = dirp.next();
  auto nextSector = nextp.getDataSector();
  auto tailp = nextp.next();
  auto tailSectors = tailp.getWord(TOTAL_LENGTH_WORD);


  srand(1);
  for (auto i = 0; i < 3; i++) {
    auto block = blockCache->getBlock(dirp.getDataSector() + i, 1);
    for (auto j = 0; j < Block::SECTOR_SIZE; j++) {
      block->setByte(j, rand() & 0xff);
    }
    blockCache->putBlock(block);
  }

  EXPECT_EQ(dir.truncate(dirp, 6 * Block::SECTOR_SIZE), 0);

  EXPECT_EQ(dirp.getSegment(), 1);
  EXPECT_EQ(dirp.getIndex(), 2);

  dirp = dir.startScan();

  ++dirp;

  // the original entry should combine with the preceding free block
  EXPECT_EQ(dirp.getWord(STATUS_WORD), E_MPTY);
  EXPECT_EQ(dirp.getWord(TOTAL_LENGTH_WORD), 5);
  EXPECT_EQ(dirp.getByte(JOB_BYTE), 0);
  EXPECT_EQ(dirp.getByte(CHANNEL_BYTE), 0);

  ++dirp;

  // next should be the real file that originally followed SWAP.SYS
  EXPECT_EQ(dirp.getDataSector(), nextSector);
  EXPECT_EQ(dirp.getWord(STATUS_WORD), E_PERM);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 0), 1);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 2), 2);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 4), 3);
  EXPECT_EQ(dirp.getWord(TOTAL_LENGTH_WORD), 5);
  EXPECT_EQ(dirp.getByte(JOB_BYTE), 0);
  EXPECT_EQ(dirp.getByte(CHANNEL_BYTE), 0);

  ++dirp;

  // now we should have the moved file
  EXPECT_EQ(dirp.getWord(STATUS_WORD), E_PERM);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 0), swapFilename[0]);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 2), swapFilename[1]);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 4), swapFilename[2]);
  EXPECT_EQ(dirp.getWord(TOTAL_LENGTH_WORD), 6);
  EXPECT_EQ(dirp.getByte(JOB_BYTE), 0);
  EXPECT_EQ(dirp.getByte(CHANNEL_BYTE), 0);

  // make sure the data was also moved
  srand(1);
  auto blockDataMatched = true;
  for (auto i = 0; i < 3 && blockDataMatched; i++) {
    auto block = blockCache->getBlock(dirp.getDataSector() + i, 1);
    for (auto j = 0; j < Block::SECTOR_SIZE && blockDataMatched; j++) {
      blockDataMatched = (block->getByte(j) == (rand() & 0xff));
    }
    blockCache->putBlock(block);
  }

  EXPECT_TRUE(blockDataMatched);

  ++dirp;

  // the tail block should have shrunk to accomodate the moved file
  EXPECT_EQ(dirp.getWord(STATUS_WORD), E_MPTY);
  EXPECT_EQ(dirp.getWord(TOTAL_LENGTH_WORD), tailSectors - 6);
  EXPECT_EQ(dirp.getByte(JOB_BYTE), 0);
  EXPECT_EQ(dirp.getByte(CHANNEL_BYTE), 0);

  ++dirp;

  // and finally we should still have the end of segment marker
  EXPECT_EQ(dirp.getWord(STATUS_WORD), E_EOS);
}

TEST_F(DirectoryTest, TruncateShrinkWithSpill)
{
  auto segments = 8;
  auto swapFilename = Rad50Name { 075131, 062000, 075273 };   // SWAP.SYS

  using Ent = DirectoryBuilder::DirEntry;
  vector<vector<Ent>> dirdata = {
    {
      Ent {E_PERM, 3, swapFilename },
    },
    {
      Ent {E_MPTY, DirectoryBuilder::REST_OF_DATA},
      Ent {E_EOS}
    },
  };

  // The scenario we're building (n == max entries per segment)
  // Segment 1: 
  //   0: file to shrink
  //   1..n-2: permanent, 1 sector files
  //   n-1: EOS
  // Segment 2:
  //   0: rest-of-data empty sector
  //   1: EOS
  // truncating 1:0 to shrink will make everything in segment 1 have to move 
  // down, and the last file entry will have to move to segment 2

  auto entries = segmentsPerEntry();

  auto &firstSeg = dirdata[0];
  auto index = uint16_t {1};

  while (firstSeg.size() < entries - 1) {
    firstSeg.push_back(Ent {
      E_PERM,
      1,
      Rad50Name {index, index, index}
    });
    index++;
  }
  firstSeg.push_back(Ent{E_EOS});

  builder.formatWithEntries(segments, dirdata);

  auto dir = Directory {blockCache.get()};
    
  uint16_t lastFile = index - 1;
  auto dirp = dir.getDirPointer(Rad50Name {lastFile, lastFile, lastFile});
  auto lastFileSector = dirp.getDataSector();

  dirp = dir.getDirPointer(swapFilename);

  EXPECT_EQ(dir.truncate(dirp, 0), 0);
    
  EXPECT_EQ(dirp.getSegment(), 1);
  EXPECT_EQ(dirp.getIndex(), 0);

  dirp = dir.startScan();
  ++dirp;

  // we should have the original entry, but with zero length
  EXPECT_EQ(dirp.getWord(STATUS_WORD), E_PERM);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 0), swapFilename[0]);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 2), swapFilename[1]);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 4), swapFilename[2]);
  EXPECT_EQ(dirp.getWord(TOTAL_LENGTH_WORD), 0);
  EXPECT_EQ(dirp.getByte(JOB_BYTE), 0);
  EXPECT_EQ(dirp.getByte(CHANNEL_BYTE), 0);

  ++dirp;

  // now we should have a free block of the size the file used to be
  EXPECT_EQ(dirp.getWord(STATUS_WORD), E_MPTY);
  EXPECT_EQ(dirp.getWord(TOTAL_LENGTH_WORD), 3);
  EXPECT_EQ(dirp.getByte(JOB_BYTE), 0);
  EXPECT_EQ(dirp.getByte(CHANNEL_BYTE), 0);

  // we should have one less than the number of files we had before
  for (auto i = 1; i < index - 1; i++) {
    ++dirp;
    EXPECT_EQ(dirp.getWord(STATUS_WORD), E_PERM);
    EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 0), i);
    EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 2), i);
    EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 4), i);
    EXPECT_EQ(dirp.getWord(TOTAL_LENGTH_WORD), 1);
    EXPECT_EQ(dirp.getByte(JOB_BYTE), 0);
    EXPECT_EQ(dirp.getByte(CHANNEL_BYTE), 0);
  }

  ++dirp;

  // now we should have this segment's end
  EXPECT_EQ(dirp.getWord(STATUS_WORD), E_EOS);

  ++dirp;

  // We should be on to the next segment and have the spilled index entry
  EXPECT_EQ(dirp.getSegment(), 2);
  EXPECT_EQ(dirp.getIndex(), 0);

  EXPECT_EQ(dirp.getWord(STATUS_WORD), E_PERM);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 0), index - 1);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 2), index - 1);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 4), index - 1);
  EXPECT_EQ(dirp.getWord(TOTAL_LENGTH_WORD), 1);
  EXPECT_EQ(dirp.getByte(JOB_BYTE), 0);
  EXPECT_EQ(dirp.getByte(CHANNEL_BYTE), 0);
  EXPECT_EQ(dirp.getDataSector(), lastFileSector);

  ++dirp;

  // free space for rest of volumne
  EXPECT_EQ(dirp.getWord(STATUS_WORD), E_MPTY);
  EXPECT_EQ(dirp.getWord(TOTAL_LENGTH_WORD), sectors - lastFileSector - 1);

  ++dirp;
  EXPECT_EQ(dirp.getWord(STATUS_WORD), E_EOS);

  // Make doubly sure the starting sector of the second segment has been 
  // updated
  EXPECT_EQ(dirp.getSegmentWord(SEGMENT_DATA_BLOCK), lastFileSector);
}

TEST_F(DirectoryTest, TruncateShrinkAndDeleteFree)
{
  auto segments = 8;
  auto swapFilename = Rad50Name { 075131, 062000, 075273 };   // SWAP.SYS

  using Ent = DirectoryBuilder::DirEntry;
  vector<vector<Ent>> dirdata = {
    {
      Ent {E_MPTY, 2},
      Ent {E_PERM, 3, swapFilename},  // we will grow this to 6 and subsume the following free entry
      Ent {E_MPTY, 3},      
      Ent {E_PERM, 5, Rad50Name {1, 2, 3}},      
      Ent {E_MPTY, DirectoryBuilder::REST_OF_DATA},
      Ent {E_EOS}
    },
  };

  builder.formatWithEntries(segments, dirdata);

  auto dir = Directory {blockCache.get()};
  auto dirp = dir.getDirPointer(swapFilename);
  auto nextp = dirp.next().next();
  auto secondFileSector = nextp.getDataSector();
  auto tailp = nextp.next();
  auto tailSectors = tailp.getWord(TOTAL_LENGTH_WORD);

  EXPECT_EQ(dir.truncate(dirp, 6 * Block::SECTOR_SIZE), 0);

  EXPECT_EQ(dirp.getSegment(), 1);
  EXPECT_EQ(dirp.getIndex(), 1);

  // dirp should point to an entry that just has the length changed
  EXPECT_EQ(dirp.getWord(STATUS_WORD), E_PERM);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 0), swapFilename[0]);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 2), swapFilename[1]);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 4), swapFilename[2]);
  EXPECT_EQ(dirp.getWord(TOTAL_LENGTH_WORD), 6);
  EXPECT_EQ(dirp.getByte(JOB_BYTE), 0);
  EXPECT_EQ(dirp.getByte(CHANNEL_BYTE), 0);

  ++dirp;

  // the free space entry should be gone, replaced by the real second file
  EXPECT_EQ(dirp.getWord(STATUS_WORD), E_PERM);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 0), 1);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 2), 2);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 4), 3);
  EXPECT_EQ(dirp.getWord(TOTAL_LENGTH_WORD), 5);
  EXPECT_EQ(dirp.getByte(JOB_BYTE), 0);
  EXPECT_EQ(dirp.getByte(CHANNEL_BYTE), 0);
  EXPECT_EQ(dirp.getDataSector(), secondFileSector);

  ++dirp;

  // next should be the rest-of-space entry, which should not have changed length
  EXPECT_EQ(dirp.getWord(STATUS_WORD), E_MPTY);
  EXPECT_EQ(dirp.getWord(TOTAL_LENGTH_WORD), tailSectors);
  EXPECT_EQ(dirp.getByte(JOB_BYTE), 0);
  EXPECT_EQ(dirp.getByte(CHANNEL_BYTE), 0);

  ++dirp;

  // and finally we should still have the end of segment marker
  EXPECT_EQ(dirp.getWord(STATUS_WORD), E_EOS);
}

TEST_F(DirectoryTest, TruncateShrinkAndMergeFree)
{
  auto segments = 8;
  auto swapFilename = Rad50Name { 075131, 062000, 075273 };   // SWAP.SYS

  using Ent = DirectoryBuilder::DirEntry;
  vector<vector<Ent>> dirdata = {
    {
      Ent {E_MPTY, 2},
      Ent {E_PERM, 3, swapFilename},  // we will grow this to 6 and subsume the following free entry
      Ent {E_MPTY, 3},      
      Ent {E_PERM, 5, Rad50Name {1, 2, 3}},      
      Ent {E_MPTY, DirectoryBuilder::REST_OF_DATA},
      Ent {E_EOS}
    },
  };

  builder.formatWithEntries(segments, dirdata);

  auto dir = Directory {blockCache.get()};
  auto dirp = dir.getDirPointer(swapFilename);
  auto nextp = dirp.next().next();
  auto secondFileSector = nextp.getDataSector();
  auto tailp = nextp.next();
  auto tailSectors = tailp.getWord(TOTAL_LENGTH_WORD);

  srand(1);
  for (auto i = 0; i < 3; i++) {
    auto block = blockCache->getBlock(dirp.getDataSector() + i, 1);
    for (auto j = 0; j < Block::SECTOR_SIZE; j++) {
      block->setByte(j, rand() & 0xff);
    }
    blockCache->putBlock(block);
  }

  EXPECT_EQ(dir.truncate(dirp, 7 * Block::SECTOR_SIZE), 0);

  EXPECT_EQ(dirp.getSegment(), 1);
  EXPECT_EQ(dirp.getIndex(), 2);

  dirp = dir.startScan();
  ++dirp;

  // since the file won't fit where it was, its space and the following free block 
  // should get merged
  EXPECT_EQ(dirp.getWord(STATUS_WORD), E_MPTY);
  EXPECT_EQ(dirp.getWord(TOTAL_LENGTH_WORD), 8);
  EXPECT_EQ(dirp.getByte(JOB_BYTE), 0);
  EXPECT_EQ(dirp.getByte(CHANNEL_BYTE), 0);

  ++dirp;

  // that should still be followed by the real file
  EXPECT_EQ(dirp.getWord(STATUS_WORD), E_PERM);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 0), 1);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 2), 2);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 4), 3);
  EXPECT_EQ(dirp.getWord(TOTAL_LENGTH_WORD), 5);
  EXPECT_EQ(dirp.getByte(JOB_BYTE), 0);
  EXPECT_EQ(dirp.getByte(CHANNEL_BYTE), 0);
  EXPECT_EQ(dirp.getDataSector(), secondFileSector);

  ++dirp;

  // next should be the moved file
  EXPECT_EQ(dirp.getWord(STATUS_WORD), E_PERM);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 0), swapFilename[0]);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 2), swapFilename[1]);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 4), swapFilename[2]);
  EXPECT_EQ(dirp.getWord(TOTAL_LENGTH_WORD), 7);
  EXPECT_EQ(dirp.getByte(JOB_BYTE), 0);
  EXPECT_EQ(dirp.getByte(CHANNEL_BYTE), 0);

  // the first 3 sectors should be the same
  srand(1);
  auto blockDataMatched = true;
  for (auto i = 0; i < 3 && blockDataMatched; i++) {
    auto block = blockCache->getBlock(dirp.getDataSector() + i, 1);
    for (auto j = 0; j < Block::SECTOR_SIZE && blockDataMatched; j++) {
      blockDataMatched = (block->getByte(j) == (rand() & 0xff));
    }
    blockCache->putBlock(block);
  }

  EXPECT_TRUE(blockDataMatched);

  ++dirp;

  // next should be the rest of free space
  EXPECT_EQ(dirp.getWord(STATUS_WORD), E_MPTY);
  EXPECT_EQ(dirp.getWord(TOTAL_LENGTH_WORD), tailSectors - 7);

  ++dirp;

  // and finally we should still have the end of segment marker
  EXPECT_EQ(dirp.getWord(STATUS_WORD), E_EOS);
}

TEST_F(DirectoryTest, TruncateShrinkWithSpillToAllocatedSegment)
{
  auto segments = 8;
  auto swapFilename = Rad50Name { 075131, 062000, 075273 };   // SWAP.SYS

  using Ent = DirectoryBuilder::DirEntry;
  vector<vector<Ent>> dirdata = {
    {
      Ent {E_PERM, 3, swapFilename },
    },
  };

  // The scenario we're building (n == max entries per segment)
  // Segment 1: 
  //   0: file to shrink
  //   1..n-2: permanent, 1 sector files
  //   n-1: EOS
  // No allocated second segment
  //
  // truncating 1:0 to shrink will make everything in segment 1 have to move 
  // down, and the last file entry will have to move to segment 2 (which will
  // be allocated)

  auto entries = segmentsPerEntry();

  auto &firstSeg = dirdata[0];
  auto index = uint16_t {1};

  while (firstSeg.size() < entries - 1) {
    firstSeg.push_back(Ent {
      E_PERM,
      1,
      Rad50Name {index, index, index}
    });
    index++;
  }
  firstSeg.push_back(Ent{E_EOS});

  builder.formatWithEntries(segments, dirdata);

  auto dir = Directory {blockCache.get()};
    
  uint16_t lastFile = index - 1;
  auto dirp = dir.getDirPointer(Rad50Name {lastFile, lastFile, lastFile});
  auto lastFileSector = dirp.getDataSector();

  dirp = dir.getDirPointer(swapFilename);

  // make sure the directory builder put togther the headers we expect
  EXPECT_EQ(dirp.getSegmentWord(TOTAL_SEGMENTS), segments);
  EXPECT_EQ(dirp.getSegmentWord(NEXT_SEGMENT), 0);
  EXPECT_EQ(dirp.getSegmentWord(HIGHEST_SEGMENT), 1);

  EXPECT_EQ(dir.truncate(dirp, 0), 0);

  EXPECT_EQ(dirp.getSegment(), 1);
  EXPECT_EQ(dirp.getIndex(), 0);
    
  dirp = dir.startScan();
  ++dirp;

  // we should have the original entry, but with zero length
  EXPECT_EQ(dirp.getWord(STATUS_WORD), E_PERM);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 0), swapFilename[0]);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 2), swapFilename[1]);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 4), swapFilename[2]);
  EXPECT_EQ(dirp.getWord(TOTAL_LENGTH_WORD), 0);
  EXPECT_EQ(dirp.getByte(JOB_BYTE), 0);
  EXPECT_EQ(dirp.getByte(CHANNEL_BYTE), 0);

  ++dirp;

  // now we should have a free block of the size the file used to be
  EXPECT_EQ(dirp.getWord(STATUS_WORD), E_MPTY);
  EXPECT_EQ(dirp.getWord(TOTAL_LENGTH_WORD), 3);
  EXPECT_EQ(dirp.getByte(JOB_BYTE), 0);
  EXPECT_EQ(dirp.getByte(CHANNEL_BYTE), 0);

  // we should have one less than the number of files we had before
  for (auto i = 1; i < index - 1; i++) {
    ++dirp;
    EXPECT_EQ(dirp.getWord(STATUS_WORD), E_PERM);
    EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 0), i);
    EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 2), i);
    EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 4), i);
    EXPECT_EQ(dirp.getWord(TOTAL_LENGTH_WORD), 1);
    EXPECT_EQ(dirp.getByte(JOB_BYTE), 0);
    EXPECT_EQ(dirp.getByte(CHANNEL_BYTE), 0);
  }

  ++dirp;

  // now we should have this segment's end
  EXPECT_EQ(dirp.getWord(STATUS_WORD), E_EOS);

  ++dirp;

  // We should be on to the next segment and have the spilled index entry
  EXPECT_EQ(dirp.getSegment(), 2);
  EXPECT_EQ(dirp.getIndex(), 0);

  EXPECT_EQ(dirp.getWord(STATUS_WORD), E_PERM);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 0), index - 1);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 2), index - 1);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 4), index - 1);
  EXPECT_EQ(dirp.getWord(TOTAL_LENGTH_WORD), 1);
  EXPECT_EQ(dirp.getByte(JOB_BYTE), 0);
  EXPECT_EQ(dirp.getByte(CHANNEL_BYTE), 0);
  EXPECT_EQ(dirp.getDataSector(), lastFileSector);

  ++dirp;
  EXPECT_EQ(dirp.getWord(STATUS_WORD), E_EOS);

  // Make doubly sure the starting sector of the second segment has been 
  // updated
  EXPECT_EQ(dirp.getSegmentWord(SEGMENT_DATA_BLOCK), lastFileSector);
}

TEST_F(DirectoryTest, TruncateShrinkWithNoRoom)
{
  auto segments = 1;
  auto swapFilename = Rad50Name { 075131, 062000, 075273 };   // SWAP.SYS

  using Ent = DirectoryBuilder::DirEntry;
  vector<vector<Ent>> dirdata = {
    {
      Ent {E_PERM, 3, swapFilename },
    },
  };

  // The scenario we're building (n == max entries per segment)
  // Segment 1: 
  //   0: file to shrink
  //   1..n-2: permanent, 1 sector files
  //   n-1: EOS
  // No allocated second segment
  //
  // Since there is no segment 2, we should fail with out of space

  auto entries = segmentsPerEntry();

  auto &firstSeg = dirdata[0];
  auto index = uint16_t {1};

  while (firstSeg.size() < entries - 1) {
    firstSeg.push_back(Ent {
      E_PERM,
      1,
      Rad50Name {index, index, index}
    });
    index++;
  }
  firstSeg.push_back(Ent{E_EOS});

  builder.formatWithEntries(segments, dirdata);

  auto dir = Directory {blockCache.get()};
    
  auto dirp = dir.getDirPointer(swapFilename);
  auto sector = dirp.getDataSector();

  EXPECT_EQ(dir.truncate(dirp, 0), -ENOSPC);

  // since we had an error, nothing should have been disturbed
  dirp = dir.startScan();

  ++dirp;
  EXPECT_EQ(dirp.getWord(STATUS_WORD), E_PERM);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 0), swapFilename[0]);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 2), swapFilename[1]);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 4), swapFilename[2]);
  EXPECT_EQ(dirp.getWord(TOTAL_LENGTH_WORD), 3);
  EXPECT_EQ(dirp.getByte(JOB_BYTE), 0);
  EXPECT_EQ(dirp.getByte(CHANNEL_BYTE), 0);
  EXPECT_EQ(dirp.getDataSector(), sector);

  sector += 3;

  for (auto i = 1; i < index; i++) {
    ++dirp;

    EXPECT_EQ(dirp.getWord(STATUS_WORD), E_PERM);
    EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 0), i);
    EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 2), i);
    EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 4), i);
    EXPECT_EQ(dirp.getWord(TOTAL_LENGTH_WORD), 1);
    EXPECT_EQ(dirp.getByte(JOB_BYTE), 0);
    EXPECT_EQ(dirp.getByte(CHANNEL_BYTE), 0);
    EXPECT_EQ(dirp.getDataSector(), sector);
    sector++;
  }

  ++dirp;
  EXPECT_EQ(dirp.getWord(STATUS_WORD), E_EOS);
}

TEST_F(DirectoryTest, TruncateGrowWithNoSpace)
{
  auto segments = 1;
  auto swapFilename = Rad50Name { 075131, 062000, 075273 };   // SWAP.SYS

  using Ent = DirectoryBuilder::DirEntry;
  vector<vector<Ent>> dirdata = {
    {
      Ent {E_MPTY, 2 },
      Ent {E_PERM, 3, swapFilename },
      Ent {E_PERM, sectors - (6 + 2 + 2 + 3 + 3), Rad50Name {1, 2, 3}},      
      Ent {E_MPTY, 3 },
      Ent {E_EOS}
    },
  };

  builder.formatWithEntries(segments, dirdata);

  auto dir = Directory {blockCache.get()};

  auto dirp = dir.getDirPointer(swapFilename);
  EXPECT_EQ(dir.truncate(dirp, 6 * Block::SECTOR_SIZE), -ENOSPC);

  // ensure nothing changed
  dirp = dir.startScan();

  ++dirp;
  EXPECT_EQ(dirp.getWord(STATUS_WORD), E_MPTY);
  EXPECT_EQ(dirp.getWord(TOTAL_LENGTH_WORD), 2);

  ++dirp;
  EXPECT_EQ(dirp.getWord(STATUS_WORD), E_PERM);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 0), swapFilename[0]);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 2), swapFilename[1]);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 4), swapFilename[2]);
  EXPECT_EQ(dirp.getWord(TOTAL_LENGTH_WORD), 3);
  EXPECT_EQ(dirp.getByte(JOB_BYTE), 0);
  EXPECT_EQ(dirp.getByte(CHANNEL_BYTE), 0);

  ++dirp;
  EXPECT_EQ(dirp.getWord(STATUS_WORD), E_PERM);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 0), 1);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 2), 2);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 4), 3);
  EXPECT_EQ(dirp.getWord(TOTAL_LENGTH_WORD), sectors - (6 + 2 + 2 + 3 + 3));
  EXPECT_EQ(dirp.getByte(JOB_BYTE), 0);
  EXPECT_EQ(dirp.getByte(CHANNEL_BYTE), 0);

  ++dirp;
  EXPECT_EQ(dirp.getWord(STATUS_WORD), E_MPTY);
  EXPECT_EQ(dirp.getWord(TOTAL_LENGTH_WORD), 3);

  ++dirp;
  EXPECT_EQ(dirp.getWord(STATUS_WORD), E_EOS);
}

TEST_F(DirectoryTest, TruncateGrowIntoExactPrecedingSpace)
{
  auto segments = 1;
  auto swapFilename = Rad50Name { 075131, 062000, 075273 };   // SWAP.SYS

  using Ent = DirectoryBuilder::DirEntry;
  vector<vector<Ent>> dirdata = {
    {
      Ent {E_MPTY, 6 },
      Ent {E_PERM, 3, swapFilename },
      Ent {E_PERM, sectors - (6 + 2 + 6 + 3 + 3), Rad50Name {1, 2, 3}},      
      Ent {E_MPTY, 3 },
      Ent {E_EOS}
    },
  };

  // since the first free block is the largest, our file should end up there
  // since we're asking for exactly that amount of space, nothing else should
  // have to move around
  //
  builder.formatWithEntries(segments, dirdata);

  auto dir = Directory {blockCache.get()};

  auto dirp = dir.getDirPointer(swapFilename);
  EXPECT_EQ(dir.truncate(dirp, 6 * Block::SECTOR_SIZE), 0);

  EXPECT_EQ(dirp.getSegment(), 1);
  EXPECT_EQ(dirp.getIndex(), 0); 

  dirp = dir.startScan();

  ++dirp;
  EXPECT_EQ(dirp.getWord(STATUS_WORD), E_PERM);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 0), swapFilename[0]);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 2), swapFilename[1]);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 4), swapFilename[2]);
  EXPECT_EQ(dirp.getWord(TOTAL_LENGTH_WORD), 6);
  EXPECT_EQ(dirp.getByte(JOB_BYTE), 0);
  EXPECT_EQ(dirp.getByte(CHANNEL_BYTE), 0);

  ++dirp;
  EXPECT_EQ(dirp.getWord(STATUS_WORD), E_MPTY);
  EXPECT_EQ(dirp.getWord(TOTAL_LENGTH_WORD), 3);

  ++dirp;
  EXPECT_EQ(dirp.getWord(STATUS_WORD), E_PERM);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 0), 1);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 2), 2);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 4), 3);
  EXPECT_EQ(dirp.getWord(TOTAL_LENGTH_WORD), sectors - (6 + 2 + 6 + 3 + 3));
  EXPECT_EQ(dirp.getByte(JOB_BYTE), 0);
  EXPECT_EQ(dirp.getByte(CHANNEL_BYTE), 0);

  ++dirp;
  EXPECT_EQ(dirp.getWord(STATUS_WORD), E_MPTY);
  EXPECT_EQ(dirp.getWord(TOTAL_LENGTH_WORD), 3);

  ++dirp;
  EXPECT_EQ(dirp.getWord(STATUS_WORD), E_EOS);
}

TEST_F(DirectoryTest, TruncateGrowIntoLargerPrecedingSpace)
{
  auto segments = 1;
  auto swapFilename = Rad50Name { 075131, 062000, 075273 };   // SWAP.SYS

  using Ent = DirectoryBuilder::DirEntry;
  vector<vector<Ent>> dirdata = {
    {
      Ent {E_MPTY, 6 },
      Ent {E_PERM, 3, swapFilename },
      Ent {E_PERM, sectors - (6 + 2 + 6 + 3 + 3), Rad50Name {1, 2, 3}},      
      Ent {E_MPTY, 3 },
      Ent {E_EOS}
    },
  };

  // since the first free block is the largest, our file should end up there
  // since we're asking for exactly that amount of space, nothing else should
  // have to move around
  //
  builder.formatWithEntries(segments, dirdata);

  auto dir = Directory {blockCache.get()};

  auto dirp = dir.getDirPointer(swapFilename);
  EXPECT_EQ(dir.truncate(dirp, 5 * Block::SECTOR_SIZE), 0);

  EXPECT_EQ(dirp.getSegment(), 1);
  EXPECT_EQ(dirp.getIndex(), 0);

  dirp = dir.startScan();

  ++dirp;
  EXPECT_EQ(dirp.getWord(STATUS_WORD), E_PERM);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 0), swapFilename[0]);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 2), swapFilename[1]);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 4), swapFilename[2]);
  EXPECT_EQ(dirp.getWord(TOTAL_LENGTH_WORD), 5);
  EXPECT_EQ(dirp.getByte(JOB_BYTE), 0);
  EXPECT_EQ(dirp.getByte(CHANNEL_BYTE), 0);

  ++dirp;
  EXPECT_EQ(dirp.getWord(STATUS_WORD), E_MPTY);
  EXPECT_EQ(dirp.getWord(TOTAL_LENGTH_WORD), 4);

  ++dirp;
  EXPECT_EQ(dirp.getWord(STATUS_WORD), E_PERM);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 0), 1);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 2), 2);
  EXPECT_EQ(dirp.getWord(FILENAME_WORDS + 4), 3);
  EXPECT_EQ(dirp.getWord(TOTAL_LENGTH_WORD), sectors - (6 + 2 + 6 + 3 + 3));
  EXPECT_EQ(dirp.getByte(JOB_BYTE), 0);
  EXPECT_EQ(dirp.getByte(CHANNEL_BYTE), 0);

  ++dirp;
  EXPECT_EQ(dirp.getWord(STATUS_WORD), E_MPTY);
  EXPECT_EQ(dirp.getWord(TOTAL_LENGTH_WORD), 3);

  ++dirp;
  EXPECT_EQ(dirp.getWord(STATUS_WORD), E_EOS);
}

}
