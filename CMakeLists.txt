cmake_minimum_required(VERSION 3.15)
project(GoodbyeOS)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -lpthread")

add_executable(GoodbyeOS 4/main.cpp)