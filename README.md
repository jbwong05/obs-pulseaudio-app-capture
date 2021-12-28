# obs-pulseaudio-app-capture

[![latest-tag](https://badgen.net/github/release/jbwong05/obs-pulseaudio-app-capture)](https://github.com/jbwong05/obs-pulseaudio-app-capture)
[![Downloads](https://img.shields.io/github/downloads/jbwong05/obs-pulseaudio-app-capture/total?cacheSeconds=3600)](https://github.com/jbwong05/obs-pulseaudio-app-capture/releases)
![CI Multiplatform Build](https://github.com/jbwong05/obs-pulseaudio-app-capture/actions/workflows/main.yml/badge.svg)
![Clang Format](https://github.com/jbwong05/obs-pulseaudio-app-capture/actions/workflows/clang-format.yml/badge.svg)

Similar to the [win-capture-audio](https://obsproject.com/forum/resources/win-capture-audio.1338/) plugin by [bozbez](https://obsproject.com/forum/members/bozbez.344203/) that allows for the capturing of audio from specific applications except for systems that use PulseAudio as their main sound server. Also influenced by the existing `linux-pulseaudio` plugin built into OBS as well as the [pactl](https://gitlab.freedesktop.org/pulseaudio/pulseaudio/-/blob/master/src/utils/pactl.c) utility built into PulseAudio <b>NOTE: This plugin is still experimental and under development. Bugs are expected</b>

## How It Works (Technical)
PulseAudio uses a client server model to manage sound. Unfortunately (at least from what I can tell) it does not allow recording directly from a specific application as clients and the corresponding sink-inputs do not have monitors that can be recorded from. The way to get around this is to load a new `combine-sink` module, set the `combine-sink` module's sink (output) to be the same sink as the application, and then move the application's sink to be the new `combine-sink` module. Because the `combine-sink` module has a monitor, we can read the audio data from the monitor while the audio is still being sent to desired location. <b>Note:</b> While not ideal, this is actually the same method that `Zoom` uses in order to capture and share desktop audio on Linux devices running a PulseAudio server.

## Usage
Simply add the source, select the application, and the audio should be recorded.

## Known Bugs and Limitations
* Each of these sources will end up loading its own `combine-sink` module. If you have a lot of these sources, you will end up with a lot of pulse modules being loaded. There may be a better way to reuse existing loaded modules instead creating new ones everytime, however there is something to be said for keeping each source in isolation, especially when different sources are on different scenes.
* The functionality of this plugin depends heavily on loading the `combine-sink` pulse module. If the module is not available or cannot be loaded for whatever reason, the plugin will not function.
* If OBS crashes before the plugin is able to destroy and clean itself up, lingering `combine-sink` modules will remain loaded afterwards. This may lead to unintended behavior when OBS is restarted.
* This plugin is still experimental. Lots of other bugs are expected.

## Dependencies
* libpulse

## Installation
Debain installer can be found in the [Releases](https://github.com/jbwong05/obs-pulseaudio-app-capture/releases) section. A tar file containing the actual plugin library and data files can also be found and extracted for other non-Debian based systems.

## Building from Source

### Linux
```
git clone https://github.com/jbwong05/obs-pulseaudio-app-capture.git
cd obs-pulseaudio-app-capture
mkdir build && cd build
# If you are on Ubuntu, add the `-DBUILD_UBUNTU=true` flag to your cmake command
cmake ..
# If dependencies are not on your path, you can manually specify their paths with the following:
cmake -DQTDIR=<path to Qt dir> -DLIBOBS_INCLUDE_DIR=<path to the libobs sub-folder in obs-studio's source code> -DLIBOBS_LIB=<path to libobs.so> -DOBS_FRONTEND_LIB=<path to libobs-frontend-api.so> ..
make
sudo make install
```