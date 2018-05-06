
Dependencies
============

* ALSA
* jackd
* jack-plumbing (package jack-tools) or qjackctl
* a2jmidid
* fluidsynth

```
sudo apt install a2jmidid jack-tools fluidsynth
```

Building
========

Install the Rust tool chain:

```
curl https://sh.rustup.rs -sSf | sh
source $HOME/.cargo/env
```

Build applications:

```
cd software/midi314-display
cargo build --release

cd ../midi314-looper
cargo build --release
cd -
```

Install:

```
sudo software/scripts/install.sh
```

Running
=======

Execute the `midi314.sh` script from the command line.

```
midi314.sh
```

Press a key to stop.

Building and running headless on Raspberry Pi
=============================================

Starting from an official Raspbian Stretch Lite image.
Install the dependencies and build the applications as explained in
the previous sections.

Low-latency configuration
-------------------------

Edit `/etc/security/limits.d/audio.conf`:

```
@audio - rtprio 80
@audio - memlock unlimited
```

Check that the user `pi` belongs to the `audio` group.

Reduce the swap usage:

```
sudo /sbin/sysctl -w vm.swappiness=10
```

Headless Jackd
--------------

Install Jackd without the dbus dependency:
[Running Jackd headless](https://capocasa.net/jackd-headless)

```
sudo apt install build-essential libsamplerate0-dev libasound2-dev libsndfile1-dev git

git clone git://github.com/jackaudio/jack2.git
cd jack2

./waf configure --alsa --prefix=/usr/local --libdir=/usr/lib/arm-linux-gnueabihf
./waf
sudo ./waf install
```

LCD Display
-----------

In `/boot/config.txt`, uncomment the line:

```
dtparam=spi=on
```

Run on boot
-----------

To run `midi314.sh` automatically when the board boots, do the following:

```
sudo systemctl edit midi314 --force
```

Add the following content to the service definition:

```
[Unit]
Description=midi@3:14
After=multi-user.target

[Service]
Type=idle
Environment="DEVICE=hw:0"
Environment="INTERFACE=service"
Environment="PATH=/usr/local/bin:/usr/bin:/bin"
ExecStart=/usr/bin/midi314.sh
User=pi

[Install]
WantedBy=multi-user.target
```

Enable the service:

```
sudo systemctl enable midi314.service
sudo reboot
```
