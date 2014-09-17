Tox-WeeChat
===========
Tox-WeeChat is a C plugin for the [WeeChat][1] chat client that enables it to connect to the [Tox][2] network. It is functional, but fairly limited in features and not intended for general use yet.

Current build status: [![Build Status](https://travis-ci.org/haavardp/tox-weechat.svg?branch=master)](https://travis-ci.org/haavardp/tox-weechat)

Installation
------------
Tox-WeeChat requires the latest [libtoxcore][3] and WeeChat 1.0. It also requires cmake to be built. Installation is fairly simple:

    $ git clone https://github.com/haavardp/tox-weechat.git
    $ cd tox-weechat
    $ mkdir build && cd build
    $ cmake -DHOME_FOLDER_INSTALL=ON ..
    $ make
    $ make install

This installs the plugin binary `tox.so` in the recommended location `~/.weechat/plugins`. Omitting the home folder flag installs it to `/usr/local/lib/weechat/plugins`. Install anywhere else by setting `INSTALL_PATH`.

Usage
-----
 - In WeeChat, load the plugin with `/plugin load tox`. If it fails, try specifying the full path to the binary.
 - Create a new identity with `/tox add <name>`. The data file is stored in `<WeeChat home>/tox/` by default.
 - Connect your identity to the Tox network with `/tox connect <name>`.
 - Change your name with `/name <new name>`.
 - Get your Tox address with `/myaddress`.
 - To add friends or respond to friend requests, `/help friend` will get you started.
 - Message a friend with `/msg <friend number>`. Get their friend number with `/friend list`.

A list of commands is available with `/help -list tox`.

TODO/Implemented features
----
 - [x] Adding friends, one-to-one chats
 - [x] Support multiple identities
 - [ ] Encrypted save files
 - [ ] Save friend requests
 - [ ] Tox DNS
 - [ ] Group chats (awaiting libtoxcore implementation)
 - [ ] Support proxies (TOR)
 - [ ] A/V (long term)

License
---------
Tox-WeeChat is licensed under the MIT license. For more information, see the LICENSE file.

Copyright (c) 2014 Håvard Pettersson <haavard.pettersson@gmail.com>.

[1]: http://weechat.org
[2]: http://tox.im
[3]: https://github.com/irungentoo/toxcore

