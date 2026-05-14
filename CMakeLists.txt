cmake_minimum_required(VERSION 3.10)
project(SimpleDB)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -g")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

find_package(Boost REQUIRED COMPONENTS system thread log program_options)

set(COMMON_SRC
    src/common/filehandle.cc
    src/common/hash_code.cc
    src/common/logging.cc
    src/common/options.cc
    src/common/message.cc
    src/common/cmdline.cc
    src/common/client.cc
    src/common/shell.cc
    src/common/simpledb.cc
)

set(STORAGE_SRC
    src/storage/mmapfile.cc
    src/storage/dms.cc
)

set(INDEX_SRC
    src/index/bucket.cc
)

set(SERVER_SRC
    src/server/threadpool.cc
    src/server/handle_message.cc
)

set(PMD_SRC
    src/pmd/pmd.cc
)

set(COMMAND_SRC
    src/command/icommand.cc
    src/command/command.cc
)

add_executable(simpledb
    src/server/main.cc
    ${COMMON_SRC}
    ${STORAGE_SRC}
    ${INDEX_SRC}
    ${SERVER_SRC}
    ${PMD_SRC}
    ${COMMAND_SRC}
)

target_link_libraries(simpledb
    Boost::system
    Boost::thread
    Boost::log
    Boost::program_options
    pthread
)

add_executable(simpledb_shell
    shell.cc
    src/common/shell.cc
    src/common/client.cc
    src/common/cmdline.cc
    src/common/options.cc
    src/common/hash_code.cc
    src/common/simpledb.cc
    src/common/filehandle.cc
    src/storage/mmapfile.cc
    src/storage/dms.cc
    src/index/bucket.cc
    src/pmd/pmd.cc
    src/command/icommand.cc
    src/command/command.cc
)

target_link_libraries(simpledb_shell
    Boost::system
    Boost::thread
    Boost::log
    Boost::program_options
    pthread
)