# Writes PRODUCT_VERSION (the CI tag or git describe) to OUT only when it changes, so objects rebuild only then
cmake_minimum_required(VERSION 3.10)

if(DEFINED ENV{GITHUB_REF_NAME} AND "$ENV{GITHUB_REF_NAME}" MATCHES "^v[0-9]")
    set(VERSION "$ENV{GITHUB_REF_NAME}")
else()
    execute_process(
        COMMAND git describe --tags --always --dirty
        WORKING_DIRECTORY "${REPO_ROOT}"
        OUTPUT_VARIABLE VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE GIT_DESCRIBE_RESULT)
    if(NOT GIT_DESCRIBE_RESULT EQUAL 0 OR NOT VERSION)
        set(VERSION "unknown")
    endif()
endif()

set(CONTENT "#define PRODUCT_VERSION \"${VERSION}\"\n")
set(OLD "")
if(EXISTS "${OUT}")
    file(READ "${OUT}" OLD)
endif()
if(NOT OLD STREQUAL CONTENT)
    file(WRITE "${OUT}" "${CONTENT}")
endif()
