# WebGPU/Emscripten & Dawn Raytracing Demo

![](512x512.png "")

A simple raytracing demo built with Dawn WebGPU C/C++ headers for Windows and Web. It's following Peter Shirley's ["Ray Tracing in One Weekend"](https://raytracing.github.io/) series, and is written (mostly) in WGSL compute shader.

[**Run it on your browser! Latest Chrome Canary is required with '#enable-unsafe-webgpu' flag enabled.**](https://yuneismyname.com/demos/raytracing/)

<details>
<summary><h3>TODOs</h3></summary>

The current method to obtain a device in an Emscripten build involves using the 'wgpuInstanceRequestAdapter' and 'wgpuAdapterRequestDevice' functions, which internally call JavaScript functions. For further details, please refer to this [link](https://github.com/kainino0x/webgpu-cross-platform-demo/blob/8e1a5883f2d29a3030e813c0ccfaddea4f6398b5/main.cpp#L51). It appears that these functions are asynchronous in nature, and therefore the remaining initialization steps (such as creating a swap chain and initializing pipelines) should be performed within the callback function.

Another way to obtain a device in an Emscripten build is by using the function 'emscripten_webgpu_get_device()'. However, it appears that manually setting 'Module["preinitializedWebGPUDevice"]' in either JavaScript or HTML is required before calling this function. Several existing projects, notably @cwoffenden's ["Hello, Triangle"](https://github.com/cwoffenden/hello-webgpu/blob/6ada98bea21ad7283fb3a88b91d94b28f87ea190/src/ems/glue.cpp#L35) and @ocornut's [Dear ImGui](https://github.com/ocornut/imgui/blob/e8206db829f7c5d9a07985a2e2a8de6769cac64d/examples/example_emscripten_wgpu/web/index.html#L66), have demonstrated this approach. For further information, please refer to @mewmew-tea's [article](https://zenn.dev/kd_gamegikenblg/articles/a5a8effe43bf3c#%E3%81%8A%E3%81%BE%E3%81%91%EF%BC%92%EF%BC%9Adevice%E5%8F%96%E5%BE%97%E3%81%AE%E3%82%A2%E3%83%97%E3%83%AD%E3%83%BC%E3%83%81).

The current implementation is a bit messy, as I added Emscripten build-specific code into the main function as a last resort to overcome asynchronous issues. However, I plan to clean up the codebase for modularity and refine the code further, particularly if I decide to continue following up on Peter Shirley's later volumes.

</details>

---

### References
- [https://github.com/cwoffenden/hello-webgpu](https://github.com/cwoffenden/hello-webgpu)
- [https://github.com/kainino0x/webgpu-cross-platform-demo](https://github.com/kainino0x/webgpu-cross-platform-demo)
- [https://zenn.dev/kd_gamegikenblg/articles/a5a8effe43bf3c](https://zenn.dev/kd_gamegikenblg/articles/a5a8effe43bf3c)

---

### Build with MSVC
You can try running the included Visual Studio 2019 project for the Windows build. Please follow this [link](https://github.com/cwoffenden/hello-webgpu/tree/6ada98bea21ad7283fb3a88b91d94b28f87ea190/lib#building-windows-dawn-and-optionally-angle) for instructions on building Dawn on your Windows machine. @cwoffenden has done an excellent job compiling the necessary steps to follow. The prebuilt Dawn lib that I have uploaded in this repository is the result of following his instructions.

#### Release
It appears that Dawn has three official builds: 'Debug', 'Release', and 'Official', listed in order from least to most performant. I have attempted to put together either the 'Release' or 'Official' builds, but I encountered runtime errors related to null dereferencing when iterating over the list of available adapters. I can confirm that the Windows build currently works for at least the 'Debug' build.

#### Debug
Open the 'webgpu_raytracing_demo.sln', and run 'Debug' configuration under 'x64' platform.

---

### Build with Emscripten
TODO

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

---