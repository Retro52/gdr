# OpenGL-3D-Drawer
This simple 3D renderer can load and display almost any 3D model using OpenGL graphics API.
![mainImage](https://github.com/Retro52/OpenGL-3D-Drawer/blob/main/res/img/DefaultImage.png?raw=true)

## Build
### Dependencies
The project depends on FreeType, Assimp, GLFW, GLEW, and OpenGL32 libraries. All of them are present in the vendors folder. All of the neccessarry dll`s are present in bin folder.

To build the project you will have to install Assimp and FreeType libraries (note that you have to put the FreeType library header in your default include folder, due to how the library is built) and run CMakeList.txt

The following header-only libraries were also used:
 - [inipp.h](https://github.com/mcmtroffaes/inipp): used under MIT License, Copyright (c) 2017-2020 Matthias C. M. Troffaes.
 - [json.hpp](https://github.com/nlohmann/json): used under MIT license, Copyright (c) 2013-2022 Niels Lohmann, v3.11.2.
 - [stb_image.h](https://github.com/nothings/stb/blob/master/stb_image.h): used under MIT License, Copyright (c) 2017 Sean Barrett, v2.27.

## Run
Note that the application needs 2 configuration files (config.ini and config.json by default). You can specify the .json config file path in the .ini file without recompiling, but not vice versa. You also might want to ensure that all of the relative paths in the .json and .ini files are correct before running.

## Features
- Simple materials
- Simple logging system
- Directional lights soft shadows
- Lighting supports normal and specular maps
- Lighting supports multiple light-sources lighting
- Textures of almost any format are supported as well
- The renderer supports loading models in a large number of formats
- Entity system implemented using entt library (inspired by TheCherno youtube series)
- ... and more

## License

MIT License.

