Tox-WeeChat
===========

Tox-WeeChat is a C plugin for the [WeeChat][1] chat client that enables it to connect to the [Tox][2] network. It is functional, but not intended for regular use yet, as it lacks a ton of features. Bug reports and suggestions are appreciated!

Installation
------------

Tox-WeeChat requires [libtoxcore][3] to work. After getting it, install Tox-WeeChat like this:

    $ git clone https://github.com/haavardp/tox-weechat.git
    $ cd tox-weechat
    $ mkdir build
    $ cd build

    $ cmake ..
    $ make install

This places `tox.so` in `/usr/local/lib/weechat/plugins`. If you don't have root access or otherwise would like to install it locally, replace the cmake command above with this one: `cmake -DPREFIX=$HOME/.weechat ..`.

Usage
-----

In WeeChat, load the plugin: `/plugin load tox`. You should get a new buffer called tox. This is the core Tox buffer, where output from Tox commands will print.

 - To change your name, `/name <new name>`.
 - Get your Tox address with `/myaddress`.
 - To add friends or respond to friend requests, `/help friend` will get you started.
 - Message a friend with `/msg <friend number>`. Get their friend number with `/friend`.

That's pretty much all that's implemented for now.

License
---------

Tox-WeeChat is licensed under the MIT license. For more information, see the LICENSE file.

Copyright (c) 2014 Håvard Pettersson <haavard.pettersson@gmail.com>.

[1]: http://weechat.org
[2]: http://tox.im
[3]: https://github.com/irungentoo/toxcore
