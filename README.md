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
  - Under 'Individual components', select 'C++ Clang Compiler for Windows' and 'MSBuild support for LLVM (clang-cl) toolset'
- Open the source folder in VS, select a configuration (`x64-Clang-Debug` for `-O0` or `x64-Clang-Release` for `-O3`) and click Build->Build All.
- An executable is created in `/out/build/x64-Clang-Release/`. You can run this directly from the command line, or use the play button in VS (in this case, see `.vs/launch.vs.json` for cmdline options).

### Troubleshooting
- See https://learn.microsoft.com/en-us/cpp/build/cmake-projects-in-visual-studio?view=msvc-170.
- If the `x64-Clang-` configs do not work, use the `x64-Debug` or `x64-Release` configs instead.
- You must be connected to the internet for the first build so CMake can fetch required dependencies.

## Building on Linux/Mac
Install CMake from your package manager (on Mac you can use [Homebrew](https://brew.sh/) to install CMake).  
Clone the repo then run the following in terminal:
```
cd <clone-location>
mkdir build && cd build
cmake ..
cmake --build .
```
Change `cmake ..` to ``cmake -DCMAKE_BUILD_TYPE=Debug ..`` to compile without optimizations.

### Troubleshooting
- You need a compiler with C++20 or C++23 support (g++ version 11 or higher).
- Connecting to the FPGA does not work in WSL out of the box. [This might help.](https://learn.microsoft.com/en-us/windows/wsl/networking#mirrored-mode-networking)

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
