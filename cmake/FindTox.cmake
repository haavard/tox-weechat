find_path(TOX_INCLUDE_DIR tox/tox.h)
find_path(TOXAV_INCLUDE_DIR tox/toxav.h)

find_library(TOX_LIBRARY toxcore)
find_library(TOXAV_LIBRARY toxav)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Tox DEFAULT_MSG TOX_INCLUDE_DIR TOXAV_INCLUDE_DIR TOX_LIBRARY TOXAV_LIBRARY)
