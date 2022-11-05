cmake_minimum_required(VERSION 3.16)
project(notifly)

set(CMAKE_CXX_STANDARD 20)

include_directories(include)

add_executable(${PROJECT_NAME}
        src/main.cpp
        src/notifly.cpp)

set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)