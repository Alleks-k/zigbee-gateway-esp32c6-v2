cmake_minimum_required(VERSION 3.16)

# Pin project to a single ESP-IDF checkout to avoid build cache conflicts
# between different clones (e.g. /home/alex/esp/esp-idf vs /home/alex/dev/esp32/esp-idf).
get_filename_component(_project_root "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)
get_filename_component(_idf_from_repo "${_project_root}/../esp-idf" REALPATH)
set(_idf_from_env "$ENV{IDF_PATH}")

if(EXISTS "${_idf_from_repo}/tools/cmake/project.cmake")
    if(DEFINED ENV{IDF_PATH} AND NOT _idf_from_env STREQUAL "" AND NOT _idf_from_env STREQUAL _idf_from_repo)
        message(WARNING "Overriding IDF_PATH='${_idf_from_env}' with project-pinned '${_idf_from_repo}'")
    endif()
    set(ENV{IDF_PATH} "${_idf_from_repo}")
elseif(NOT DEFINED ENV{IDF_PATH} OR _idf_from_env STREQUAL "")
    message(FATAL_ERROR "IDF_PATH is not set and bundled ../esp-idf was not found")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp_zigbee_gateway)
