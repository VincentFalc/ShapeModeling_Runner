### Provided Libraries
The geometry processing library [libigl](https://github.com/libigl/libigl), which includes implementations of many of the algorithms presented in class. The libigl library includes a set of tutorials, an introduction which can be found [online](http://libigl.github.io/libigl/) or directly in the folder 'libigl/tutorial/tutorial.html'. 

### Building Each Assignment
Create a directory called build in the directory, e.g.:
```
mkdir build
```
Use CMake to generate the Makefiles needed for compilation inside the build/ directory:
```
cd build; cmake -DCMAKE_BUILD_TYPE=Release ../
```
Compile and run the executable, e.g.:
```
make
```