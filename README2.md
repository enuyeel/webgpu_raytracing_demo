a regular prompt approach

(...)

6. Install Google's Depot Tools. I didn't install Depot Tools the recommended way, instead I did this:

[O] i. Make sure Git is on the PATH (via the environment variable control panel).

[O] ii. Launch a VS2019 x64 Native Tools Command Prompt (found in the Windows menu; note to investigate: a regular Prompt might be enough, since Depot Tools knows how to find the compiler).

[O; NA] iii. Set-up Git following the Bootstrapping section in the Depot Tools page above.

(...)

[O] v. Clone Depot Tools:
	git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git

[O] vi. Add Depot Tools to the front of your PATH in the current Command Prompt:
	set PATH=C:\Users\Yune\Desktop\eel94nuy\repos\webgpu_raytracing_demo\depot_tools;%PATH%
	
[O] vii. Run gclient (which should download the CIPD client then show you some options).

[O] viii. Add win32file to Depot Tools' Python (and verify with where python that Depot Tools is the preferred python executable):
	python -m pip install pywin32
	
7. In the same VS2019 x64 Prompt above, with the same PATH, etc., clone Dawn following the Building Dawn instructions. We need to make these steps a little more Windows friendly:

	git clone https://dawn.googlesource.com/dawn dawn && cd dawn
	copy scripts\standalone.gclient .gclient
	set DEPOT_TOOLS_WIN_TOOLCHAIN=0
	gclient sync
	
8. Configure then build Dawn:

[https://gn.googlesource.com/gn/+/main/docs/quick_start.md]
Look up 'Setting up a build' section for the command 'gn'.

	gn gen out\Release (Make a build directory of name 'out\Release')
	
		"You must install Windows 10 SDK version 10.0.22621.0 including the "Debugging Tools for Windows" feature."
		[https://chromium.googlesource.com/chromium/src/+/master/docs/windows_build_instructions.md#Visual-Studio]
		Go to the very end of 'Setting up Windows' section for details.
		
		"You must have the version 10.0.22621.0 Windows 11 SDK installed. This can be installed separately or by checking the appropriate box in the Visual Studio Installer. The 10.0.22621.755 (Windows 11) SDK Debugging Tools must also be installed. This version of the Debugging tools is needed in order to support reading the large-page PDBs that Chrome uses to allow greater-than 4 GiB PDBs."
	
	gn args --list out\Release (Print the list of available arguments and their default values associated with the given build) 
	
	is_clang

		Set to true when compiling with the Clang compiler.
	
	visual_studio_version
	
		Version of Visual Studio pointed to by the visual_studio_path.
		Currently always "2015".
	
	is_debug

		Debug build. Enabling official builds automatically sets is_debug to false.
	
	enable_iterator_debugging

		When set, enables libc++ debug mode with iterator debugging.

		Iterator debugging is generally useful for catching bugs. But it can
		introduce extra locking to check the state of an iterator against the state
		of the current object. For iterator- and thread-heavy code, this can
		significantly slow execution - two orders of magnitude slowdown has been
		seen (crbug.com/903553) and iterator debugging also slows builds by making
		generation of snapshot_blob.bin take ~40-60 s longer. Therefore this
		defaults to off.

	symbol_level

		How many symbols to include in the build. This affects the performance of
		the build since the symbols are large and dealing with them is slow.
		  2 means regular build with symbols.
		  1 means minimal symbols, usually enough for backtraces only. Symbols with
		internal linkage (static functions or those in anonymous namespaces) may not
		appear when using this level.
		  0 means no symbols.
		  -1 means auto-set according to debug/release and platform.
		  
	dawn_use_angle

		TODO(dawn:1545): Re-enable dawn_use_angle on Android. In non-component
		builds, this is adding a dependency on ANGLE's libEGL.so and
		libGLESv2.so, apparently without regard for the use_static_angle=true
		GN variable. Chromium's linker on Android disallows production of more
		than one shared object per target (?).
		
	dawn_use_swiftshader

		Enables SwiftShader as the fallback adapter. Requires dawn_swiftshader_dir
		to be set to take effect.
		TODO(dawn:1536): Enable SwiftShader for Android.
		
	target_cpu
	
		(Internally set; try `gn help target_cpu`.)

			target_cpu: The desired cpu architecture for the build.

				This value should be used to indicate the desired architecture for the
				primary objects of the build. It will match the cpu architecture of the
				default toolchain, but not necessarily the current toolchain.

				In many cases, this is the same as "host_cpu", but in the case of
				cross-compiles, this can be set to something different. This value is
				different from "current_cpu" in that it does not change based on the current
				toolchain. When writing rules, "current_cpu" should be used rather than
				"target_cpu" most of the time.

				This value is not used internally by GN for any purpose, so it may be set to
				whatever value is needed for the build. GN defaults this value to the empty
				string ("") and the configuration files should set it to an appropriate value
				(e.g., setting it to the value of "host_cpu") if it is not overridden on the
				command line or in the args.gn file.

			Possible values
				- "x86"
				- "x64"
				- "arm"
				- "arm64"
				- "mipsel"
				- "mips64el"
				- "s390x"
				- "ppc64"
				- "riscv32"
				- "riscv64"
				- "e2k"
				- "loong64"
				
	# Set build arguments here. See `gn help buildargs`.
	is_clang=false
	visual_studio_version="2019"

	is_debug=true
	enable_iterator_debugging=true
	symbol_level=2

	dawn_use_angle=false
	dawn_use_swiftshader=false

	target_cpu="x64"