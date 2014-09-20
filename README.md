Tox-WeeChat
===========
Tox-WeeChat is a C plugin for the [WeeChat][1] chat client that enables it to connect to the [Tox][2] network. It is functional, but fairly limited in features and not intended for general use yet.

Current build status: [![Build Status](https://travis-ci.org/haavardp/tox-weechat.svg?branch=master)](https://travis-ci.org/haavardp/tox-weechat)

Installation
------------
> Tox-WeeChat is available on the [AUR][3].

Tox-WeeChat requires [WeeChat][1] >=1.0, [libjansson][4] >=2.5, and the latest-ish [libtoxcore][5]. It also requires CMake to be built. Installation is fairly simple; after getting the source, compile and install using CMake:

    $ mkdir build && cd build
    $ cmake -DHOME_FOLDER_INSTALL=ON ..
    $ make
    $ make install

This installs the plugin binary `tox.so` to the recommended location `~/.weechat/plugins`. Omitting the home folder flag installs to `/usr/local/lib/weechat/plugins`. Installing to a custom WeeChat home or similar is achieved by setting `INSTALL_PATH`.

Usage
-----
 - If the plugin does no automatically load, load it with `/plugin load tox`. You may have to specify the full path to the plugin binary.
 - Create a new identity with `/tox create <name>`. The data file is stored in `~/.weechat/tox/` by default.
 - Connect your identity to the Tox network with `/tox connect <name>`.
 - Change your name with `/name <new name>`.
 - Get your Tox ID with `/myid`.
 - To add friends or respond to friend requests, `/help friend` will get you started.
 - Message a friend with `/msg <friend number>`. Get their friend number with `/friend list`.

A list of commands is available with `/help -list tox`.

TODO & Implemented features
----
 - [x] Adding friends, one-to-one chats
 - [x] Support multiple identities
 - [x] Save friend requests
 - [ ] Encrypted save files
 - [ ] Tox DNS
 - [ ] Group chats (awaiting libtoxcore implementation)
 - [ ] Support proxies (TOR)
 - [ ] A/V (long term)

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

[1]: http://weechat.org
[2]: http://tox.im
[3]: https://aur.archlinux.org/packages/tox-weechat-git
[4]: http://www.digip.org/jansson/
[5]: https://github.com/irungentoo/toxcore

