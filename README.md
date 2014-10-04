Tox-WeeChat
===========
Tox-WeeChat is a [Tox][1] protocol plugin for [WeeChat][2]. It is functional, but lacks certain features like Tox DNS (e.g. user@toxme.se) and group chats.

Current build status: [![Build Status](https://travis-ci.org/haavardp/tox-weechat.svg?branch=master)][3]

Installation
------------
> Tox-WeeChat is available in the [AUR][4].

Tox-WeeChat requires [WeeChat][1] >= 1.0.1, [SQLite][5] >= 3.6.19 and the latest-ish [libtoxcore][6]. It also requires CMake to be built. Installation is fairly simple; after getting the source, compile and install using CMake:

    $ mkdir build && cd build
    $ cmake -DHOME_FOLDER_INSTALL=ON ..
    $ make install

This installs the plugin binary `tox.so` to the recommended location `~/.weechat/plugins`. Without the home folder flag, the binary is placed in `/usr/local/lib/weechat/plugins`. Installing to a custom WeeChat folder or elsewhere is achieved by setting `INSTALL_PATH`.

Usage
-----
 - If the plugin does not load automatically, load it with `/plugin load tox`. You may have to specify the full path to the plugin binary.
 - Create a new profile with `/tox create <name>`. The data file is stored in `~/.weechat/tox/` by default.
 - Load your profile and connect to the Tox network with `/tox load <name>`.
 - Change your name with `/name <new name>`.
 - Get your Tox ID with `/myid`.
 - To add friends or respond to friend requests, `/help friend` will get you started.
 - Message a friend with `/msg <friend number>`. Get their friend number with `/friend list`.

Run `/help -listfull tox` to get a list of all available commands.

TODO
----
 - Persist data (friend requests etc.)
 - Support encrypted save files
 - Tox DNS
 - Group chats
 - Support proxies (e.g. TOR)
 - Support WeeChat `/upgrade`
 - Audio/video chats

License
---------
Copyright (c) 2014 Håvard Pettersson <haavard.pettersson@gmail.com>

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
[5]: http://www.sqlite.org
[6]: https://github.com/irungentoo/toxcore

