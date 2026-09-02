# CTest reads this from the *build* tree, not here -- CMakeLists.txt
# (configure_file, next to include(CTest)) copies it over at configure time.
#
# Tests listed here still run as part of a normal `ctest`/`Run Tests`
# invocation -- this only skips them during the memcheck (Valgrind) pass.
# See CMakeLists.txt, next to where UnitTestsHighFrequencyAsyncDispatch is
# registered, for why.
set(CTEST_CUSTOM_MEMCHECK_IGNORE
    UnitTestsHighFrequencyAsyncDispatch
)
