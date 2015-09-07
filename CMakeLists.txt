#
# Copyright (c) 2015 Håvard Pettersson <mail@haavard.me>.
#
# This file is part of Tox-WeeChat.
#
# Tox-WeeChat is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# Tox-WeeChat is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Tox-WeeChat.  If not, see <http://www.gnu.org/licenses/>.
#

cmake_minimum_required(VERSION 2.8.11)

project(tox-weechat C)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

find_package(WeeChat REQUIRED)
find_package(Tox REQUIRED
    COMPONENTS CORE
    OPTIONAL_COMPONENTS AV ENCRYPTSAVE)

set(PLUGIN_PATH "lib/weechat/plugins" CACHE PATH
    "Path to install the plugin binary to.")

add_library(tox MODULE
    src/twc.c
    src/twc-bootstrap.c
    src/twc-chat.c
    src/twc-commands.c
    src/twc-completion.c
    src/twc-config.c
    src/twc-friend-request.c
    src/twc-gui.c
    src/twc-group-invite.c
    src/twc-list.c
    src/twc-message-queue.c
    src/twc-profile.c
    src/twc-tox-callbacks.c
    src/twc-utils.c
)

set(CMAKE_C_FLAGS_DEBUG "-DTWC_DEBUG")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -Wall -Wextra -Wno-unused-parameter")

include_directories(${Tox_INCLUDE_DIRS})
include_directories(${WeeChat_INCLUDE_DIRS})

target_link_libraries(tox ${Tox_LIBRARIES})

if(Tox_AV_FOUND)
    add_definitions(-DTOXAV_ENABLED)
endif()

if(Tox_ENCRYPTSAVE_FOUND)
    add_definitions(-DTOXENCRYPTSAVE_ENABLED)
endif()

# remove lib prefix (libtox.so -> tox.so)
set_target_properties(tox PROPERTIES PREFIX "")

install(TARGETS tox DESTINATION "${PLUGIN_PATH}")

