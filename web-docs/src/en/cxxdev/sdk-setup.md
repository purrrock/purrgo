# SDK Project Configuration

Glyphix is distributed to device manufacturers as precompiled libraries. This document describes how to configure the build environment in an SDK project to develop Native Modules, Native Widgets, or platform adaptation code on top of it.

::: tip Prerequisites
Before starting, please ensure the following are installed:
- CMake 3.14 or higher
- A C++ compiler supporting C++14 (GCC, Clang, or MSVC)
- Glyphix Meta-Object Compiler `meta` (matching the SDK; see below for acquisition details)
- Currently, precompiled libraries are only provided for Linux host environments; Windows and macOS precompiled libraries are not yet available.
:::

## SDK Package Structure

The extracted SDK contains the following directories:

```
glyphix-sdk/
├── libs/
│   └── <target-triple>/       # Precompiled libraries categorized by target triple
│       ├── include/           # Glyphix header files (gx_*.h)
│       └── lib/               # Static libraries (libglyphix-core.a, etc.)
├── cmake/
│   ├── GlyphixSDK.cmake       # Main SDK configuration script
│   ├── meta.cmake             # Meta-object compiler integration (glyphix_add_meta_objects)
│   ├── cross-compile.cmake    # Cross-compilation toolchain loading
│   ├── arch/                  # Compilation parameters for various architectures (mips-linux-gnu, cortex-m33, etc.)
│   └── toolchain/             # CMake toolchain files for various toolchains
├── wrapper/                   # Platform adaptation layer (host implementations for network, filesystem, etc.)
├── app/                       # Example application entry points (emulator, async, etc.)
└── vendor/                    # Third-party dependency libraries
```

### `libs/<target-triple>/`

The SDK's pre-built libraries use the **target triple** as the directory name to distinguish between platforms, for example:

- `x86_64-linux-gnu/`: 64-bit Linux host development/emulation
- `mips-linux-gnu/`: MIPS Linux embedded target
- `cortex_m55-none-gnu/`: Cortex-M55 bare-metal target

The `include/` directory contains all Glyphix public header files, all prefixed with `gx_`. The `lib/` directory contains static libraries; the core libraries include:

| Library File | Description |
|:---|:---|
| `libglyphix-core.a` | Core framework (object system, widget tree, events, etc.) |
| `libglyphix-widgets.a` | Built-in widget library |
| `libglyphix-reactive.a` | Reactive framework (JavaScript bridge layer) |
| `libglyphix-platform.a` | Platform Abstraction Layer interface |
| `libglyphix-service.a` | System service layer |

## Preparation

### Configuring the Meta-Object Compiler

The `meta` meta-object compiler is distributed separately from the SDK as an independent archive. After extraction, you will get two directories, `bin/` and `lib/`. **Both must be kept in the same directory**, as the `meta` executable depends on the runtime libraries in `lib/`.

On Linux and macOS, it is recommended to extract to `/usr/local` so that `meta` is automatically included in your `PATH`:

::: code-tabs#bash

@tab Linux

```bash
sudo tar -xJf glyphix-meta-vX.X-linux-x86_64.tar.xz -C /usr/local
```

@tab macOS

```bash
sudo tar -xJf glyphix-meta-vX.X-darwin-arm64.tar.xz -C /usr/local
```

:::

You can also extract it to any directory and then add the `bin/` directory to your `PATH`. Once completed, verify that it is working with the following command:

```bash
meta --version
```

If you do not wish to modify the `PATH`, you can explicitly specify the full path to the executable during CMake configuration using `-DGX_META=/path/to/bin/meta`.

## Configure CMakeLists.txt

### Minimal Configuration

::: tip
The CMake configuration introduced in this section is similar to the standard example template of a Glyphix SDK project; you can refer directly to the SDK source files.
:::

The following is a minimal runnable `CMakeLists.txt`, demonstrating the standard configuration skeleton of a project:

```cmake
cmake_minimum_required(VERSION 3.14)

# Must be loaded before project() so that the toolchain is in place when
# project() probes the compiler
include(cmake/cross-compile.cmake)

project(my_glyphix_app)
set(CMAKE_CXX_STANDARD 14)

# Load Glyphix SDK (sets header paths, link directories, glyphix::sdk target)
include(cmake/GlyphixSDK.cmake)

add_subdirectory(vendor)  # Third-party dependencies (if any)
add_subdirectory(src)     # Your source code
```

Create a target and link the SDK in `src/CMakeLists.txt`:

```cmake
add_executable(my_app
  main.cpp
  my_module.cpp
  my_widget.cpp
)

# Link Glyphix SDK
target_link_libraries(my_app PRIVATE glyphix::sdk)

# Generate metadata for header files containing GX_OBJECT
glyphix_add_meta_objects(my_app
  my_module.h
  my_widget.h
)
```

### Registering Meta Objects (`glyphix_add_meta_objects`)

As mentioned in the [Object System](./object-system) documentation, any class declaring `GX_OBJECT` must be registered with the build system, and the meta-object compiler will generate `*_meta.cpp` files for it. `glyphix_add_meta_objects()` is the CMake function to complete this step:

```cmake
glyphix_add_meta_objects(<target> [header1.h header2.h ...])
```

It accepts a target name and a set of **header file** paths as arguments. For each header file, the `meta` tool generates a corresponding `*_meta.cpp` in the `meta/` subdirectory of the build directory and automatically adds it to the target's source file list for compilation.

**Example:** Suppose the project has the following structure:

```
src/
├── CMakeLists.txt
├── main.cpp
├── sensors/
│   ├── step_counter.h       # Contains GX_OBJECT
│   └── step_counter.cpp
└── widgets/
    ├── activity_ring.h      # Contains GX_OBJECT
    └── activity_ring.cpp
```

Corresponding `CMakeLists.txt`:

```cmake
include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/meta.cmake)

add_executable(my_app
  main.cpp
  sensors/step_counter.cpp
  widgets/activity_ring.cpp
)
target_link_libraries(my_app PRIVATE glyphix::sdk)

glyphix_add_meta_objects(my_app
  sensors/step_counter.h
  widgets/activity_ring.h
)
```

::: tip Pass only header files, not .cpp files
`glyphix_add_meta_objects()` only needs the **header files** (`.h`) containing the `GX_OBJECT` declaration. The meta-object compiler reads the macro declarations in the header files to generate code and does not need to parse implementation files. Conversely, classes containing `GX_OBJECT` cannot be defined in `.cpp` files.
:::

::: warning Do not omit registration
If a class declares `GX_OBJECT` but is not registered via `glyphix_add_meta_objects()`, it will result in a **linker error** (symbols like `staticMetaObject` not found). Whenever a new header file containing `GX_OBJECT` is added, remember to update the `CMakeLists.txt` accordingly.
:::

### `glyphix::sdk` Interface Target

`GlyphixSDK.cmake` defines the `glyphix::sdk` CMake interface library target, which encapsulates all linking dependencies of the SDK. In your `CMakeLists.txt`, you only need to link this single target:

```cmake
target_link_libraries(my_target PRIVATE glyphix::sdk)
```

Internally, it is equivalent to:

```cmake
# Pseudo-code — actually managed automatically by GlyphixSDK.cmake
target_include_directories(... ${GLYPHIX_INCLUDE_DIRS} wrapper/include)
target_link_libraries(... -Wl,--start-group ${glyphix-*.a} glyphix-wrapper -Wl,--end-group)
target_link_libraries(... m pthread dl)  # UNIX system libraries
```

Wrapping static libraries with `-Wl,--start-group ... -Wl,--end-group` is to resolve circular dependency linking issues between static libraries on embedded platforms.

::: tip Linking Order Issues
If the project has its own static libraries (e.g., `add_library(my_module STATIC ...)`), they should be linked **inside** `glyphix::sdk`; otherwise, the scope of `--start-group` will not cover them, which may cause linking errors. The method is to append your static library path after the `GLYPHIX_LIBS` variable in `GlyphixSDK.cmake` is defined and before the `glyphix-sdk` target is created, or directly have the final executable link both `my_module` and `glyphix::sdk` and manually specify `--start-group`.
:::

## Host Build

Host builds are used to run Glyphix example programs on the development machine, allowing for quick verification of widget and module logic without connecting to hardware.

```bash
mkdir build && cd build
cmake -G Ninja ..
cmake --build .
```

The `app/` directory of the SDK contains multiple examples, each subdirectory corresponding to an independent executable target. For example:

| Subdirectory | Build Artifact | Description |
|:---|:---|:---|
| `app/emulator/` | `demo` | Emulator with graphical interface, depends on MiniFB window backend |
| `app/async/` | `async-demo` | Asynchronous service example without graphical interface, demonstrating Native Modules and asynchronous callbacks |

`GlyphixSDK.cmake` automatically detects the host machine's compiler triple (via `gcc -dumpmachine` or `clang -dumpmachine`) and uses it as a key to find the corresponding pre-compiled libraries in the `libs/` directory. For example, on an x86_64 Linux development machine, it will automatically resolve to `libs/x86_64-linux-gnu/`.

If the automatically detected triple does not match the actual library directory, you can specify it manually:

```bash
cmake -G Ninja -DTARGET_TRIPLE=x86_64-linux-gnu ..
```

If you only need to build a specific example, you can specify the target name:

```bash
cmake --build . --target demo
cmake --build . --target async-demo
```

## CMake Cross-Compilation

For embedded targets, you need to specify the target architecture via the `-DARCH` parameter. The SDK provides the following pre-configured architectures:

| `-DARCH` Value | Target Platform | Toolchain Prefix |
|:---:|:---:|:---:|
| `mips-linux-gnu` | MIPS Linux | `mips-linux-gnu-` |
| `cortex_m33-gnu` | ARM Cortex-M33 (GNU) | `arm-none-eabi-` |
| `cortex_m7-gnu` | ARM Cortex-M7 (GNU) | `arm-none-eabi-` |

### MIPS Linux Example

```bash
export MIPS_TOOLCHAIN_DIR="/opt/mips-gcc720-glibc229"

mkdir build-mips && cd build-mips
cmake -G Ninja .. \
  -DARCH=mips-linux-gnu \
  -DMIPS_TOOLCHAIN_DIR="$MIPS_TOOLCHAIN_DIR" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

If the toolchain is already in the `PATH` (i.e., `mips-linux-gnu-gcc` can be called directly), `-DMIPS_TOOLCHAIN_DIR` can be omitted, and CMake will locate it automatically.

### ARM Cortex-M Example

```bash
mkdir build-cm33 && cd build-cm33
cmake -G Ninja .. \
  -DARCH=cortex_m33-gnu \
  -DARM_TOOLCHAIN_DIR="/opt/arm-none-eabi-gcc" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

In cross-compilation, `GlyphixSDK.cmake` does not attempt to auto-detect the triple—architecture files (e.g., `cmake/arch/cortex_m33-gnu.cmake`) will directly set `TARGET_TRIPLE`, pointing to the correct library directory.

### Supported Target Architectures

The SDK only provides pre-built libraries for the architectures listed in the table above. If your target platform is not among them, you need to contact Glyphix to obtain the SDK package for the corresponding architecture, rather than adding support yourself based on the existing SDK.

## Other Build Systems

The SDK uses CMake as the primary build system, and Glyphix also provides support for other build systems to partner manufacturers. This usually involves just importing pre-built SDK libraries and header files, and adding porting layer source files.

### Project Limitations

This solution is suitable for projects that only require standard SDK features. Once custom widgets, Native Modules, or other capabilities are needed, the `meta` meta-object compiler must be introduced to generate the necessary binding code. Currently, CMake is the only supported build system.

There are several available alternatives:
1. Use the SDK CMake project to build custom code, then link the generated library to your main project.
2. Use the SDK CMake project to build custom code, then include the generated source files (`*_meta.cpp`) in your main project.
3. Directly call the `meta` tool in your build system to generate binding code.

Among these, the Glyphix SDK itself is built using method 1. However, it is not suitable for the internal development workflows of downstream vendors, as it requires maintaining a separate project outside the main firmware project and linking the generated binary library back to the main project. This can cause serious version management issues.

Method 3 is also usually not ideal, as vendors typically do not want to introduce an external tool into the main project's build system.

### Recommended Solution

Therefore, method 2 is recommended. This approach involves copying source code; although it requires manual operation, it is easy to audit and integrate into existing build workflows. You can build the custom code in the SDK's CMake project to generate `*_meta.cpp` files, then copy these files to your main project and compile them within the main project's build system.

Another limitation of this method is that the custom source files must be able to build successfully within the SDK project environment. Specifically, this requires that it can be built independently of the main project, including:
- Include paths and preprocessor definitions must be set correctly; the header files of custom components must not include header files specific to the main project.
- It is also preferable for the custom component's cpp files to compile successfully. Although this does not affect the generation of `*_meta.cpp` files, it facilitates rapid iteration and debugging in the host environment.

::: tip
For most [custom widgets](widget.md), this is not an issue. For [Native Modules](native-module.md), it might be more troublesome. Note that header files declaring `GX_OBJECT` should not include header files specific to the main project.
:::
