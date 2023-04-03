# WebGPU/Emscripten & Dawn Raytracing Demo

![](512x512.png "")
	
[**Run it on your browser! Latest Chrome Canary is required with '#enable-unsafe-webgpu' flag enabled.**](https://yuneismyname.com/demos/raytracing/)

---

### Build with MSVC

#### Release
> TODO

#### Debug
> TODO

---

### Build with Emscripten

#### Release
> em++ -std=c++17 -Wall -Wextra -fno-rtti -Wnonportable-include-path -Wno-unused -Wno-unused-parameter -Os -s ASSERTIONS=1 -s ENVIRONMENT=web -s ALLOW_MEMORY_GROWTH=1 -s LLD_REPORT_UNDEFINED -s USE_WEBGPU=1 -o index.html --embed-file ./shader@/ --shell-file src/emscripten/shell_minimal.html src/main.cpp src/emscripten/window.cpp -I./inc

#### Debug
> TODO

#### Testing On Browser Locally
> \>python -m http.server

<details>
<summary><h4>MISC</h4></summary>

#### Compiler & Linker Flags

##### References
- [https://emscripten.org/docs/tools_reference/emcc.html](https://emscripten.org/docs/tools_reference/emcc.html)
- [https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html#Warning-Options](https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html#Warning-Options)
- [https://gcc.gnu.org/onlinedocs/gcc/C_002b_002b-Dialect-Options.html#C_002b_002b-Dialect-Options](https://gcc.gnu.org/onlinedocs/gcc/C_002b_002b-Dialect-Options.html#C_002b_002b-Dialect-Options)

"Most clang options will work, as will gcc options (...) To see the full list of Clang options supported on the version of Clang used by Emscripten, run clang --help."
e.g.) clang --help | findstr /c:"no-rtti"

- `-std=c++17`
TODO

- `-Wall`
TODO

- `-Wextra`
TODO

- `-fno-rtti`
"Disable generation of information about every class with virtual functions for use by the C++ run-time type identification features (dynamic_cast and typeid). If you don’t use those parts of the language, you can save some space by using this flag."

 - `-Os`
The compiler will use a variety of optimization techniques to reduce the size of the resulting binary file. This can include things like removing unnecessary code, inlining functions, and using smaller data types.

- `-s ASSERTIONS=1`
Emscripten will add assertions to the compiled code, which can help catch programming errors and bugs at runtime. However, assertions can also have some performance overhead, as they add extra instructions to the compiled code. Therefore, it is generally recommended to disable assertions in production builds to maximize performance.

- `-s ENVIRONMENT=web`
This particular flag specifies the target environment for the compiled code, setting it to "web". Emscripten will apply various optimizations to the compiled code that are specific to web environments, such as providing better integration with JavaScript and the DOM, using asynchronous I/O, and optimizing for smaller code size and faster load times.

- `-s ALLOW_MEMORY_GROWTH=1`
Emscripten will disable dynamic memory growth, which can be useful in cases where the amount of memory needed by the compiled code is known in advance. However, if the compiled code attempts to allocate more memory than was allocated at startup, it will fail and the program may crash.

- `-s LLD_REPORT_UNDEFINED`
LLD linker (a linker designed to be used with LLVM-based compilers) will report any undefined symbols encountered during the linking process. It can be useful for identifying missing dependencies or debugging linking issues.

- `-s USE_WEBGPU=1`
TODO

"Each of these specific warning options also has a negative form beginning ‘-Wno-’ to turn off warnings; (...)."

- `-Wnonportable-include-path` 
If the path is non-portable, meaning it is specific to a certain platform or environment, the Clang compiler may issue a warning indicating that the path is non-portable.

- `-Wunused-parameter`
"Warn whenever a function parameter is unused aside from its declaration. To suppress this warning use the unused attribute (see Variable Attributes)."

</details>