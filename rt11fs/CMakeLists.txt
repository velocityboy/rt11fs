link_directories(/usr/local/lib)

add_executable (rt11fs
  rt11fs.cpp
)

add_definitions(-D_FILE_OFFSET_BITS=64)

if (MACOS)
	add_definitions(-DFUSE_USE_VERSION=26)
	include_directories(/usr/local/include)
	add_definitions(-D_DARWIN_USE_64_BIT_INODE)
	target_link_libraries(rt11fs LINK_PUBLIC osxfuse fslib)
elseif(LINUX)
	add_definitions(-DFUSE_USE_VERSION=30)
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(FUSE REQUIRED fuse3)
	include_directories(${FUSE_INCLUDE_DIRS})
	# target_link_libraries(rt11fs LINK_PUBLIC fslib)
	# Link with libfuse3
	target_link_libraries(rt11fs LINK_PUBLIC fslib ${FUSE_LIBRARIES})
endif()

