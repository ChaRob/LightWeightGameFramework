cmake_minimum_required(VERSION 3.20)

project(LightWeightGameFramework LANGUAGES CXX)

add_executable(LightWeightGameFramework
    src/main.cpp
)

target_compile_features(LightWeightGameFramework PRIVATE cxx_std_20)
