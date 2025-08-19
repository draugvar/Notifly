# Notifly - C++ Notification Center Library

Notifly is a header-only C++ notification center library inspired by Cocoa's NSNotificationCenter API. It provides synchronous and asynchronous notification posting with type-safe observer pattern implementation.

Always reference these instructions first and fallback to search or bash commands only when you encounter unexpected information that does not match the info here.

## Working Effectively

### Bootstrap and Build
- Clean clone setup and build:
  - `cd /path/to/Notifly`
  - `cmake -B build -DCMAKE_BUILD_TYPE=Release`
  - `cmake --build build --config Release` -- takes 16-25 seconds. NEVER CANCEL. Set timeout to 60+ seconds.
  - First-time build with GoogleTest download may take longer due to network.

### Alternative Build Configurations
- Debug build (slower compilation, more debugging info):
  - `cmake -B build-debug -DCMAKE_BUILD_TYPE=Debug`
  - `cmake --build build-debug --config Debug` -- takes 10-15 seconds. NEVER CANCEL. Set timeout to 60+ seconds.

### Clean Build (complete rebuild)
- Clean build from scratch:
  - `rm -rf build build-debug`
  - `cmake -B build`
  - `cmake --build build` -- takes 16-25 seconds. NEVER CANCEL. Set timeout to 60+ seconds.

### Running Tests
- Unit tests (comprehensive test suite):
  - `./build/notifly_unit_test` -- takes less than 1 second. NEVER CANCEL. Set timeout to 30+ seconds.
  - All 26 tests should pass, testing core notification functionality, threading, and edge cases
  - Example final output: `[==========] 26 tests from 1 test suite ran. (XXXms total)` and `[  PASSED  ] 26 tests.`
  - To count individual passed tests: `./build/notifly_unit_test | grep "\[       OK \]" | wc -l` should return 26

### Running Example Application
- Example demonstrating library usage:
  - `./build/notifly` -- runs immediately. Set timeout to 30+ seconds.
  - Shows various notification patterns: lambdas, function pointers, async notifications, observer management

## Validation

### Manual Testing Requirements
- ALWAYS run the complete unit test suite after making changes: `./build/notifly_unit_test`
- ALWAYS run the example application to verify basic functionality: `./build/notifly`  
- ALWAYS test both Release and Debug builds when making significant changes
- Verify thread safety by running multi-threaded tests (included in unit test suite)

### Critical Validation Scenarios
After any changes to the core library (`include/notifly.h`):
1. **Basic Notification Test**: Verify simple observer registration and notification posting works
2. **Type Safety Test**: Verify type mismatches are properly caught and handled
3. **Threading Test**: Verify async notifications work correctly without race conditions
4. **Memory Management Test**: Verify observer cleanup works without leaks
5. **Multiple Observer Test**: Verify multiple observers for same notification work correctly

### Code Quality
- Format code using clang-format (available at `/usr/bin/clang-format`)
- No specific linting rules configured in repository - follow existing code style
- The CI pipeline (`.github/workflows/build_and_test.yml`) will build and test on both Ubuntu and Windows

## Common Tasks

### Repository Structure
```
/home/runner/work/Notifly/Notifly/
├── include/
│   └── notifly.h              # Main header-only library  
├── example/
│   └── main.cpp               # Example application
├── test/
│   ├── unit_test.cpp          # Unit test implementation
│   └── unit_test.h            # Unit test helper functions
├── CMakeLists.txt             # CMake build configuration
├── .github/workflows/         # CI/CD pipeline
└── README.md                  # Library documentation
```

### Key Files to Know
- **`include/notifly.h`**: The entire library implementation (header-only)
- **`test/unit_test.cpp`**: Comprehensive test suite with 26 test cases
- **`example/main.cpp`**: Example showing library usage patterns
- **`CMakeLists.txt`**: Build configuration, GoogleTest integration, C++20 requirement

### Dependencies and Requirements
- **C++ Compiler**: GCC 13.3.0+ or equivalent with C++20 support
- **CMake**: 3.16+ (tested with 3.31.6)
- **Required C++20 Features**: std::mutex, std::function, std::shared_ptr, std::any, std::unordered_map, std::jthread
- **GoogleTest**: Automatically downloaded by CMake FetchContent during build
- **Threading**: pthread (automatically linked on Linux)

### Expected Timing (Linux environment, validated)
- **CMake Configure**: 2-5 seconds
- **Release Build**: 16-25 seconds (first time may take longer due to GoogleTest download)
- **Debug Build**: 10-15 seconds
- **Unit Tests**: 0.26 seconds (26 tests, all should pass)
- **Example Run**: <0.01 seconds

### Library Usage Patterns
The library supports these notification patterns:
```cpp
// Basic usage
notifly::default_notifly().add_observer(1, [](){ printf("Hello!"); });
notifly::default_notifly().post_notification(1);

// With parameters (must match types exactly)
auto id = notifly::default_notifly().add_observer(2, [](int a, std::string b){ /* ... */ });
notifly::default_notifly().post_notification(2, 42, std::string("test"));

// Async notifications
notifly::default_notifly().post_notification_async(1);

// Observer cleanup
notifly::default_notifly().remove_observer(id);
```

### Error Codes
- `0`: Success
- `-1`: Observer not found
- `-2`: Notification not found  
- `-3`: Payload type mismatch (most common error)
- `-4`: No more observer IDs available

### Troubleshooting
- **Build failures**: Ensure C++20 compiler is available and CMake >= 3.16
- **Type mismatch errors**: Notification payloads must exactly match observer function signatures
- **Async test failures**: May indicate threading issues - run tests multiple times to verify consistency
- **GoogleTest download issues**: Network connectivity required for first build

### Cross-Platform Notes
- **Linux**: Primary development platform, fully tested
- **Windows**: Supported via CI, uses Visual Studio compiler (`cl`)
- **macOS**: Uses different thread implementation (`std::thread` vs `std::jthread`)