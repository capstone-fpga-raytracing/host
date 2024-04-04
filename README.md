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
- See https://learn.microsoft.com/en-us/cpp/build/cmake-projects-in-visual-studio?view=msvc-170 if you have any issues.
- If the `x64-Clang-` configs do not work, use the `x64-Debug` or `x64-Release` configs instead (these use MSVC).
- You must be connected to the internet for the first build so CMake can fetch required dependencies.

## Building on Linux/Mac/WSL
Install CMake from your package manager (on Mac you can use [Homebrew](https://brew.sh/) to install CMake).  
Clone the repo then run the following in terminal:
```
cd <clone-location>
mkdir build && cd build
cmake ..
cmake --build .
```
Change `cmake ..` to ``cmake -DCMAKE_BUILD_TYPE=Debug ..`` to compile without optimizations.


## Running
`./rthost --help` brings up a list of options:
```
FPGA raytracer host.
Usage:
  rthost [OPTION...]

  -h, --help                    Show usage.
  -i, --in <file>               Input file (.scene or .bin).
  -o, --out <file>              Output file (.bmp, .png, or .bin).
      --rt [=<host>,<port>(=de1soclinux,50000)]
                                Raytrace scene on FPGA. Faster if scene is
                                in binary format.
      --max-bv <uint>           Max bounding volumes. Must be a power of 2.
                                (default: 128)
      --bv-report               Report efficiency of bounding volumes
                                (takes a few seconds).
      --ser-fmt <dup|nodup>     Serialization format. (default: dup)
  -b, --tobin                   Convert scene to binary format.
  -c, --tohdr                   Convert scene to C header.
  -v, --verbose                 Verbose mode.
```
Example: `./rthost.exe --in ..\..\..\tests\jeep.scene --out jeep.png --rt=de1soclinux,50000 --verbose`.  
Note that there is no space after --rt.
