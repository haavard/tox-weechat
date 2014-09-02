cmake_minimum_required(VERSION 3.0)

project(tox-weechat)
add_definitions(-std=c11)

add_library(tox SHARED
    src/tox-weechat.c
    src/tox-weechat-tox.c
    src/tox-weechat-utils.c
    src/tox-weechat-commands.c
    src/tox-weechat-gui.c
    src/tox-weechat-chats.c
    src/tox-weechat-friend-requests.c
)

if (PREFIX)
    set(CMAKE_INSTALL_PREFIX ${PREFIX})
else()
    set(CMAKE_INSTALL_PREFIX /usr/local/lib/weechat)
endif()

# remove lib prefix
set_target_properties(tox PROPERTIES PREFIX "")

target_link_libraries(tox toxcore)

install(FILES build/tox.so DESTINATION plugins)
