# feedme-core (web-)radio recorder

## What does it do?

Are you tired of your favourite radio shows still not providing a podcast service? Is your favourite music only played at nighttime? feedme-core allows you to record just about any radio webstream that syndicates mp3 streams via m3u playlists (which should be just about every station) according to a predefined schedule.

## How do I get it?

### Clone the repository

You know how to do this! Just `git clone https://github.com/niklasfi/feedme-core.git`.

### Installing the prerequisites

#### Build Tools

To build feedme-core, all you need is a c++ compiler of your choice (`clang++` and `g++` come to mind) and `cmake`. You will find them in your distributions package manager.

#### Libraries

Please make sure to have [`libconfig`](http://www.hyperrealm.com/libconfig/) and [`boost`](www.boost.org) (particularly the `system`, `filesystem`, `date_time` and `parse` sublibraries) available on your system. Don't be afraid you should be able to install them from your favourite package manager. You will probably need the respective development packages usually ending in `-devel` or `-dev` as well.

### Compiling

If all prerequisites are installed properly, all you need to do is to run the following three commands:

    cd bin
    cmake ../src
    make

### Configuring

Edit one of the `etc/config.*` files, or create one of your own. The sample config files are very verbose and contain a detailed explanation about creating your own recording shedule. Ensure to set your preferred `destinationPath` in which all your recordings will be stored.

### Runing

Running feedme-core is pretty simple. Just pass your config file as a parameter to the main executable

    bin/feedme-core path/to/your/config

### Registering as a systemd service

There is a sample systemd service file in the `etc` directory. You can adapt it to your needs by changing the `User` and `Group` as well as the path to the binary and config in the `ExecStart` setting.

To start the feedme-core service, call

    sudo systemctl start feedme-core

To ensure the feedme-core service is run at startup you need to enable it. To do so:

    sudo systemctl enable feedme-core

If you kept the `StandardOutput` setting in your service file, you can follow feedme-core's logs using

    sudo journalctl -u feedme-core -f -o cat

## Isn't this totally overengineered?

Of course it is! It all started out with a simple recording script which was called via cron jobs at regular intervals. For reference, this is it:

    #! /bin/bash

    tmp=$(mktemp)
    curl -s ${url} | wget -q -O "${tmp}" -i - &
    id=$!
    sleep ${rectime}
    (kill ${id}) 2>/dev/null
    chmod +r "${tmp}"
    mv "${tmp}" "${dst}"

If you want to try it, put it in a file called `download.sh`, mark it as executable and run it with

    dst=$(pwd) rectime=60 url=http://www.wdr.de/wdrlive/media/einslive.m3u ./download.sh

It turned out, that, while this is a good way to get started, it did not handle dropping connections very well; or at least not as well as I would have liked it to. So I wrote a node.js service which was a mess and never really worked and finally settled to redo it in a properly typed language with sane error handling.

In the end, the C++ solution is elegant in its own way.
- It utilizes worker threads which ensure that their assigned stream is always available.
- The use of boost_parse allows for a very concise way to formulate recording shedules as time constraints.
- It just works.
