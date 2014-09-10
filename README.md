Tox-WeeChat
===========
Tox-WeeChat is a C plugin for the [WeeChat][1] chat client that enables it to connect to the [Tox][2] network. It is functional, but fairly limited in features for now. Bug reports and suggestions are appreciated.

[![Build Status](https://travis-ci.org/haavardp/tox-weechat.svg?branch=master)](https://travis-ci.org/haavardp/tox-weechat)

Installation
------------
Tox-WeeChat requires [libtoxcore][3] (and WeeChat) to work. After getting them, install Tox-WeeChat like this:

    $ git clone https://github.com/haavardp/tox-weechat.git
    $ cd tox-weechat
    $ mkdir build && cd build

Now, depending on where you want to install the plugin binary:
 - To install to `~/.weechat/plugins` (recommended): `cmake -DHOME_FOLDER_INSTALL=ON ..`
 - To install to `/usr/local/lib/weechat/plugins`: `cmake ..`
 - To install to `/usr/lib/weechat/plugins`: `cmake -DCMAKE_INSTALL_PREFIX=/usr ..`
 - To install somewhere else: `cmake -DINSTALL_PATH=/some/path ..`

Next, install the plugin. You may need sudo for the last command, depending on install location.
    $ make
    $ make install

Usage
-----
In WeeChat, load the plugin: `/plugin load tox`. In cases where WeeChat can't find the plugin, try specifying the full path to the binary. You should get a new buffer called tox. This is the core Tox buffer, where output from commands will appear.

 - To change your name, `/name <new name>`.
 - Get your Tox address with `/myaddress`.
 - To add friends or respond to friend requests, `/help friend` will get you started.
 - Message a friend with `/msg <friend number>`. Get their friend number with `/friend`.

A list of commands is available with `/help -list tox`.

TODO
----
 - [ ] Support multiple identities (in progress)
 - [ ] Preserve friend requests when closing
 - [ ] Group chats
 - [ ] Polish and reach a "stable" release

License
---------
Tox-WeeChat is licensed under the MIT license. For more information, see the LICENSE file.

Copyright (c) 2014 Håvard Pettersson <haavard.pettersson@gmail.com>.

[1]: http://weechat.org
[2]: http://tox.im
[3]: https://github.com/irungentoo/toxcore

