# host

Responsible for:
- Reading and writing 3D scene files
- Creating the bounding-volume tree to acclerate spatial queries
- Serializing the scene into a compact binary format
- Transmitting the scene to FPGA, receiving and saving returned image


## Windows
Install Visual Studio Community 2022.
Tick the "Linux Development with C++" and "Desktop Development with C++" workloads in the installer.
You can select Release (full optimizations, can't use a debugger) or Debug (no optimizations + debugger allowed) when running.

## Mac/Linux
Install CMake from your package manager, 'cd' to the source directory and run:
```
mkdir build
cd build
cmake ..
cmake --build .
```
To compile in Release/Debug, run ``cmake -DCMAKE_BUILD_TYPE=Release ..`` or ``cmake -DCMAKE_BUILD_TYPE=Debug ..`` in the build folder, then re-run ``cmake --build .``.
