# kmap
Knowledge Map

## Prerequisites

### Packages
```bash
sudo apt install build-essential
sudo apt install libgdk-pixbuf2.0-dev
sudo apt install libgtk3.0-cil-dev
sudo apt install libnss3-dev
sudo apt install libtool
sudo apt install tcl
```

### Install emsdk
Ensure Emscripten's emcc is accessible via your $PATH.

```bash
git clone https://github.com/emscripten-core/emsdk.git
git pull
./emsdk install 3.1.27
./emsdk activate 3.1.27
source ./emsdk_env.sh
```

> 3.1.27 is latest known working version, but other versions may work as well.

### Clone Kmap

```bash
git clone https://github.com/moralismercatus/kmap.git
cd kmap
npm install
```

## Building - Linux
>Note: Replace all instances of 'Debug' with 'Release' (case-respective) for release build.
```bash
cd ..
mkdir kmap-Debug
cd kmap-Debug
CC=emcc CXX=emcc cmake -DCMAKE_BUILD_TYPE=Debug ../kmap
make -j
cd ../kmap 
```

### Visual Studio Code Set Up

1. Install CMake Extension
1. Open "Settings": `>Preferences: Open Settings (JSON)`
    >Add following to settings.json:
    ```json
    "cmake.configureOnOpen": false,
    "cmake.buildDirectory": "${workspaceFolder}/../kmap-${buildType}",
    "cmake.generator": "Unix Makefiles",
    "cmake.emscriptenSearchDirs": [ "/<emsdk_repo>" ],
    "cmake.environment": {
        "PATH": "${env:PATH}:/<emsdk_repo>/upstream/emscripten"
    },
    ```
1. Open kmap folder in vscode
1. Set "kit" to "Emscription": `>CMake: Select a Kit`

## Building - Windows
Use WSL + Linux instructions.

## Troubleshooting

### Resolve Boost Build Error
Modify boost/project-config.jam
```jam
#using msvc ;
using emscripten : : C:/<path_to_emsdk>/emscripten/<version>/emcc ;
```
### Rename all libraries to simplified .bc
Ensure all built libraries (in lib/) match those named as the link libraries in src/CMakeLists.txt, and `make` again.
>Note: Replace all instances of 'debug' with 'release' (case-respective) for release build.

## Running
```bash
cd kmap
npm run debug
# Or...
npm run release
```

## Deploying

### Dependencies
```bash
sudo apt-get install wine1.6
npm install electron-packager -g
```

### Account for Proxy
If behind a proxy, I recommend setting HTTP_PROXY, HTTPS_PROXY, and:
```bash
export GLOBAL_AGENT_ENVIRONMENT_VARIABLE_NAMESPACE=
```
Which will force electron-packager (or its dependencies) to use HTTP/HTTPS_PROXY.

### Packaging
```bash
npm run package
```
The `package.json` `package` command may need altering (e.g., for different targets).
