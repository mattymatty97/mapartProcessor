cmake_minimum_required(VERSION 3.26)
project(mapartProcessor C)

set(CMAKE_C_STANDARD 17)

IF (CYGWIN)
    SET (CMAKE_FIND_LIBRARY_PREFIXES "")
    SET (CMAKE_FIND_LIBRARY_SUFFIXES ".lib" ".LIB" ".dll" ".DLL")
ELSE()
    SET (CMAKE_FIND_LIBRARY_PREFIXES "lib")
    SET (CMAKE_FIND_LIBRARY_SUFFIXES ".so" ".a")
ENDIF (CYGWIN)

set(LIBMONGOC_PATH "D:/bin/MongoDb/c-driver")

find_package(bson-1.0 REQUIRED PATHS ${LIBMONGOC_PATH} NO_DEFAULT_PATH)
find_package(mongoc-1.0 REQUIRED PATHS ${LIBMONGOC_PATH} NO_DEFAULT_PATH)

include_directories(src)
file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_SOURCE_DIR} "src/*.c")
## this line has to be before the target link
add_executable(mapartProcessor ${SOURCES})

target_link_libraries(mapartProcessor PUBLIC mongo::mongoc_static)
include_directories(mapartProcessor ${LIBMONGOC_INCLUDE_DIRS})
