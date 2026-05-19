cmake_minimum_required(VERSION 3.22)

project(discord-bot VERSION 1.0 DESCRIPTION "A discord bot")

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.h")

add_executable(${PROJECT_NAME} ${SOURCES})

find_package(DPP REQUIRED)
find_package(Threads REQUIRED)

target_link_libraries(${PROJECT_NAME}
        ${DPP_LIBRARIES}
        Threads::Threads
)

target_include_directories(${PROJECT_NAME} PRIVATE
        ${DPP_INCLUDE_DIR}
)

set_target_properties(${PROJECT_NAME} PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
)
