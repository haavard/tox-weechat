#
# Copyright (c) 2017 Håvard Pettersson <mail@haavard.me>.
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

set(Tox_INCLUDE_DIRS)
set(Tox_LIBRARIES)

find_path(Tox_CORE_INCLUDE_DIR tox/tox.h)
find_library(Tox_CORE_LIBRARY toxcore)

find_path(Tox_AV_INCLUDE_DIR tox/toxav.h)
find_library(Tox_AV_LIBRARY toxav)

find_path(Tox_ENCRYPTSAVE_INCLUDE_DIR tox/toxencryptsave.h)
find_library(Tox_ENCRYPTSAVE_LIBRARY toxencryptsave)

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

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Tox
                                  FOUND_VAR Tox_FOUND
                                  REQUIRED_VARS Tox_INCLUDE_DIRS Tox_LIBRARIES
                                  HANDLE_COMPONENTS)
