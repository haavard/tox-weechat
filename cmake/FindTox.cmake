find_path(Tox_CORE_INCLUDE_DIR tox/tox.h)
find_library(Tox_CORE_LIBRARY toxcore)

if(Tox_CORE_INCLUDE_DIR AND Tox_CORE_LIBRARY)
    set(Tox_CORE_FOUND TRUE)
endif()

find_path(Tox_AV_INCLUDE_DIR tox/toxav.h)
find_library(Tox_AV_LIBRARY toxav)

if(Tox_AV_INCLUDE_DIR AND Tox_AV_LIBRARY)
    set(Tox_AV_FOUND TRUE)
endif()

set(Tox_INCLUDE_DIRS)
set(Tox_LIBRARIES)

if(Tox_CORE_FOUND)
    list(APPEND Tox_INCLUDE_DIRS ${Tox_CORE_INCLUDE_DIR})
    list(APPEND Tox_LIBRARIES ${Tox_CORE_LIBRARY})
endif()

if(Tox_AV_FOUND)
    list(APPEND Tox_INCLUDE_DIRS ${Tox_AV_INCLUDE_DIR})
    list(APPEND Tox_LIBRARIES ${Tox_AV_LIBRARY})
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Tox HANDLE_COMPONENTS REQUIRED_VARS Tox_INCLUDE_DIRS Tox_LIBRARIES)
