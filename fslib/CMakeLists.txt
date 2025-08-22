add_library (fslib
  Block.cpp
  BlockCache.cpp
  DirChangeTracker.cpp
  Directory.cpp
  DirPtr.cpp
  FileDataSource.cpp
  FileSystem.cpp
  LogUnimpl.cpp
  MemoryDataSource.cpp
  OpenFileTable.cpp
  Rad50.cpp
)

if (MACOS)
	add_definitions(-DFUSE_USE_VERSION=26)
	add_definitions(-D_FILE_OFFSET_BITS=64)
	add_definitions(-D_DARWIN_USE_64_BIT_INODE)

	include_directories(/usr/local/include)
	target_include_directories(fslib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
elseif(LINUX)
	add_definitions(-DFUSE_USE_VERSION=30)
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(FUSE REQUIRED fuse3)
	include_directories(${FUSE_INCLUDE_DIRS})
	target_include_directories(fslib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
	target_link_libraries(fslib ${FUSE3_LIBRARIES})
endif()

