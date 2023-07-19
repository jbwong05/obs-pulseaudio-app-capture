# obs-pulseaudio-app-capture

[![latest-tag](https://badgen.net/github/release/jbwong05/obs-pulseaudio-app-capture)](https://github.com/jbwong05/obs-pulseaudio-app-capture)
[![Downloads](https://img.shields.io/github/downloads/jbwong05/obs-pulseaudio-app-capture/total?cacheSeconds=3600)](https://github.com/jbwong05/obs-pulseaudio-app-capture/releases)
![Plugin Build](https://github.com/jbwong05/obs-pulseaudio-app-capture/actions/workflows/main.yml/badge.svg)

Similar to the [win-capture-audio](https://obsproject.com/forum/resources/win-capture-audio.1338/) plugin by [bozbez](https://obsproject.com/forum/members/bozbez.344203/) that allows for the capturing of audio from specific applications except for systems that use PulseAudio as their main sound server. Also influenced by the existing `linux-pulseaudio` plugin built into OBS as well as the [pactl](https://gitlab.freedesktop.org/pulseaudio/pulseaudio/-/blob/master/src/utils/pactl.c) utility built into PulseAudio 

## NOTE: This plugin is still experimental. Bugs are expected
## Anoter Note: This plugin is not under very active development. It has been updated to support OBS 29 but with very if any testing. If you wish to contribute with development of this plugin, feel free to fork the repo

## How It Works (Technical) (v0.2.0-alpha)
While upgrading the plugin to OBS 29, I decided to take another look if `libpulse` supported any other ways of capturing application audio without using the `combine-sink` module. `v0.1.0` used the `combine-sink` module dynamically resulting a lot of bugs and janky behavior. Instead of using a `combine-sink` module to man in the middle the audio so it can be recorded before it makes it to the sink, `v0.2.0-alpha` attaches itself to the monitor of the sink of which the application is sending audio to. Because there is no longer a man in the middle, there is no more added latency as well as random audio routing issues when the `combine-sink` module would misbehave. `libpulse` allows for only capturing audio from a monitor from a specific sink-input (in this case an application) if desired making this entire approach possible in much less code.

## How It Works (Technical) (v0.1.0)
PulseAudio uses a client server model to manage sound. Unfortunately (at least from what I can tell) it does not allow recording directly from a specific application as clients and the corresponding sink-inputs do not have monitors that can be recorded from. The way to get around this is to load a new `combine-sink` module, set the `combine-sink` module's sink (output) to be the same sink as the application, and then move the application's sink to be the new `combine-sink` module. Because the `combine-sink` module has a monitor, we can read the audio data from the monitor while the audio is still being sent to desired location. <b>Note:</b> While not ideal, this is actually the same method that `Zoom` uses in order to capture and share desktop audio on Linux devices running a PulseAudio server.

## Usage
Simply add the source, select the application, and the audio should be recorded.

## Dependencies
* libpulse0

## Installation
Debain installer can be found in the [Releases](https://github.com/jbwong05/obs-pulseaudio-app-capture/releases) section. A tar file containing the actual plugin library and data files can also be found and extracted for other non-Debian based systems.

## Building from Source

### Linux
```
git clone https://github.com/jbwong05/obs-pulseaudio-app-capture.git
cd obs-pulseaudio-app-capture
./.github/scripts/build-linux.sh
```
