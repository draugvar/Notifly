cmake_minimum_required(VERSION 3.16)
project(notifly)

set(CMAKE_CXX_STANDARD 20)

# ── Backend selection ─────────────────────────────────────────────────────────
set(NOTIFLY_BACKEND "cpp" CACHE STRING "Backend implementation: cpp or rust")
set_property(CACHE NOTIFLY_BACKEND PROPERTY STRINGS "cpp" "rust")
message(STATUS "NOTIFLY_BACKEND = ${NOTIFLY_BACKEND}")

include_directories(include)

include(FetchContent)

# ── C++ backend ───────────────────────────────────────────────────────────────
if(NOTIFLY_BACKEND STREQUAL "cpp")

    # C++ example executable
    add_executable(${PROJECT_NAME} example/main.cpp)
    set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX)
    target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

    # C interface shared library (C++ implementation)
    add_library(${PROJECT_NAME}_c SHARED src/notifly_c.cpp)
    set_target_properties(${PROJECT_NAME}_c PROPERTIES
        LINKER_LANGUAGE CXX
        OUTPUT_NAME "notifly_c"
        VERSION 1.0.0
        SOVERSION 1
        DEFINE_SYMBOL "NOTIFLY_C_EXPORTS"
    )
    target_compile_features(${PROJECT_NAME}_c PUBLIC cxx_std_20)
    target_include_directories(${PROJECT_NAME}_c PUBLIC include)

    # Google Test
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

    # C++ unit tests (use notifly.h directly, no shared lib dependency)
    file(GLOB UNIT_SOURCE test/unit_test.cpp test/unit_test.h test/exchange_test.cpp src/*.cpp include/*.h)
    set(UNIT_TEST notifly_unit_test)
    add_executable(${UNIT_TEST} ${UNIT_SOURCE})
    target_include_directories(${UNIT_TEST} PRIVATE test/google_test/include)
    target_link_libraries(${UNIT_TEST} PRIVATE
        ${CMAKE_THREAD_LIBS_INIT}
        $<$<PLATFORM_ID:Linux>:dl>
        gtest
        gtest_main
    )
    set_target_properties(${UNIT_TEST} PROPERTIES
        OUTPUT_NAME ${UNIT_TEST}
        MSVC_RUNTIME_LIBRARY MultiThreaded$<$<CONFIG:Debug>:Debug>
    )

    add_test(NAME UnitTests COMMAND ${UNIT_TEST})

    # Exchange example
    set(EXCHANGE_EXAMPLE notifly_exchange_example)
    add_executable(${EXCHANGE_EXAMPLE} example/exchange_example.cpp)
    target_include_directories(${EXCHANGE_EXAMPLE} PRIVATE include)
    target_compile_features(${EXCHANGE_EXAMPLE} PUBLIC cxx_std_20)
    target_link_libraries(${EXCHANGE_EXAMPLE} PRIVATE ${CMAKE_THREAD_LIBS_INIT})
    set_target_properties(${EXCHANGE_EXAMPLE} PROPERTIES
        OUTPUT_NAME ${EXCHANGE_EXAMPLE}
        LINKER_LANGUAGE CXX
        MSVC_RUNTIME_LIBRARY MultiThreaded$<$<CONFIG:Debug>:Debug>
    )

# ── Rust backend ──────────────────────────────────────────────────────────────
elseif(NOTIFLY_BACKEND STREQUAL "rust")

    # Corrosion: CMake ↔ Cargo bridge
    FetchContent_Declare(
        Corrosion
        GIT_REPOSITORY https://github.com/corrosion-rs/corrosion.git
        GIT_TAG v0.5
    )
    FetchContent_MakeAvailable(Corrosion)

    # Import the Rust crate. Produces a CMake target named notifly_c
    # (matches [package] name in rust/Cargo.toml).
    corrosion_import_crate(
        MANIFEST_PATH ${CMAKE_CURRENT_SOURCE_DIR}/rust/Cargo.toml
    )

    # Expose the include directory so C consumers find notifly_c.h
    target_include_directories(notifly_c INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

else()
    message(FATAL_ERROR "Unknown NOTIFLY_BACKEND '${NOTIFLY_BACKEND}'. Use 'cpp' or 'rust'.")
endif()

# ── Common C targets (both backends) ─────────────────────────────────────────

# C interface example
set(C_EXAMPLE notifly_c_example)
add_executable(${C_EXAMPLE} example/c_example.c)
target_link_libraries(${C_EXAMPLE} PRIVATE ${PROJECT_NAME}_c)
target_include_directories(${C_EXAMPLE} PRIVATE include)
set_target_properties(${C_EXAMPLE} PROPERTIES
    OUTPUT_NAME ${C_EXAMPLE}
    LINKER_LANGUAGE C
)

# C interface test (pure C, no GoogleTest)
set(C_API_TEST notifly_c_api_test)
add_executable(${C_API_TEST} test/c_api_test.c)
target_link_libraries(${C_API_TEST} PRIVATE
    ${PROJECT_NAME}_c
    ${CMAKE_THREAD_LIBS_INIT}
    $<$<PLATFORM_ID:Linux>:dl>
)
target_include_directories(${C_API_TEST} PRIVATE include)
set_target_properties(${C_API_TEST} PROPERTIES
    OUTPUT_NAME ${C_API_TEST}
    LINKER_LANGUAGE C
)

# ── Valgrind / CTest ──────────────────────────────────────────────────────────
find_program(VALGRIND_EXECUTABLE valgrind)
if(VALGRIND_EXECUTABLE)
    set(MEMORYCHECK_COMMAND ${VALGRIND_EXECUTABLE})
    set(CTEST_MEMORYCHECK_COMMAND ${VALGRIND_EXECUTABLE})
    set(MEMORYCHECK_COMMAND_OPTIONS "--leak-check=full --show-leak-kinds=definite,possible --track-origins=yes --error-exitcode=1")
    set(CTEST_MEMORYCHECK_COMMAND_OPTIONS "--leak-check=full --show-leak-kinds=definite,possible --track-origins=yes --error-exitcode=1")
    if(EXISTS "${CMAKE_SOURCE_DIR}/valgrind.supp")
        set(MEMORYCHECK_SUPPRESSIONS_FILE "${CMAKE_SOURCE_DIR}/valgrind.supp")
        set(CTEST_MEMORYCHECK_SUPPRESSIONS_FILE "${CMAKE_SOURCE_DIR}/valgrind.supp")
    endif()
endif()

include(CTest)
add_test(NAME CApiTests COMMAND ${C_API_TEST})

# ── Utility function ──────────────────────────────────────────────────────────
function(move_file source_file destination_dir)
    if(EXISTS ${source_file})
        get_filename_component(file_name ${source_file} NAME)
        set(destination_file ${destination_dir}/${file_name})
        if(NOT EXISTS ${destination_file})
            file(COPY ${source_file} DESTINATION ${destination_dir})
            message(STATUS "File ${source_file} copied to ${destination_dir}.")
        else()
            file(SHA256 ${source_file} SHA256_SOURCE)
            file(SHA256 ${destination_file} SHA256_DESTINATION)
            if(NOT SHA256_SOURCE STREQUAL SHA256_DESTINATION)
                file(COPY ${source_file} DESTINATION ${destination_dir})
                message(STATUS "File ${source_file} copied to ${destination_dir}.")
            else()
                message(STATUS "File ${source_file} is equal to ${destination_file}. No copy needed.")
            endif()
        endif()
    else()
        message(FATAL_ERROR "The file ${source_file} does not exist.")
    endif()
endfunction()
