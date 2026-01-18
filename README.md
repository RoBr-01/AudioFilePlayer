# AudioFilePlayer (enhanced)

Based on the AudioFilePlayer by matkatmusic:
[AudioFilePlayer](https://github.com/matkatmusic/AudioFilePlayer)

Extended to now support arbitrary channel counts, up to a maximum of 16.

## Requirements

- Cmake
- Ninja
- LLDB

This template loosely assumes you are using VSCode, but you can set up your own debugging if preferred.

## Set-up

Clone the project using:

```sh
git clone --recurse-submodules <repo link>
```

This *should* immediately set-up the submodules as well.

## Building

This project uses **CMake** for building.
In the root of the project run the following commands in a terminal window.

### Configure (Set Up the Build)

```sh
cmake --preset <configure preset name>
```

The available configure presets are:

- debug
- Release

### Build (Compile the Project)

After configuring, build using the corresponding preset:

```sh
cmake --build --preset <build preset name>
```

The available build presets corresponding to your configure preset are:

- build-debug (if you used the debug preset)
- build-release (if you used the release preset)

The final files will be in:

```
cwd/build/build_preset/artefacts/<build preset name>
```

There you will find the AU, Standalone and VST3 builds.

The AudioPluginHost is handy for debugging, though optional, it is located at:

```sh
cwd/lib/JUCE/extras/AudioPluginHost/Builds
```

There you can pick the platform you are building for, and build the *debug* version of the host.