cmake_minimum_required(VERSION 2.8.7)

project(tox-weechat)
add_definitions(-std=c99 -D_GNU_SOURCE)

add_library(tox SHARED
    src/tox-weechat.c
    src/tox-weechat-tox.c
    src/tox-weechat-utils.c
    src/tox-weechat-commands.c
    src/tox-weechat-gui.c
    src/tox-weechat-chats.c
    src/tox-weechat-friend-requests.c
)

# remove lib prefix
set_target_properties(tox PROPERTIES PREFIX "")

target_link_libraries(tox toxcore)

install(TARGETS tox DESTINATION ~/.weechat/plugins)
