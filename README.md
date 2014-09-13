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
    $ cmake -DHOME_FOLDER_INSTALL=ON ..
    $ make
    $ make install

This installs the plugin binary `tox.so` in the recommended location `~/.weechat/plugins`. To install the plugin somewhere else, replace the cmake command above with either of these:

 - To install to `/usr/local/lib/weechat/plugins`: `cmake ..`
 - To install to `/usr/lib/weechat/plugins`: `cmake -DCMAKE_INSTALL_PREFIX=/usr ..`
 - To install a custom path: `cmake -DINSTALL_PATH=/some/path ..`

You may also need to `sudo make install`, depending on permissions.

Usage
-----
In WeeChat, load the plugin: `/plugin load tox`. In cases where WeeChat can't find the plugin, try specifying the full path to the binary. You should get a new buffer called tox. This is the core Tox buffer, where output from commands will appear.

 - In WeeChat, load the plugin with `/plugin load tox`. If it fails, try specifying the full path to the binary.
 - Create a new identity with `/tox add <name>`. The data file is created in `<WeeChat home>/tox/<name>` by default. Can be changed with `/set tox.identity.<name>.save_file`.
 - Connect your new identity to the Tox network with `/tox connect <name>`.

The following commands must be executed on a Tox buffer:

 - To change your name, `/name <new name>`.
 - Get your Tox address with `/myaddress`.
 - To add friends or respond to friend requests, `/help friend` will get you started.
 - Message a friend with `/msg <friend number>`. Get their friend number with `/friend`.

A list of commands is available with `/help -list tox`.

TODO
----
 - [x] Support multiple identities
 - [ ] Preserve friend requests when closing
 - [ ] Group chats
 - [ ] Polish and reach a "stable" release
   - [ ] Add autocomplete to all commands

License
---------
Tox-WeeChat is licensed under the MIT license. For more information, see the LICENSE file.

Copyright (c) 2014 Håvard Pettersson <haavard.pettersson@gmail.com>.

[1]: http://weechat.org
[2]: http://tox.im
[3]: https://github.com/irungentoo/toxcore

