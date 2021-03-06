cmake_minimum_required(VERSION 2.8)

if (WIN32)
  cmake_policy (SET CMP0020 NEW)
endif (WIN32)

project(hifi)
add_definitions(-DGLM_FORCE_RADIANS)

if (WIN32)
  add_definitions(-DNOMINMAX -D_CRT_SECURE_NO_WARNINGS)
  set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "C:\\Program Files\\Microsoft SDKs\\Windows\\v7.1 ")
elseif (CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
  #SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-long-long -pedantic")
  #SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-unknown-pragmas")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -fno-strict-aliasing")
endif(WIN32)

if (NOT QT_CMAKE_PREFIX_PATH)
  set(QT_CMAKE_PREFIX_PATH $ENV{QT_CMAKE_PREFIX_PATH})
endif ()

set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${QT_CMAKE_PREFIX_PATH})

# set our Base SDK to 10.8
set(CMAKE_OSX_SYSROOT /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.8.sdk)
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.8)

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)
# Instruct CMake to run moc automatically when needed.
set(CMAKE_AUTOMOC ON)

if (APPLE)
   exec_program(uname ARGS -v  OUTPUT_VARIABLE DARWIN_VERSION)
   string(REGEX MATCH "[0-9]+" DARWIN_VERSION ${DARWIN_VERSION})
   if (DARWIN_VERSION GREATER 12)
     set(CMAKE_CXX_FLAGS "-stdlib=libstdc++")
   endif (DARWIN_VERSION GREATER 12)
endif (APPLE)

# targets not supported on windows
if (NOT WIN32)
  add_subdirectory(animation-server)
endif (NOT WIN32)

# targets on all platforms
add_subdirectory(assignment-client)
add_subdirectory(domain-server)
add_subdirectory(interface)
add_subdirectory(tests)
add_subdirectory(tools)
add_subdirectory(voxel-edit)
