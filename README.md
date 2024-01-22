# host

Responsible for:
- Reading and writing 3D scene files
- Creating the bounding-volume tree to acclerate spatial queries
- Serializing the scene into a compact binary format
- Transmitting the scene to FPGA, receiving and saving returned image


## Windows
Install Visual Studio Community 2022 with the following options:
- Tick the "Linux Development with C++" and "Desktop Development with C++" workloads in the installer.
- Under 'Individual components', tick 'C++ Clang Compiler for Windows' and 'MSBuild support for LLVM (clang-cl) toolset'

On opening the source folder in VS, you will have the option to select either the x64-Clang-Release (full optimizations, no debugger) or x64-Clang-Debug (no optimizations + debugger) configurations.
Click the play button dropdown and select host.exe.

To change command line arguments modify the `.vs/launch.vs.json` file in the source folder.

## Mac/Linux
Install CMake from your package manager, cd to the source directory and run:
```
mkdir build && cd build
cmake ..
cmake --build .
```
To compile in Release or Debug, run ``cmake -DCMAKE_BUILD_TYPE=Release ..`` or ``cmake -DCMAKE_BUILD_TYPE=Debug ..`` in the build folder, then re-run ``cmake --build .``

Once built, you can run `./host --help` to get a list of command line options.

## Profiling
On Windows, you can use Visual Studio's superb Performance Profiler. 

On Linux:
- Install valgrind and kcachegrind.
- Run `valgrind --tool=callgrind ./host -i ../tests/jeep.scene --tobin jeep.bin` (example).
- Use kcachegrind to open the produced callgrind.out.\<pid\> file.

On Ubuntu WSL, use [QCacheGrind](https://sourceforge.net/projects/qcachegrindwin/) instead as kcachegrind does not seem to work.


