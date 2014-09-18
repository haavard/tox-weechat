cmake_minimum_required(VERSION 2.8.7)

project(tox-weechat C)

option(HOME_FOLDER_INSTALL "Install to ~/.weechat/plugins instead of default location." OFF)
if(HOME_FOLDER_INSTALL)
    set(INSTALL_PATH "~/.weechat/plugins")
endif()

if (NOT DEFINED INSTALL_PATH OR "${INSTALL_PATH}" STREQUAL "")
    set(INSTALL_PATH "lib/weechat/plugins")
endif()
set(INSTALL_PATH "${INSTALL_PATH}" CACHE PATH "Path to install the plugin binary to.")

add_library(tox MODULE
    src/tox-weechat.c
    src/tox-weechat-identities.c
    src/tox-weechat-chats.c
    src/tox-weechat-friend-requests.c
    src/tox-weechat-tox-callbacks.c
    src/tox-weechat-commands.c
    src/tox-weechat-gui.c
    src/tox-weechat-utils.c
    src/tox-weechat-config.c
)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -Wall -Wextra -Werror-implicit-function-declaration -Wno-unused-parameter")

target_link_libraries(tox toxcore)

# remove lib prefix (libtox.so -> tox.so)
set_target_properties(tox PROPERTIES PREFIX "")

install(TARGETS tox DESTINATION "${INSTALL_PATH}")

