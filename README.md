wsproxy-cpp
===========

WebSocket-NDNx proxy implemented in pure C++

Dependencies
------------

This program requires Boost ASIO library and C++11 features. Older platforms and compilers may not be able to configure and/or compile.

The code is tested on Mac OS X Mavericks with default clang++ compiler and Boost library 1.55.0.

Compile
-------

After cloning the Github repo, go to root folder and run the following two commands:

    git submodule init
    git submodule update

Then run the following two commands to compile:

    ./waf configure
    ./waf

The compiled binary is in ./build folder:

    ./build/ws_proxy

Options
-------

* -c [ndnd_addr]: specify the unix socket address for local ndnd. Default value is "/tmp/.ndnd.sock".

* -m [max_clients]: specify the number of maximum concurrent clients. Default value is 50.

* -p [proxy_port]: specify the port number for the proxy. Default value is 9696.
