set(Ldns_INCLUDE_DIRS)
set(Ldns_LIBRARIES)

find_path(Ldns_INCLUDE_DIR ldns/ldns.h)
find_library(Ldns_LIBRARY ldns)

if (Ldns_INCLUDE_DIR AND Ldns_LIBRARY)
    set(Ldns_FOUND TRUE)
    list(APPEND Ldns_LIBRARIES ldns)
    list(APPEND Ldns_INCLUDE_DIRS ${Ldns_INCLUDE_DIR})
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Ldns FOUND_VAR Ldns_FOUND REQUIRED_VARS Ldns_INCLUDE_DIRS Ldns_LIBRARIES)
