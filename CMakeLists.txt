cmake_minimum_required(VERSION 3.2)
project(DupTree)

set(CMAKE_CXX_STANDARD 17)
#set(OPENSSL_ROOT_DIR /opt/homebrew/opt/openssl@3)
#set(ENV{LIBRARY_PATH} "$ENV{LIBRARY_PATH}:$(brew --prefix)/lib")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

# file(GLOB SOURCES ./src/*.cpp)
# Add executable target
find_package(OpenSSL REQUIRED)

add_executable(duptree ./src/main.cpp src/tools.cpp src/tools.hpp src/duptree_child.hpp src/io.hpp src/mem_checker.hpp src/node.hpp src/merkle.hpp src/duptree.hpp src/duptree_plus.hpp
        src/sparse.hpp
        src/fattree.hpp
        src/rattree.hpp)
# add_executable(exp_eth src/exp_eth.cpp src/exp.hpp src/exp.cpp)
find_library(LEVELDB_LIB leveldb /usr/local/lib)
target_include_directories (duptree PUBLIC /usr/include)
#target_include_directories (duptree PUBLIC /opt/homebrew/include)
target_link_libraries(duptree LINK_PUBLIC OpenSSL::SSL ${LEVELDB_LIB})
#target_include_directories (exp_eth PUBLIC /usr/include)
#target_link_libraries(exp_eth LINK_PUBLIC ${LEVELDB_LIB})