cmake_minimum_required(VERSION 3.0)

set(PROJECT lesson2)

project(${PROJECT})

set(HEADERS)

set(SOURCES
        lesson2.c
        )

set(GXX_FLAGS "-g")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${GXX_FLAGS}")
find_package(PkgConfig REQUIRED)
pkg_check_modules(PIPEWIRE REQUIRED libpipewire-0.3)

include_directories(${PTHREAD_INCLUDE_DIR})

add_executable(${PROJECT} ${HEADERS} ${SOURCES})
target_link_libraries(${PROJECT} ${PIPEWIRE_LIBRARIES})
target_include_directories(${PROJECT} PUBLIC ${PIPEWIRE_INCLUDE_DIRS})
target_compile_options(${PROJECT} PUBLIC ${PIPEWIRE_CFLAGS_OTHER})
