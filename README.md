# Coldgaze
PBR Graphics rendering engine with RTX support and Vulkan API backend.

# Features
- [x] Basic PBR support
- [x] RTX path tracing
- [x] Camera lenses
- [x] Indirect light path tracing
- [ ] Enviroment light path tracing

# How to build?
Below, you can find the build instructions. Don't forget to pull data from lfs and init submodules before build!
```
$ git submodule update --init --recursive
$ git lfs install
$ git lfs pull
```

#### Windows (Using VS generator)
```
$ CMake -G "Visual Studio 16 2019"
```
After that, run generated solution.
