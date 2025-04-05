cmake_minimum_required(VERSION 3.16)
project(notifly)

set(CMAKE_CXX_STANDARD 20)

add_executable(${PROJECT_NAME}_cpp_example
        example/cpp_example.cpp
)

target_include_directories(${PROJECT_NAME}_cpp_example PRIVATE include)

set_target_properties(${PROJECT_NAME}_cpp_example PROPERTIES LINKER_LANGUAGE CXX)
target_compile_features(${PROJECT_NAME}_cpp_example PUBLIC cxx_std_20)

add_library(${PROJECT_NAME}_c STATIC src/c_notifly.cpp)
target_include_directories(${PROJECT_NAME}_c PRIVATE include include/c_interface)
set_target_properties(${PROJECT_NAME}_c PROPERTIES LINKER_LANGUAGE CXX)
target_compile_features(${PROJECT_NAME}_c PUBLIC cxx_std_20)
target_link_libraries(${PROJECT_NAME}_c PRIVATE
        ${CMAKE_THREAD_LIBS_INIT}
        $<$<PLATFORM_ID:Linux>:dl>
)
set_target_properties(${PROJECT_NAME}_c PROPERTIES
        OUTPUT_NAME ${PROJECT_NAME}
        MSVC_RUNTIME_LIBRARY MultiThreaded$<$<CONFIG:Debug>:Debug>
)

add_executable(${PROJECT_NAME}_c_example
        example/c_example.c
)
target_include_directories(${PROJECT_NAME}_c_example PRIVATE include/c_interface)
target_link_libraries(${PROJECT_NAME}_c_example PRIVATE
        ${CMAKE_THREAD_LIBS_INIT}
        $<$<PLATFORM_ID:Linux>:dl>
        ${PROJECT_NAME}_c
)

include(test/gtest.cmake)