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

set(UNIT_TEST notifly_unit_test)
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

# Function to copy a file only if it does not exist or is different
function(move_file source_file destination_dir)
        # Check if the source file exists
        if(EXISTS ${source_file})
                # Get the file name from the source file path
                get_filename_component(file_name ${source_file} NAME)
                # Set the destination file path
                set(destination_file ${destination_dir}/${file_name})

                # Check if the destination file does not exist or if the files are different
                if(NOT EXISTS ${destination_file})
                        # Copy the source file to the destination
                        file(COPY ${source_file} DESTINATION ${destination_dir})
                        message(STATUS "File ${source_file} copied to ${destination_dir}.")
                else()
                        # Compute the SHA256 hash of the source and destination files
                        file(SHA256 ${source_file} SHA256_SOURCE)
                        file(SHA256 ${destination_file} SHA256_DESTINATION)

                        # Compare the hashes
                        if(NOT SHA256_SOURCE STREQUAL SHA256_DESTINATION)
                                # Copy the source file to the destination
                                file(COPY ${source_file} DESTINATION ${destination_dir})
                                message(STATUS "File ${source_file} copied to ${destination_dir}.")
                        else()
                                # Print a message if the files are the same
                                message(STATUS "File ${source_file} is equal to ${destination_file}. No copy needed.")
                        endif()
                endif()
        else()
                # Print an error message if the source file does not exist
                message(FATAL_ERROR "The file ${source_file} does not exist.")
        endif()
endfunction()
