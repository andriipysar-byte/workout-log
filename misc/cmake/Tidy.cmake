# clang-tidy integration. Adapted from ../remoteguard/CERAMIC/misc/cmake/Tidy.cmake
# (the `tidy-revisor` baseline-ratchet system is not carried over here -- this
# project has no misc/ci/ tracking infrastructure to ratchet against; add it if
# the check backlog ever needs one).
#
# Target (only added when a clang-tidy binary is found):
#   tidy   run clang-tidy over all first-party C/C++ translation units,
#          reporting diagnostics (no source files modified)
#
# Files are analyzed in parallel via run-clang-tidy with -j WL_TIDY_THREADS
# (default: processor count; override with -DWL_TIDY_THREADS=N). When
# run-clang-tidy is unavailable the target falls back to a single serial
# clang-tidy invocation.
#
# Checks are read from the top-level .clang-tidy (passed explicitly via
# --config-file), and diagnostics are resolved against the build's
# compile_commands.json -- which is why this module forces
# CMAKE_EXPORT_COMPILE_COMMANDS on. Only first-party trees are analyzed; fetched
# deps under build*/_deps are never touched.
# Override the binary with -DCLANG_TIDY_BIN=/path/to/clang-tidy.

set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Export compile_commands.json" FORCE)

# NOTE: push generic name to the end to make checks more deterministic
find_program(CLANG_TIDY_BIN
    NAMES clang-tidy-18 clang-tidy-17 clang-tidy-16 clang-tidy
    DOC "clang-tidy executable used by the `tidy` target")

if(NOT CLANG_TIDY_BIN)
    message(STATUS "clang-tidy not found; `tidy` target disabled")
    return()
else()
    execute_process(
        COMMAND ${CLANG_TIDY_BIN} --version
        OUTPUT_VARIABLE CLANG_TIDY_VERSION_OUTPUT
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(CLANG_TIDY_VERSION_OUTPUT MATCHES "version ([0-9]+\\.[0-9]+\\.[0-9]+)")
        set(CLANG_TIDY_VERSION ${CMAKE_MATCH_1})
    else()
        set(CLANG_TIDY_VERSION ${CLANG_TIDY_VERSION_OUTPUT})
    endif()
    message(STATUS "Found clang-tidy: ${CLANG_TIDY_BIN} (version ${CLANG_TIDY_VERSION})")
endif()

# First-party source roots (mirrors the add_subdirectory list in CMakeLists.txt).
# capi/ and ui/ are listed even though they're only populated by later milestones
# (see docs/05-architecture.md) -- file(GLOB_RECURSE) over an absent directory is
# harmless, and this avoids a second edit when they land.
set(WL_TIDY_DIRS
    core
    tools
    capi
    ui
    )

# Only translation units are passed to clang-tidy; headers are analyzed
# transitively when included, and clang-tidy cannot compile a bare header
# without the flags from a compile_commands.json entry.
set(_tidy_globs "")
foreach(_dir IN LISTS WL_TIDY_DIRS)
    foreach(_ext cpp cc cxx c)
        list(APPEND _tidy_globs "${CMAKE_SOURCE_DIR}/${_dir}/*.${_ext}")
    endforeach()
endforeach()

file(GLOB_RECURSE WL_TIDY_SOURCES CONFIGURE_DEPENDS ${_tidy_globs})
list(LENGTH WL_TIDY_SOURCES _tidy_count)

set(_tidy_config "${CMAKE_SOURCE_DIR}/.clang-tidy")

include(ProcessorCount)
ProcessorCount(_nproc)
if(_nproc LESS 1)
    set(_nproc 1)
endif()
set(WL_TIDY_THREADS "${_nproc}" CACHE STRING "Parallel clang-tidy jobs (-j) for the tidy target")

find_program(RUN_CLANG_TIDY_BIN
    NAMES run-clang-tidy-18 run-clang-tidy-17 run-clang-tidy-16 run-clang-tidy
    DOC "run-clang-tidy wrapper used to analyze files in parallel")

# run-clang-tidy selects translation units from the compile DB by a path regex
# rather than a file list; anchor it to the first-party roots so fetched deps stay out.
string(JOIN "|" _tidy_dirs_alt ${WL_TIDY_DIRS})
set(_tidy_path_re "^${CMAKE_SOURCE_DIR}/(${_tidy_dirs_alt})/")

if(RUN_CLANG_TIDY_BIN)
    add_custom_target(tidy
        COMMAND ${RUN_CLANG_TIDY_BIN} -p ${CMAKE_BINARY_DIR} -j ${WL_TIDY_THREADS}
                -clang-tidy-binary ${CLANG_TIDY_BIN} -config-file=${_tidy_config}
                -quiet "${_tidy_path_re}"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Analyzing ${_tidy_count} translation units with run-clang-tidy -j ${WL_TIDY_THREADS}"
        USES_TERMINAL
        VERBATIM)
else()
    message(STATUS "run-clang-tidy not found; `tidy` runs clang-tidy serially")
    add_custom_target(tidy
        COMMAND ${CLANG_TIDY_BIN} -p ${CMAKE_BINARY_DIR} --config-file=${_tidy_config} ${WL_TIDY_SOURCES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Analyzing ${_tidy_count} C/C++ translation units with ${CLANG_TIDY_BIN} (serial)"
        COMMAND_EXPAND_LISTS
        VERBATIM)
endif()
