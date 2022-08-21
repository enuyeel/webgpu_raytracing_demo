# git clone https://github.com/emscripten-core/emsdk.git
#
# https://emscripten.org/docs/getting_started/downloads.html#
#
# Run the following emsdk commands to get the latest tools from GitHub and set them as active:
#
# Fetch the latest version of the emsdk (not needed the first time you clone)
# git pull
#
# Download and install the latest SDK tools.
# emsdk install latest
#
# Make the "latest" SDK "active" for the current user. (writes .emscripten file)
# emsdk activate latest
#
# Activate PATH and other environment variables in the current terminal
# emsdk_env.bat
#
# Platform-specific notes
# Windows
# Install Python 3.6 or newer (older versions may not work due to a GitHub change with SSL).


# Build for Windows & Emscripten based on available example online.
# https://github.com/cwoffenden/hello-webgpu/blob/main/build-web.bat
# 
# CPP_FLAGS (Base)
# -std=c++11 -Wall -Wextra -Werror -Wno-nonportable-include-path -fno-exceptions -fno-rtti
# CPP_FLAGS (Debug)
# -g3 -D_DEBUG=1 -Wno-unused -Wno-unused-parameter 
# CPP_FLAGS (Release)
# -g0 -DNDEBUG=1 -flto
#
# EMS_FLAGS (Base)
# --output_eol linux -s ALLOW_MEMORY_GROWTH=0 -s ENVIRONMENT=web -s MINIMAL_RUNTIME=2 -s NO_EXIT_RUNTIME=1 -s NO_FILESYSTEM=1 -s STRICT=1 -s TEXTDECODER=2 -s USE_WEBGPU=1 -s WASM=1 --shell-file src/ems/shell.html
# EMS_FLAGS (Debug)
# -s ASSERTIONS=2 -s DEMANGLE_SUPPORT=1 -s SAFE_HEAP=1 -s STACK_OVERFLOW_CHECK=2
# EMS_FLAGS (Release)
# -s ABORTING_MALLOC=0 -s ASSERTIONS=0 -s DISABLE_EXCEPTION_CATCHING=1 -s EVAL_CTORS=1 -s SUPPORT_ERRNO=0 --closure 1
#
# OPT_FLAGS (Base)
#
# OPT_FLAGS (Debug)
# -O0
# OPT_FLAGS (Release)
# -O3 
#
# emcc %CPP_FLAGS% %OPT_FLAGS% %EMS_FLAGS% %INC% %SRC% -o %OUT%.html

# emcc -std=c++11 -Wall -Wextra -Werror -Wno-nonportable-include-path -fno-exceptions -fno-rtti -g3 -D_DEBUG=1 -Wno-unused --output_eol linux -s ALLOW_MEMORY_GROWTH=0 -s ENVIRONMENT=web -s MINIMAL_RUNTIME=2 -s NO_EXIT_RUNTIME=1 -s NO_FILESYSTEM=1 -s STRICT=1 -s TEXTDECODER=2 -s USE_WEBGPU=1 -s WASM=1 --shell-file src/ems/shell.html -s ASSERTIONS=2 -s DEMANGLE_SUPPORT=1 -s SAFE_HEAP=1 -s STACK_OVERFLOW_CHECK=2 main.cpp -o output/index.html

# em++ -std=c++11 -Wall -Wextra -Werror -Wno-nonportable-include-path -fno-exceptions -fno-rtti -g3 -D_DEBUG=1 -Wno-unused -Wno-unused-parameter --output_eol linux -s ALLOW_MEMORY_GROWTH=0 -s ENVIRONMENT=web -s MINIMAL_RUNTIME=2 -s NO_EXIT_RUNTIME=1 -s NO_FILESYSTEM=1 -s STRICT=1 -s TEXTDECODER=2 -s USE_WEBGPU=1 -s WASM=1 -s ASSERTIONS=1 -s DEMANGLE_SUPPORT=1 -s SAFE_HEAP=1 -s STACK_OVERFLOW_CHECK=2 main.cpp -o output/index.html -s LLD_REPORT_UNDEFINED

# ENVIRONMENT=web 
# Building with '-sENVIRONMENT=web' will emit code that only runs on the Web, 
# and does not include support code for Node.js and other environments.

# ALLOW_MEMORY_GROWTH=0
# Building with -sALLOW_MEMORY_GROWTH allows the total amount of memory used to change 
# depending on the demands of the application.

# em++ -std=c++11 -Wall -Wextra -Werror -Wno-nonportable-include-path -fno-exceptions -fno-rtti -g3 -D_DEBUG=1 -Wno-unused -Wno-unused-parameter -s ENVIRONMENT=web -s ALLOW_MEMORY_GROWTH=0 main.cpp -s USE_WEBGPU=1 -o output/index.html --shell-file shell_minimal.html

# python -m http.server -d output
# http://localhost:8000
