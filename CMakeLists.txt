cmake_minimum_required(VERSION 3.22)

project(discord-bot VERSION 1.0 DESCRIPTION "A discord bot")

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.h")

add_executable(${PROJECT_NAME} ${SOURCES})
add_compile_definitions(DPP_CORO)

find_package(DPP REQUIRED)
find_package(Threads REQUIRED)
find_package(libpqxx REQUIRED)

target_link_libraries(${PROJECT_NAME}
        ${DPP_LIBRARIES}
        Threads::Threads
        libpqxx::pqxx
)

target_include_directories(${PROJECT_NAME} PRIVATE
        ${DPP_INCLUDE_DIR}
)


set_target_properties(${PROJECT_NAME} PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
)
