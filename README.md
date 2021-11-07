```shell

mkdir build

cd build

cmake -G "Visual Studio 16 2019" -A x64 ..

#windows
cmake --build . --config Release
 
#linux
cmake --build . -DCMAKE_BUILD_TYPE=Release
```