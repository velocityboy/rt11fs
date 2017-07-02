#include "DirectoryBuilder.h"

#include <cassert>

using namespace RT11FS;
using namespace RT11FS::Dir;

using std::copy;
using std::vector;

DirectoryBuilder::DirectoryBuilder(RT11FS::MemoryDataSource &dataSource)
  : dataSource(dataSource)
  , data(dataSource.getData())
{ 
}

auto DirectoryBuilder::putWord(int offset, uint16_t word) -> void
{
  data[offset] = word & 0xff;
  data[offset + 1] = (word >> 8) & 0xff;
}

auto DirectoryBuilder::putEntry(int segment, int index, const DirEntry &entry, int extraBytes) -> void
{
  auto offset = 
    (FIRST_SEGMENT_SECTOR + (segment - 1) * SECTORS_PER_SEGMENT) * Block::SECTOR_SIZE +
    FIRST_ENTRY_OFFSET + (ENTRY_LENGTH + extraBytes) * index;

  putWord(offset + STATUS_WORD, entry.status);

  for (auto i = 0; i < entry.name.size(); i++) {
    putWord(offset + FILENAME_WORDS + 2 * i, entry.name[i]);
  }

  putWord(offset + TOTAL_LENGTH_WORD, entry.length);
  data[offset + JOB_BYTE] = entry.job;
  data[offset + CHANNEL_BYTE] = entry.channel;
  putWord(offset + CREATION_DATE_WORD, entry.creation);
}

auto DirectoryBuilder::formatEmpty(int dirSegments, int extraBytes) -> void
{
  struct stat st;
  dataSource.stat(&st);

  auto sectors = st.st_size / Block::SECTOR_SIZE;
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
  putEntry(1, 0, DirEntry {E_EOS, static_cast<uint16_t>(sectors - firstDataSector)});
}

auto DirectoryBuilder::formatWithEntries(int dirSegments, const std::vector<std::vector<DirEntry>> &entries, int extraBytes) -> void
{
  assert(entries.size() <= dirSegments);

  if (entries.size() == 0) {
    formatEmpty(dirSegments, extraBytes);
    return;
  }

  auto entrySize = ENTRY_LENGTH + extraBytes;

  struct stat st;
  dataSource.stat(&st);

  auto sectors = st.st_size / Block::SECTOR_SIZE;
  auto dirSectors = dirSegments * SECTORS_PER_SEGMENT;
  auto nextSector = FIRST_SEGMENT_SECTOR + dirSectors;
  auto dataSectors = sectors - nextSector;

  for (auto i = 0; i < entries.size(); i++) {
    auto isFirst = (i == 0);
    auto isLast = (i == entries.size() - 1);
    auto offset = (FIRST_SEGMENT_SECTOR + i * SECTORS_PER_SEGMENT) * Block::SECTOR_SIZE;

    // header
    putWord(offset + TOTAL_SEGMENTS, dirSegments);
    putWord(offset + NEXT_SEGMENT, isLast ? 0 : i + 2);   // i is zero-based, on disk it's one-based
    putWord(offset + HIGHEST_SEGMENT, isFirst ? entries.size() : 0);
    putWord(offset + EXTRA_BYTES, extraBytes);
    putWord(offset + SEGMENT_DATA_BLOCK, nextSector);

    // entries
    // caller is responsible for include end of segment markers
    auto index = 0;
    for (const auto &entry : entries[i]) {
      auto writeEntry = entry;

      if (entry.length == REST_OF_DATA) {
        writeEntry.length = sectors - nextSector;
      }

      putEntry(i + 1, index++, writeEntry, extraBytes);
      nextSector += writeEntry.length;
    }
  }
}
