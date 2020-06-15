# Coldgaze
PBR Graphics rendering engine with RTX support and Vulkan API backend.

![Rifle](https://github.com/ShpakovNikita/Coldgaze/blob/master/images/RenderedRifle.png)
Rifle model is not mine, [here](https://sketchfab.com/3d-models/rainier-ak-3d-57aef8cdf42046a39f1ad9b428756213) is the link on the original model.

# Features
- [x] GLTF2 format support
- [x] Basic PBR support
- [x] RTX path tracing
- [x] Camera lenses
- [x] Indirect light path tracing
- [ ] Enviroment light path tracing
- [ ] MoltenVK Macos support
- [ ] Refactor all dirty stuff in shaders

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
