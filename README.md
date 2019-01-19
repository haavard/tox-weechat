# Tox-WeeChat
Tox-WeeChat is a [Tox][1] protocol plugin for [WeeChat][2]. It is functional,
but lacks certain features that might be expected of a full-fledged Tox client.

Tox-WeeChat is compliant with all "Required" points in the [Tox Client
Standard][3].

[![Build Status](https://travis-ci.org/haavard/tox-weechat.svg?branch=master)][4]

## Features
 - One-to-one chats
 - Group chats (text only)
 - Proxy support
 - Multiple profiles
 - Encrypted save files
 - File transfer

## Installation
Tox-WeeChat requires [WeeChat][2] (tested with version 2.3) and [TokTok
c-toxcore][5] (tested with version 0.2.8). CMake 2.8.12 or newer is also
required to build. Installation is fairly simple; after getting the source
code, compile and install using CMake:

    $ mkdir build && cd build
    $ cmake -DPLUGIN_PATH=~/.weechat/plugins ..
    $ make install

This installs the plugin binary `tox.so` to the recommended location
`~/.weechat/plugins`. The default location is `/usr/local/lib/weechat/plugins`.

If WeeChat or toxcore are installed in a non-standard location, you can try
specifying `CMAKE_PREFIX_PATH` to find them; see [.travis.yml](.travis.yml) for
an example.

## Usage
 - If the plugin does not load automatically, load it with `/plugin load tox`.
   You may have to specify the full path to the plugin binary if you installed
   it to a non-standard location.
 - Create a new profile with `/tox create <profile name>`. The data file is
   stored in `~/.weechat/tox/` by default.
 - Load your profile and connect to the Tox network with
   `/tox load <profile name>`.
 - Run `/help -listfull tox` to get a list of all available commands, and
   `/set tox.*` for a list of options.

### Common issues
#### Long Tox names messing up WeeChat layout
Tox allows names up to 128 bytes long. To prevent long names from taking all
your screen space, you can set the following options in WeeChat:
 - `weechat.bar.nicklist.size_max`
 - `weechat.look.prefix_align_max`
 - `buffers.look.name_size_max` (if using buffers.pl)

#### Tox won't connect through my proxy
Make sure the proxy type, address and port is correct, and that UDP is
disabled (`/set tox.profile.*.udp`).

## License
Copyright (c) 2018 Håvard Pettersson <mail@haavard.me>

This file is part of Tox-WeeChat.

Tox-WeeChat is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Tox-WeeChat is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Tox-WeeChat.  If not, see <http://www.gnu.org/licenses/>.

[1]: http://tox.chat
[2]: http://weechat.org
[3]: https://github.com/Tox/Tox-Client-Standard
[4]: https://travis-ci.org/haavard/tox-weechat
[5]: https://github.com/TokTok/c-toxcore

