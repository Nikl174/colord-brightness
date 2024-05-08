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
pkg_check_modules(EASYLOGGINGPP REQUIRED easyloggingpp)

set(FILE_WATCHER_SRC ${SRC_DIR}/FileWatcher.cpp)
set(COLORD_HANDLER_SRC ${SRC_DIR}/ColordHandler.cpp)

add_library(Easyloggigpp)
target_sources(Easyloggigpp
               PUBLIC ${EASYLOGGINGPP_INCLUDE_DIRS}/easylogging++.cc)
target_include_directories(Easyloggigpp PUBLIC ${EASYLOGGINGPP_INCLUDE_DIRS})

add_library(file_watcher)
target_sources(file_watcher PRIVATE ${FILE_WATCHER_SRC})
target_include_directories(file_watcher PUBLIC ${INCLUDE_DIR})
target_link_libraries(file_watcher Easyloggigpp)

add_library(colord_handler)
target_sources(colord_handler PRIVATE ${COLORD_HANDLER_SRC})
target_include_directories(
  colord_handler PUBLIC ${INCLUDE_DIR} ${COLORD_INCLUDE_DIRS}
                        ${LCMS2_INCLUDE_DIRS})
target_link_libraries(colord_handler ${COLORD_LIBRARIES} ${LCMS2_LIBRARIES}
                      Easyloggigpp)

add_executable(test_file_watcher)
target_sources(test_file_watcher PRIVATE tests/test_file_watcher.cpp)
target_link_libraries(test_file_watcher file_watcher Easyloggigpp)
target_include_directories(test_file_watcher PUBLIC ${INCLUDE_DIR})
add_test(NAME test_file_watcher COMMAND test_file_watcher)

add_executable(colord-brightness)
target_sources(colord-brightness PRIVATE ${SRC_DIR}/colord_brightness.cpp)
target_include_directories(
  colord-brightness PUBLIC ${INCLUDE_DIR} ${COLORD_INCLUDE_DIRS}
                           ${LCMS2_INCLUDE_DIRS})
target_link_libraries(colord-brightness file_watcher colord_handler
                      ${COLORD_LIBRARIES} ${LCMS2_LIBRARIES} Easyloggigpp)


