cmake_minimum_required(VERSION 3.16)
project(notifly)

set(CMAKE_CXX_STANDARD 20)

include_directories(include)

add_executable(${PROJECT_NAME}
        example/main.cpp
)

set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

include(FetchContent)

#
# Google Test
#
option(BUILD_GMOCK "Build Google test's GMock?" OFF)
option(INSTALL_GMOCK "Install Google test's GMock?" OFF)
option(INSTALL_GTEST "Install Google test's GTest?" OFF)

FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG main
)

FetchContent_MakeAvailable(googletest)

set_target_properties(gtest PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreaded$<$<CONFIG:Debug>:Debug>)
set_target_properties(gtest_main PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreaded$<$<CONFIG:Debug>:Debug>)

# include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})
# include_directories(test/google_test/include)

# 'Unit_Tests_run' is the target name
# 'UNIT_SOURCE' are source files with tests
file(GLOB UNIT_SOURCE test/*.cpp test/*.h src/*.cpp include/*.h)

set(UNIT_TEST unit_test)
add_executable(${UNIT_TEST} ${UNIT_SOURCE})
target_include_directories(${UNIT_TEST} PRIVATE test/google_test/include)
target_link_libraries(${UNIT_TEST} PRIVATE
        ${CMAKE_THREAD_LIBS_INIT}
        $<$<PLATFORM_ID:Linux>:dl>
        gtest
        gtest_main)
set_target_properties(${UNIT_TEST}
        PROPERTIES
        OUTPUT_NAME ${UNIT_TEST}
        MSVC_RUNTIME_LIBRARY MultiThreaded$<$<CONFIG:Debug>:Debug>
        )

# PartyThreads
FetchContent_Declare(
        partythreads
        GIT_REPOSITORY https://github.com/draugvar/PartyThreads.git
        GIT_TAG main
)
FetchContent_MakeAvailable(partythreads)
include_directories(${partythreads_SOURCE_DIR}/include)

target_include_directories(${PROJECT_NAME} PRIVATE ${partythreads_SOURCE_DIR}/include)