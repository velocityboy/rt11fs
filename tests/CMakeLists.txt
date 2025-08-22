include(ExternalProject)

set (GTEST_ROOT "${CMAKE_CURRENT_BINARY_DIR}/install")

ExternalProject_Add(
  GTestBuild
  GIT_REPOSITORY "https://github.com/google/googletest.git"
  GIT_TAG "main"
  PREFIX "${CMAKE_CURRENT_BINARY_DIR}/build"
  CMAKE_ARGS
    -DBUILD_SHARED_LIBS:BOOL=ON
    -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
    -DCMAKE_INSTALL_PREFIX:PATH=${GTEST_ROOT}
    -DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS} 
)
find_package(GTest REQUIRED)

if(MACOS)
	add_definitions(-DFUSE_USE_VERSION=26)
	add_definitions(-D_FILE_OFFSET_BITS=64)
	add_definitions(-D_DARWIN_USE_64_BIT_INODE)
endif()
if(LINUX)
	add_definitions(-DFUSE_USE_VERSION=30)
	add_definitions(-D_FILE_OFFSET_BITS=64)
endif()

add_executable (tests
  DirectoryBuilder.cpp
  TestBlock.cpp
  TestBlockCache.cpp
  TestDirectory.cpp
)
include_directories(/usr/local/include ${GTEST_INCLUDE_DIRS})
target_link_libraries(tests LINK_PUBLIC ${GTEST_BOTH_LIBRARIES} fslib)

