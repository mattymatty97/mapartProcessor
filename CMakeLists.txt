cmake_minimum_required(VERSION 3.26)
project(mapartProcessor C)

set(CMAKE_C_STANDARD 17)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build/${CMAKE_BUILD_TYPE}/")

find_package(OpenCL REQUIRED)

include_directories(src)
file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_SOURCE_DIR} "src/*.c")
## this line has to be before the target link
add_executable(mapartProcessor ${SOURCES})

include_directories(mapartProcessor ${OpenCL_INCLUDE_DIRS})
target_link_libraries(mapartProcessor PUBLIC ${OpenCL_LIBRARY})

target_link_libraries(mapartProcessor PRIVATE -static)
