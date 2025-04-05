# ------------------------------------------------------------------------------
# Google Test Configuration for Notifly Project
# 
# This file sets up Google Test framework for unit testing, fetches the 
# required dependencies and configures the test executable.
# ------------------------------------------------------------------------------

include(FetchContent)

# ------------------------------------------------------------------------------
# Google Test Options
# ------------------------------------------------------------------------------
option(BUILD_GMOCK "Build Google test's GMock?" OFF)
option(INSTALL_GMOCK "Install Google test's GMock?" OFF)
option(INSTALL_GTEST "Install Google test's GTest?" OFF)

# ------------------------------------------------------------------------------
# Fetch Google Test from GitHub
# ------------------------------------------------------------------------------
FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG main
)

FetchContent_MakeAvailable(googletest)

# ------------------------------------------------------------------------------
# Configure Google Test Properties
# ------------------------------------------------------------------------------
set_target_properties(gtest PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreaded$<$<CONFIG:Debug>:Debug>)
set_target_properties(gtest_main PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreaded$<$<CONFIG:Debug>:Debug>)

# Commented include directories - uncomment if needed
# include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})
# include_directories(test/google_test/include)

# ------------------------------------------------------------------------------
# Test Sources Configuration
# ------------------------------------------------------------------------------
# Find all test source files in test/ directory and implementation files
file(GLOB UNIT_SOURCE test/*.cpp test/*.h)

# ------------------------------------------------------------------------------
# Define and Configure Test Executable
# ------------------------------------------------------------------------------
set(UNIT_TEST notifly_unit_test)
add_executable(${UNIT_TEST} ${UNIT_SOURCE})

# Set include directories for test target
target_include_directories(${UNIT_TEST} PRIVATE test/google_test/include include)

# Link required libraries
target_link_libraries(${UNIT_TEST} PRIVATE
        ${CMAKE_THREAD_LIBS_INIT}
        $<$<PLATFORM_ID:Linux>:dl>
        gtest
        gtest_main)

# Set output properties for the test executable
set_target_properties(${UNIT_TEST}
        PROPERTIES
        OUTPUT_NAME ${UNIT_TEST}
        MSVC_RUNTIME_LIBRARY MultiThreaded$<$<CONFIG:Debug>:Debug>
)
