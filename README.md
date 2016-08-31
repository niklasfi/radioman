# feedme-core (web-)radio recorder

## What does it do?

Are you tired of your favourite radio shows still not providing a podcast service? Is your favourite music only played at nighttime? feedme-core allows you to record just about any radio webstream that syndicates mp3 streams either directly or via m3u or pls playlists (which should be just about every station) according to a predefined schedule.

### Features

- Schedule string syntax allows for the most complicated recording schedules.
- Recovers from connection losses gracefully without interupting the recording.
- Easy systemd service integration.

### The schedule

Let's look at a sample schedule:

    schedule: (
        ("1live", "m3u", "http://www.wdr.de/wdrlive/media/einslive.m3u", (
          ("hourly", "0M", 60),
          ("News", "0M", 5)
        )),
        ("wdr2", "m3u", "http://www.wdr.de/wdrlive/media/wdr2.m3u", (
          ("News", "0M", 5),
          ("MonTalk", "(19:05 & MON)", 115),
          ("Zugabe", "(22:30 & FRI)", 60),
          ("In Concert", "(23:05 & SUN)", 55)
        )),
        ("bbc-r1", "direct", "http://bbcmedia.ic.llnwd.net/stream/bbcmedia_radio1_mf_p", (
          ("News", "0M", 5)
        ))
    )

A schedule consists of a list of stations, which in turn consist of an identifier ("1live"), a strategy (either "direct" for direct downloads of mp3 streams or "m3u" for playlists), an URL ("http://www.wdr.de/wdrlive/media/einslive.m3u") and a list if programmes to be recorded on this station. A programme consists of an identifier, a schedule string and a duration in minutes. The schedule string encodes the conditions which have to be met to start a recording. For example *wdr2-In Concert* airs every Sunday at 11:05 PM. This can simply be stated as "(23:05 & SUN)". For a more in depth look at schedule strings and their syntax, look at the configuration files located in the `etc` directory.

### Where can I get stream urls from?

There are various listings of streaming urls available on the net. Here are a few:

- http://www.radio-browser.info/gui/#/topclick
- https://wiki.ubuntuusers.de/Internetradio/Stationen/#Radiosender-Deutschland
- http://www.thomas-oestreicher.de/radiostream.htm
- http://doc.ubuntu-fr.org/liste_radio_belgique
- http://mathewpeet.org/lists/BBC_radio_audio_streams/
- http://www.hendrikjansen.nl/henk/streaming.html

If you want to find a stream for a particular radio station it is often helpful to enter "<station-name> m3u" or "<station-name> mp3" into a search engine of your choice.

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

There is a sample systemd service file in the `etc` directory. You can adapt it to your needs by changing the `User` and `Group` as well as the path to the binary and config in the `ExecStart` setting. Once you are done, copy it to `etc/systemd/system/feedme-core.service` or create a symlink pointing to your local service file in this location. Finally you need to tell systemd to reload its configuration files by executing `sudo systemctl daemon-reload`.

To start the feedme-core service, call

    sudo systemctl start feedme-core

You can check the status of the feedme-core service with

    sudo systemctl status feedme core

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
