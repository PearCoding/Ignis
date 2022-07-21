find_package(Git QUIET)
if(NOT GIT_FOUND OR NOT EXISTS "${PROJECT_SOURCE_DIR}/.git")
    set(GIT_BRANCH "HEAD")
    set(GIT_REVISION "")
    set(GIT_DATE "")
    set(GIT_SUBJECT "")
    set(GIT_DIRTY "true")
    return()
endif()

# Get the current working branch
execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_BRANCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Get the latest abbreviated commit hash of the working branch
execute_process(
    COMMAND ${GIT_EXECUTABLE} log -1 --format=%H
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_REVISION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Get the latest date of commit
execute_process(
    COMMAND ${GIT_EXECUTABLE} log -1 --format=%ci
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_DATE
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Get the latest subject of commit
execute_process(
    COMMAND ${GIT_EXECUTABLE} log -1 --format=%s
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_SUBJECT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Check if commit is dirty
set(_GIT_DIRTY "")
execute_process(
    COMMAND ${GIT_EXECUTABLE} status --porcelain -unormal
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE _GIT_DIRTY
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(NOT "${_GIT_DIRTY}" STREQUAL "")
    set(GIT_DIRTY "true")
else()
    set(GIT_DIRTY "false")
endif()