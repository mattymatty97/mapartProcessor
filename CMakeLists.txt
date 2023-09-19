cmake_minimum_required(VERSION 3.26)
project(mapartProcessor C)

set(CMAKE_C_STANDARD 17)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build/${CMAKE_BUILD_TYPE}/")

##set(LIBMONGOC_PATH "D:/bin/MongoDb/c-driver")
set(OCL_PATH "D:/bin/NVIDIA/CUDA Toolkit/v12.2/")

##find_package(bson-1.0 REQUIRED PATHS ${LIBMONGOC_PATH} NO_DEFAULT_PATH)
##find_package(mongoc-1.0 REQUIRED PATHS ${LIBMONGOC_PATH} NO_DEFAULT_PATH)
find_package(OpenCL REQUIRED)

include_directories(src)
file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_SOURCE_DIR} "src/*.c")
## this line has to be before the target link
add_executable(mapartProcessor ${SOURCES})

##include_directories(mapartProcessor ${LIBMONGOC_INCLUDE_DIRS})
##target_link_libraries(mapartProcessor PUBLIC mongo::mongoc_static)
##include_directories(mapartProcessor ${LIBBSON_INCLUDE_DIRS})
##target_link_libraries(mapartProcessor PUBLIC mongo::bson_static)

include_directories(mapartProcessor ${OpenCL_INCLUDE_DIRS})
target_link_libraries(mapartProcessor PUBLIC ${OpenCL_LIBRARY})

target_link_libraries(mapartProcessor PRIVATE -static)

##define flag to only use first 3 multipliers
add_definitions(-DMAPART_SURVIVAL)