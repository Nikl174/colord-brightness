cmake_minimum_required(VERSION 3.5)
project(
  colord-brightness
  VERSION 0.1.0
  LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)

pkg_check_modules(COLORD REQUIRED colord)
pkg_check_modules(LCMS2 REQUIRED lcms2)

add_executable(test src/test.cpp)
target_link_libraries(test ${COLORD_LIBRARIES} ${LCMS2_LIBRARIES})
target_include_directories(test PUBLIC ${COLORD_INCLUDE_DIRS}
                                       ${LCMS2_INCLUDE_DIRS})
