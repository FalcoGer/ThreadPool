cmake_minimum_required(VERSION 3.10.0)
project(ThreadPool VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 26)

add_executable(ThreadPool main.cpp ThreadPool.cpp)

