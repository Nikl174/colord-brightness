cmake_minimum_required(VERSION 3.5)
project(
  colord-brightness
  VERSION 0.1.0
  LANGUAGES CXX)
include(CTest)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

find_package(PkgConfig REQUIRED)

pkg_check_modules(COLORD REQUIRED colord)
pkg_check_modules(LCMS2 REQUIRED lcms2)

set(FILE_WATCHER_SRC ${SRC_DIR}/FileWatcher.cpp)

add_library(file_watcher)
target_sources(file_watcher PRIVATE ${FILE_WATCHER_SRC})
target_include_directories(file_watcher PUBLIC ${INCLUDE_DIR})

add_executable(test_file_watcher)
target_sources(test_file_watcher PRIVATE tests/test_file_watcher.cpp)
target_link_libraries(test_file_watcher file_watcher)
add_test(NAME test_file_watcher COMMAND test_file_watcher)

# add_executable(test tests/test.cpp)
# target_link_libraries(test ${COLORD_LIBRARIES} ${LCMS2_LIBRARIES})
# target_include_directories(test PUBLIC ${COLORD_INCLUDE_DIRS}
#                                        ${LCMS2_INCLUDE_DIRS})
