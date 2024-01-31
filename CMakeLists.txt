cmake_minimum_required(VERSION 3.0.0)
project(your_project_name VERSION 0.1.0)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -D_POSIX_C_SOURCE=200809 -Wall")

add_executable(your_executable_name shell.c myshell.c)
