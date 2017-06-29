link_directories(/usr/local/lib)

add_executable (rt11fs
  Block.cpp
  BlockCache.cpp
  Directory.cpp
  DirPtr.cpp
  File.cpp
  FileSystem.cpp
  LogUnimpl.cpp
  Rad50.cpp
  rt11fs.cpp
)

add_definitions(-DFUSE_USE_VERSION=26)
add_definitions(-D_FILE_OFFSET_BITS=64)
add_definitions(-D_DARWIN_USE_64_BIT_INODE)

include_directories(/usr/local/include)

target_link_libraries(rt11fs LINK_PUBLIC osxfuse)

