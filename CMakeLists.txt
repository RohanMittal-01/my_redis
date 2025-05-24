cmake_minimum_required(VERSION 3.10)
project(my_redis)
set(CMAKE_CXX_STANDARD 17)

add_subdirectory(Crow)
add_executable(my_redis memory.cpp)
target_link_libraries(my_redis PRIVATE Crow)
