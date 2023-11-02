cmake_minimum_required(VERSION 3.26)
project(mapartProcessor C)

set( RC_DEPENDS "" )

function( add_resource input )
    string( MAKE_C_IDENTIFIER ${input} input_identifier )
    get_filename_component(input_dir ${input} DIRECTORY)
    file( MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${input_dir}")
    file( COPY_FILE "${CMAKE_SOURCE_DIR}/${input}" "${CMAKE_CURRENT_BINARY_DIR}/${input}")
    set( output "${CMAKE_CURRENT_BINARY_DIR}/tmp/${input_identifier}.o" )
    target_link_libraries(${PROJECT_NAME} PUBLIC ${output} )

    add_custom_command(
            OUTPUT ${output}
            COMMAND ${CMAKE_LINKER} --relocatable --format binary --output ${output} ${input}
            DEPENDS ${input}
    )

    set( RC_DEPENDS ${RC_DEPENDS} ${output} PARENT_SCOPE )
endfunction()

set(CMAKE_C_STANDARD 17)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build/${CMAKE_BUILD_TYPE}/")

find_package(OpenCL REQUIRED)
find_package(ZLIB REQUIRED)

include_directories(src)
file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_SOURCE_DIR} "src/*.c")
## this line has to be before the target link
add_executable(${PROJECT_NAME} ${SOURCES})

# Resource file list
add_resource("resources/opencl/progress.cl")
add_resource("resources/opencl/mapart.cl")
add_resource("resources/opencl/color_conversions.cl")
add_resource("resources/opencl/dither.cl")

include_directories(${PROJECT_NAME} ${OpenCL_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PUBLIC ${OpenCL_LIBRARY})
target_link_libraries(${PROJECT_NAME} PUBLIC ZLIB::ZLIB)

target_link_libraries(${PROJECT_NAME} PRIVATE -static)

add_definitions(-DNBT_OWN_ZLIB="zlib.h")

add_custom_target( rc ALL DEPENDS ${RC_DEPENDS} )