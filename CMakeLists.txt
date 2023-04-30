cmake_minimum_required(VERSION 3.10.2)
project(PrakBS23 C)

set(CMAKE_C_STANDARD 99)

add_executable(PrakBS23 main.c keyValStore.c keyValStore.h sub.c sub.h process_list.c process_list.h RestApi.c RestApi.h)
