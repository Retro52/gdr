# Gpu Driven Renderer

![preview](.github/assets/preview.png "Bistro scene (by Amazon Lumberyard")

C++20/Vulkan GPU-driven renderer focused on modern real-time rendering architecture.
Implements GPU-side visibility culling, Hi-Z occlusion culling, indirect draw generation, bindless-style resource access, 
DDS textures support, and meshlet (cluster) rendering experiments (meshlets occlusion culling, cone culling, etc.).

## Building

```cmake
git clone --recursive https://github.com/Retro52/gdr
cd gdr

cmake -S . -B build
cmake --build build --target gdr
```

## Compatible GPUs:

Mesh shading is not required to run the program. However, the following Vulkan capabilities are required by default:

* 8-bit integer storage buffer access (`vk12::storageBuffer8BitAccess`)
* 16-bit storage and uniform buffer types (`vk11::storageBuffer16BitAccess` and `vk11::uniformAndStorageBuffer16BitAccess`)
* Indirect drawing (`vk::multiDrawIndirect`, `vk12::drawIndirectCount`, and `vk11::shaderDrawParameters`)
* Dynamic rendering
* Min/max sampler reduction mode
* Bindless textures: partially bound descriptors, variable descriptor counts, and non-uniform indexing
* Scalar block layout
* Synchronization2
* Anisotropic filtering

Any NVIDIA Turing, AMD RDNA 2, or newer GPU should run the program without issues. 
Older generations, such as AMD RDNA 1 and NVIDIA Pascal, should also support the required features, 
but they are currently untested due to lack of access to that hardware.

## License

MIT License. Portions of the fiber platform code (`src/job/fibers/platform/posix/`) are derived from [Google's Marl](https://github.com/google/marl) and are licensed under Apache-2.0. See [THIRD_PARTY_NOTICES](THIRD_PARTY_NOTICES) for details.
