Tox-WeeChat
===========
Tox-WeeChat is a C plugin for the [WeeChat][1] chat client that enables it to connect to the [Tox][2] network. It is functional, but very limited in features right now. Bug reports and suggestions are appreciated.

[![Build Status](https://travis-ci.org/haavardp/tox-weechat.svg?branch=master)](https://travis-ci.org/haavardp/tox-weechat)

Installation
------------
Tox-WeeChat requires [libtoxcore][3] to work. After getting it, install Tox-WeeChat like this:

    $ git clone https://github.com/haavardp/tox-weechat.git
    $ cd tox-weechat
    $ mkdir build && cd build
    $ cmake ..
    $ make

This builds the plugin binary `tox.so`. Copy/move this file to `~/.weechat/plugins/` (or your equivalent WeeChat home folder) and you're done!

Usage
-----
In WeeChat, load the plugin: `/plugin load tox` (`/plugin load tox.dylib` on OSX). You should get a new buffer called tox. This is the core Tox buffer, where output from commands will print.

 - To change your name, `/name <new name>`.
 - Get your Tox address with `/myaddress`.
 - To add friends or respond to friend requests, `/help friend` will get you started.
 - Message a friend with `/msg <friend number>`. Get their friend number with `/friend`.

A list of commands is available with `/help -list tox`.

License
---------
Tox-WeeChat is licensed under the MIT license. For more information, see the LICENSE file.

Copyright (c) 2014 Håvard Pettersson <haavard.pettersson@gmail.com>.

[1]: http://weechat.org
[2]: http://tox.im
[3]: https://github.com/irungentoo/toxcore

