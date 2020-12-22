# kmap
Knowledge Map

## Prerequisites
```bash
git clone https://github.com/moralismercatus/kmap.git
cd kmap
npm install
npm run postinstall
```
Ensure Emscripten's emcc is accessible via your $PATH.

## Building - Linux
>Note: Replace all instances of 'debug' with 'release' (case-respective) for release build.
```bash
cd ..
mkdir kmap-debug
cd kmap-debug
CC=emcc CXX=emcc cmake -DCMAKE_BUILD_TYPE=Debug ../kmap
make -j
cd ../kmap 
```

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
