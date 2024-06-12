# host
Command-line tool to send rendering workloads to FPGA.

- Parses 3D scene file formats (.obj, .mtl, .scene)
- Triangulates geometries and creates AABB tree to accelerate spatial queries
- Serializes scene + tree and transmits it to the FPGA via Ethernet
- Saves image returned from FPGA.

Clone with `git clone --recursive https://github.com/capstone-team-2023844-fpga-raytracing/host`.  

## Building on Windows
- Install Visual Studio Community 2022 with the following options:
  - Select the "Linux Development with C++" and "Desktop Development with C++" workloads in the installer.
  - If you prefer Clang over Microsoft's C++ compiler, then also select 'C++ Clang Compiler for Windows' and 'MSBuild support for LLVM (clang-cl) toolset' under 'Individual components'.
- Open the source folder in VS, select desired configuration (`x64-Debug` or `x64-Release`) and click Build->Build All.
- An executable is created in `/out/build/`. You can use this from the command line.

### Troubleshooting
- See [here](https://learn.microsoft.com/en-us/cpp/build/cmake-projects-in-visual-studio?view=msvc-170) for any issues with CMake.

## Building on Linux/Mac
Install CMake (On MacOS, you can use [Homebrew](https://brew.sh/) to install CMake). `cd` into the source directory and run the following:
```
mkdir build && cd build
cmake ..
cmake --build .
```
Change `cmake ..` to ``cmake -DCMAKE_BUILD_TYPE=Debug ..`` if you want a debuggable executable.

### Troubleshooting
- You need a compiler with C++20 or C++23 support (gcc version 11 or higher).
- WSL Linux does not work out of the box. [This might help.](https://learn.microsoft.com/en-us/windows/wsl/networking#mirrored-mode-networking)

## Running
`./rthost --help` brings up a list of options:
```
FPGA raytracer host.
Usage:
  rthost [OPTION...]

  -h, --help                Show usage.
  -i, --in <file>           Scene to render (.scene or binary file).
  -o, --out <file>          Output (.bmp, .png, or binary file).
      --dest <host>,<port>  FPGA network destination. (default:
                            de1soclinux,50000)
      --max-bv <uint>       Max bounding volumes. Must be a power of 2.
                            (default: 128)
      --serfmt <dup|nodup>  Serialization format. (default: dup)
  -b, --tobin               Convert scene to .bin.
  -c, --tohdr               Convert scene to C header.
      --bv-report           Report on BV efficiency (might take a few
                            seconds).
  -v, --verbose             Verbose mode.
```
Example: `./rthost --in tests/jeep.scene --out jeep.png`.
