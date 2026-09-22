# Building

## Dependencies

| Needs | Why |
|:------|:----|
| A C++20 compiler | Coroutines, `std::format`, concepts, `std::jthread`. GCC 13+ or Clang 17+. |
| Boost (headers and `Boost::system`) | Asio, for all the networking. |
| CMake 3.16 or newer | The build. |
| GoogleTest | Tests only, and downloaded automatically if not installed. |

On Debian or Ubuntu:

```bash
sudo apt-get install cmake g++ libboost-dev libboost-system-dev libgtest-dev
```

On Fedora:

```bash
sudo dnf install cmake gcc-c++ boost-devel gtest-devel
```

## Build

```bash
cmake -S . -B build
cmake --build build -j
```

That leaves `srnp-master` and the demo programs in `build/bin`, and
`libsrnp.so` in `build/lib`.

## Install

```bash
sudo cmake --install build
```

Installs the headers, the library, `srnp-master`, and a CMake package
configuration. If the install prefix's library directory is not already on the
loader's path:

```bash
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
```

`sudo cmake --build build --target uninstall` removes what was installed.

## Using srnp from another project

After installing:

```cmake
find_package(srnp REQUIRED)
target_link_libraries(my_component PRIVATE srnp)
```

Or without installing, straight against the build tree:

```bash
g++ -std=c++20 -I include my_component.cpp -L build/lib -lsrnp -o my_component
```

## Build options

| Option | Default | What it does |
|:-------|:--------|:-------------|
| `CMAKE_BUILD_TYPE` | empty | Set to `Debug` or `Release` as usual. |
| `SRNP_SANITIZE` | empty | Builds with sanitizers, e.g. `-DSRNP_SANITIZE=address,undefined` or `-DSRNP_SANITIZE=thread`. See [Testing](../dev/testing.md). |
