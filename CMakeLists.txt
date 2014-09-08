cmake_minimum_required(VERSION 2.8.7)

project(tox-weechat)

add_library(tox MODULE
    src/tox-weechat.c
    src/tox-weechat-tox.c
    src/tox-weechat-utils.c
    src/tox-weechat-commands.c
    src/tox-weechat-gui.c
    src/tox-weechat-chats.c
    src/tox-weechat-friend-requests.c
)
add_definitions(-D_GNU_SOURCE)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -Wall -Wextra -Werror-implicit-function-declaration -Wno-unused-parameter")

# remove lib prefix (libtox.so -> tox.so)
set_target_properties(tox PROPERTIES PREFIX "")

install(TARGETS tox DESTINATION lib/weechat/plugins)
