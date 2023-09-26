# host

Implements 3D model file reading/writing etc.

As a test, the program reads a sphere.obj model file into lists of vertices, faces, normals etc. It then uses these lists to write sphere_copy.obj.
If the lists were correctly read from sphere.obj, sphere_copy.obj should look identical to sphere.obj. 

You can verify this with 3D Viewer app (on Microsoft Store) or with https://www.creators3d.com/online-viewer on Linux. 
On 3D viewer, click the "Stats and Shading" button on the upper right of the screen to see the triangles. On creators3d, Turn on "shaded wireframe" to see the triangles.


## Windows
Install Visual Studio Community 2022.
Tick the "Linux Development with C++" and "Desktop Development with C++" workloads in the installer.

You can run the program by pressing the play button at the top. You can select Release (full optimizations, can't use a debugger), or Debug (no optimizations + debugger allowed).

For the test to run successfully, copy sphere.obj to the out/build/x64-Debug and out/build/x64-Release folders. In the same folder, the program will output sphere_copy.obj. 

## Mac/Linux
Install CMake from your package manager, 'cd' to the source directory and run:
```
mkdir build
cd build
cmake ..
```
Copy sphere.obj to the 'build' folder before running.
