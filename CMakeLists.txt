cmake_minimum_required(VERSION 3.7.2)
project(TaskMonitorReader
        LANGUAGES CXX
        VERSION 1.5.3
)

set(CMAKE_PROJECT_HOMEPAGE_URL "https://gitlab.com/taskmonitor/tkm-reader")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" "${CMAKE_CURRENT_SOURCE_DIR}/bswinfra/cmake")

# options
option(WITH_SYSLOG "Build with syslog logger backend" Y)
option(WITH_INSTALL_LICENSE "Install license file on target" Y)
option(WITH_TESTS "Build test suite" N)
option(WITH_TIDY "Build with clang-tidy" N)
option(WITH_ASAN "Build with address sanitize" N)
option(WITH_GCC_HARDEN_FLAGS "Build with GCC harden flags" N)

# Build time configuration setup
if(EXISTS .git)
    execute_process(
        COMMAND git rev-parse --short HEAD
        OUTPUT_VARIABLE GIT_SHA1
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    set(PROJECT_VERSION_TWEAK ${GIT_SHA1})
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING
        "Choose the type of build: Debug, Release, RelWithDebInfo, MinSizeRel." FORCE)
endif()

if(WITH_TIDY)
    set(CMAKE_CXX_CLANG_TIDY "clang-tidy")
endif()

if(WITH_ASAN)
    add_compile_options(-fsanitize=address)
    add_link_options(-fsanitize=address)
endif()

if(WITH_GCC_HARDEN_FLAGS)
    # Build flags
    add_compile_options (
        -Wall
        -Wextra
        -Wno-unused-function
        -Wformat
        -Wno-variadic-macros
        -Wno-strict-aliasing
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
endif()

# cmake modules
include(GNUInstallDirs)
include(Packing)
include(BSWInfra)

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

if (${CMAKE_SYSTEM_NAME} STREQUAL FreeBSD)
    link_directories(/usr/local/lib)
endif()

# generate
configure_file(
    ${CMAKE_SOURCE_DIR}/source/Defaults.h.in
    ${CMAKE_BINARY_DIR}/source/Defaults.h @ONLY)

# Dependencies
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
find_package(Protobuf REQUIRED)
find_package(tkm REQUIRED)
find_package(SQLite3 REQUIRED)

pkg_check_modules(JSONCPP jsoncpp>=1.9.4 REQUIRED)
include_directories(${JSONCPP_INCLUDE_DIRS})

# binary
add_executable(tkmreader
    source/Query.cpp
    source/JsonWriter.cpp
    source/Dispatcher.cpp
    source/Connection.cpp
    source/Application.cpp
    source/Arguments.cpp
    source/SQLiteDatabase.cpp
    source/Main.cpp
)

target_link_libraries(tkmreader
    PRIVATE
        BSWInfra
        pthread
        tkm::tkm
        jsoncpp
        sqlite3
        ${PROTOBUF_LIBRARY}
        ${JSONCPP_LIBRARY}
)

include_directories(
    ${Protobuf_INCLUDE_DIRS}
    ${jsoncpp_INCLUDE_DIRS}
    ${SQLite3_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/source
    ${CMAKE_BINARY_DIR}/source
)

if(WITH_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# install
install(TARGETS tkmreader RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Install license file
if(WITH_INSTALL_LICENSE)
    install(FILES ${CMAKE_SOURCE_DIR}/LICENSE DESTINATION ${CMAKE_INSTALL_PREFIX}/share/licenses/tkmreader/)
endif()

# Status reporting
message (STATUS "SYSTEM_TYPE: "             ${CMAKE_SYSTEM_NAME})
message (STATUS "CMAKE_BUILD_TYPE: "        ${CMAKE_BUILD_TYPE})
message (STATUS "WITH_INSTALL_LICENSE: "    ${WITH_INSTALL_LICENSE})
message (STATUS "WITH_SYSLOG: "             ${WITH_SYSLOG})
message (STATUS "WITH_TESTS: "              ${WITH_TESTS})
message (STATUS "WITH_TIDY: "               ${WITH_TIDY})
message (STATUS "WITH_ASAN: "               ${WITH_ASAN})
message (STATUS "WITH_GCC_HARDEN_FLAGS: "   ${WITH_GCC_HARDEN_FLAGS})
