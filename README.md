Tox-WeeChat
===========
Tox-WeeChat is a [Tox][1] protocol plugin for [WeeChat][2]. It is functional, but lacks certain features like Tox DNS.

Current build status: [![Build Status](https://travis-ci.org/haavardp/tox-weechat.svg?branch=master)][3]

Features
--------
Below is a list of implemented features, as well as features that will be supported in the future.

 - [x] One-to-one chats
 - [x] Group chats (text only)
 - [x] Proxies
 - [x] Faux offline messaging
 - [x] NoSpam editing
 - [x] Multiple profiles
 - [ ] Tox DNS
 - [ ] Encrypted save files
 - [ ] File transfer
 - [ ] Avatars
 - [ ] WeeChat `/upgrade`
 - [ ] Audio/video

Installation
------------
> Tox-WeeChat is available in the [AUR][4] and the [[haavard]][5] pacman repository.

Tox-WeeChat requires [WeeChat][2] >= 1.0.1, [SQLite][6] >= 3.6.19 and the latest-ish [libtoxcore][7]. It also requires CMake to be built. Installation is fairly simple; after getting the source, compile and install using CMake:

    $ mkdir build && cd build
    $ cmake -DPLUGIN_PATH=~/.weechat/plugins ..
    $ make install

This installs the plugin binary `tox.so` to the recommended location `~/.weechat/plugins`. The default location is `/usr/local/lib/weechat/plugins`.

Usage
-----
 - If the plugin does not load automatically, load it with `/plugin load tox`. You may have to specify the full path to the plugin binary.
 - Create a new profile with `/tox create <profile name>`. The data file is stored in `~/.weechat/tox/` by default.
 - Load your profile and connect to the Tox network with `/tox load <profile name>`.
 - Run `/help -listfull tox` to get a list of all available commands, and `/set tox.*` for a list of options.

### Common issues

#### Long Tox names messing up WeeChat layout
Tox allows names up to 128 bytes long. To prevent long names from taking all your screen space, you can set the following options in WeeChat:
 - `weechat.bar.nicklist.size_max`
 - `weechat.look.prefix_align_max`
 - `buffers.look.name_size_max` (if using buffers.pl)

#### Tox won't connect through my proxy
Make sure the address and port is correct, and that UDP is disabled (`/set tox.profile.*.udp`).

License
---------
Copyright (c) 2015 Håvard Pettersson <mail@haavard.me>

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

[1]: http://tox.im
[2]: http://weechat.org
[3]: https://travis-ci.org/haavardp/tox-weechat
[4]: https://aur.archlinux.org/packages/tox-weechat-git
[5]: https://haavard.me/archlinux
[6]: http://www.sqlite.org
[7]: https://github.com/irungentoo/toxcore

