
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


Building and running headless on Raspberry Pi
=============================================

Starting from an official Raspbian Stretch Lite image.
Install the dependencies and build the applications as explained in
the previous sections.

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
