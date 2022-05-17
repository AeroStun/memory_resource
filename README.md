# Portable™ `<memory_resource>`

The `<memory_resource>` C++ standard library header from C++17 is not supported by LLVM libc++ (nor AppleClang libc++).
This project provides an implementation for libc++ directly derived from libstdc++, thus making it possible to write portable code using `<memory_resource>` or use libraries which depend on it (e.g. Standalone Boost.JSON).

This project is intended to be consumed via CMake FetchContent.
Then simply link against `memory_resource::memory_resource`, and you're all set.  
The library will only be compiled and used when using libc++, and should work portably across platforms supported by LLVM libc++.
