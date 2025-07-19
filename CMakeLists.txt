cmake_minimum_required(VERSION 3.17)
project(Raytracing LANGUAGES CXX OBJCXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories("/opt/homebrew/include")

add_executable(Raytracing
        main.cpp
        stb_image_write.h
        lodepng.cpp
)