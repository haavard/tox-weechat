set(Tox_INCLUDE_DIRS)
set(Tox_LIBRARIES)

find_path(Tox_CORE_INCLUDE_DIR tox/tox.h)
find_library(Tox_CORE_LIBRARY toxcore)

find_path(Tox_AV_INCLUDE_DIR tox/toxav.h)
find_library(Tox_AV_LIBRARY toxav)

find_path(Tox_ENCRYPTSAVE_INCLUDE_DIR tox/toxencryptsave.h)
find_library(Tox_ENCRYPTSAVE_LIBRARY toxencryptsave)

find_path(Tox_DNS_INCLUDE_DIR tox/toxdns.h)
find_library(Tox_DNS_LIBRARY toxdns)

if(Tox_CORE_INCLUDE_DIR AND Tox_CORE_LIBRARY)
    set(Tox_CORE_FOUND TRUE)
    list(APPEND Tox_INCLUDE_DIRS ${Tox_CORE_INCLUDE_DIR})
    list(APPEND Tox_LIBRARIES ${Tox_CORE_LIBRARY})
endif()

if(Tox_AV_INCLUDE_DIR AND Tox_AV_LIBRARY)
    set(Tox_AV_FOUND TRUE)
    list(APPEND Tox_INCLUDE_DIRS ${Tox_AV_INCLUDE_DIR})
    list(APPEND Tox_LIBRARIES ${Tox_AV_LIBRARY})
endif()

if(Tox_ENCRYPTSAVE_INCLUDE_DIR AND Tox_ENCRYPTSAVE_LIBRARY)
    set(Tox_ENCRYPTSAVE_FOUND TRUE)
    list(APPEND Tox_INCLUDE_DIRS ${Tox_ENCRYPTSAVE_INCLUDE_DIR})
    list(APPEND Tox_LIBRARIES ${Tox_ENCRYPTSAVE_LIBRARY})
endif()

if(Tox_DNS_INCLUDE_DIR AND Tox_DNS_LIBRARY)
    set(Tox_DNS_FOUND TRUE)
    list(APPEND Tox_INCLUDE_DIRS ${Tox_DNS_INCLUDE_DIR})
    list(APPEND Tox_LIBRARIES ${Tox_DNS_LIBRARY})
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Tox FOUND_VAR Tox_FOUND REQUIRED_VARS Tox_INCLUDE_DIRS Tox_LIBRARIES HANDLE_COMPONENTS)
