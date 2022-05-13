cmake_minimum_required(VERSION 3.7.2)
project(TaskMonitorReader)

set(CMAKE_PROJECT_HOMEPAGE_URL "https://gitlab.com/taskmonitor/tkm-reader")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# BaseSoftwareInfrastructure
set(BSWINFRA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/bswinfra)
set(INTERFACES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/interfaces)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" "${BSWINFRA_DIR}/cmake" "${INTERFACES_DIR}/cmake")

message (STATUS "Module paths: ${CMAKE_MODULE_PATH}")

option(WITH_SYSLOG "Build with syslog logger backend" Y)
option(WITH_TESTS "Build test suite" N)
option(WITH_TIDY "Build with clang-tidy" N)

# Build time configuration setup
if(EXISTS "${CMAKE_SOURCE_DIR}/.git")
    execute_process(
        COMMAND git --git-dir "${CMAKE_CURRENT_SOURCE_DIR}/.git" rev-parse --short HEAD
        OUTPUT_VARIABLE GIT_SHA1
        OUTPUT_STRIP_TRAILING_WHITESPACE
        )
else(EXISTS "${CMAKE_SOURCE_DIR}/.git")
    set(GIT_SHA1 "")
endif(EXISTS "${CMAKE_SOURCE_DIR}/.git")

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING
        "Choose the type of build: Debug, Release, RelWithDebInfo, MinSizeRel." FORCE)
endif()

# Enable collector interfaces
set(WITH_COLLECTOR_CONTROL ON)

set(PROJECT_VERSION_MAJOR "1")
set(PROJECT_VERSION_MINOR "0")
set(PROJECT_VERSION_PATCH ${GIT_SHA1})

include(GNUInstallDirs)
include(Packing)
include(BSWInfra)
include(TaskMonitorInterfaces)

# Add support for coverage analysis
if(CMAKE_BUILD_TYPE STREQUAL Coverage)
    set(COVERAGE_EXCLUDES
        "*/bswinfra/*"
        "*/build/*"
        "*/tests/*"
    )
    set(COVERAGE_BASE_DIR ${CMAKE_SOURCE_DIR}/source)
    set(COVERAGE_BRANCH_COVERAGE ON)
    set(COVERAGE_THRESHOLD_LINE 90)
    set(COVERAGE_THRESHOLD_FUNCTION 90)

    include(CoverageTarget)
endif()

configure_file(
    ${CMAKE_SOURCE_DIR}/shared/Defaults.h.in
    ${CMAKE_BINARY_DIR}/shared/Defaults.h)
    
if(WITH_TIDY)
    set(CMAKE_CXX_CLANG_TIDY "clang-tidy")
endif()

# Header files
include_directories(${CMAKE_SOURCE_DIR}/shared)
include_directories(${CMAKE_BINARY_DIR}/shared)

# Dependencies
find_package          (PkgConfig REQUIRED)
find_package          (Threads REQUIRED)
set                   (LIBS ${LIBS} pthread)

# Use jsoncpp
pkg_check_modules     (JSONCPP jsoncpp>=1.9.4 REQUIRED)
include_directories   (${JSONCPP_INCLUDE_DIRS})
set                   (LIBS ${LIBS} ${JSONCPP_LIBRARIES})

# Use sqlite3
find_package          (SQLite3 REQUIRED)
include_directories   (${SQLITE3_INCLUDE_DIR})

add_subdirectory(source)

if(WITH_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Build flags
add_compile_options (
    -Wall
    -Wextra
    -Wno-unused-function
    -Wformat
    -Wno-variadic-macros
    -Wno-strict-aliasing
    -D_FORTIFY_SOURCE=2
    -fstack-protector-strong
    -fwrapv
    -Wformat-signedness
    -Wmissing-include-dirs
    -Wimplicit-fallthrough=5
    -Wunused-parameter
    -Wuninitialized
    -Walloca
    -Wduplicated-branches
    -Wduplicated-cond
    -Wfloat-equal
    -Wshadow
    -Wcast-qual
    -Wconversion
    -Wsign-conversion
    -Wlogical-op
    -Werror
    -Wformat-security
    -Walloc-zero
    -Wcast-align
    -Wredundant-decls
    )

# Install license file
install(FILES ${CMAKE_SOURCE_DIR}/LICENSE DESTINATION ${CMAKE_INSTALL_PREFIX}/share/licenses/taskmonitor/)

# Status reporting
message (STATUS "SYSTEM_TYPE: "          ${CMAKE_SYSTEM_NAME})
message (STATUS "CMAKE_BUILD_TYPE: "     ${CMAKE_BUILD_TYPE})
message (STATUS "WITH_SYSLOG: "          ${WITH_SYSLOG})
message (STATUS "WITH_TESTS: "           ${WITH_TESTS})
message (STATUS "WITH_TIDY: "            ${WITH_TIDY})
