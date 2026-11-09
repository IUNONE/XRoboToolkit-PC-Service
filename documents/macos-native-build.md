# Native macOS tracking service and C SDK

This branch builds the existing PICO tracking service and C SDK on macOS using
native Qt, gRPC and Protobuf. It preserves the headset TCP protocol, discovery
broadcast format and public `PXREA*` callback ABI. No headset APK changes are
required for this host-side port.

The native build is selected automatically on macOS. Linux/Windows can select
the same path with `-DXRT_USE_SYSTEM_DEPENDENCIES=ON`; their original build and
packaging path remains the default. Native build dependencies are resolved by
CMake targets, not CPU-name-dependent lists of Linux/Windows binary files.

## Build on macOS

Install Xcode Command Line Tools first if they aren't already present. Then:

```sh
brew install cmake ninja qtbase qt5compat grpc protobuf
cmake -S RoboticsService -B build/macos -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/macos --parallel 4
ctest --test-dir build/macos --output-on-failure
```

Run commands from the repository root. If dependencies are in a custom prefix,
pass it via `-DCMAKE_PREFIX_PATH=/your/prefix`; do not point macOS at the bundled
Linux libraries. CMake uses the target architecture selected by its toolchain;
Apple Silicon and Intel need matching dependency architectures. A universal
binary is not created automatically.

The native path requires CMake >= 3.18, Qt >= 6.4 (Core, Network, Core5Compat), a matching Protobuf
compiler/runtime, gRPC with `grpc_cpp_plugin`, and C++17. Python 3 is used only by
the integration test (`-DBUILD_TESTING=OFF` disables that requirement). Protocol
sources are regenerated into the build directory from the existing Linux RPC
superset at `PXREAService/linux_x86/PXREAService.proto`, without changing field
numbers, message names or service methods. No generated sources are overwritten.

## Run with a PICO headset

In terminal 1:

```sh
./build/macos/bin/RoboticsServiceProcess
```

On PICO, use the existing XRoboToolkit application, enter this Mac's LAN IPv4
address for PC Service, and enable Controller tracking and Send. `WORKING` on
the headset is the first physical-connection check. In terminal 2:

```sh
./build/macos/bin/ConsoleDemo
```

The console example prints device events and raw tracking JSON received through
the native C SDK. It uses the same sample source as the existing Linux/Windows
console example. Exit the console and service with Ctrl+C when finished.

Default ports:

| Link | Default |
|---|---|
| PICO TCP connection | TCP 63901 |
| Discovery advertisement | UDP 29888 |
| Local SDK gRPC connection | 127.0.0.1:60061 |

Allow the service's local-network access when prompted by macOS. Automatic
discovery depends on the LAN permitting broadcasts; manual IP entry uses the
same TCP listener. Discovery now uses each interface's actual broadcast address
rather than assuming every network is /24.

## Configuration and runtime paths

The service reads `setting.ini` beside the executable by default. The native
build supplies `build/macos/bin/setting.ini`, and installation supplies the same
file in `bin/`. To keep custom settings outside the build tree:

```sh
XRT_SERVICE_CONFIG_FILE=/absolute/path/setting.ini ./build/macos/bin/RoboticsServiceProcess
```

The C SDK retains its `PXREASetting.ini` working-directory default, with an
explicit override for applications and tests:

```sh
XRT_SDK_CONFIG_FILE=/absolute/path/sdk.ini ./build/macos/bin/ConsoleDemo
```

```ini
# SDK configuration
[Client]
connectAddr=127.0.0.1
connectPort=60061
```

`XRT_RUNTIME_DIRECTORY` optionally relocates service logs/lock files and separates
its local single-instance socket namespace. Tests use a temporary directory and
unused ports, so they do not interfere with a normal service. In normal use,
leave it unset to retain single-instance behavior in the standard user data
location. Network interface discovery is shared across all supported platforms.

## Install layout

```sh
cmake --install build/macos --prefix "$PWD/install/macos"
./install/macos/bin/RoboticsServiceProcess
./install/macos/bin/ConsoleDemo
```

- `bin/`: service, optional console demo and default configuration.
- `lib/`: service libraries and `libPXREARobotSDK.dylib` on macOS (`.so` on Linux).
- `include/PXREARobotSDK.h`: public SDK header.

macOS libraries use `@rpath`/`@loader_path`; Linux uses `$ORIGIN`. The install tree
still depends on the native Qt/gRPC/Protobuf installation. This is a development
installation, not a signed, notarized, dependency-bundled application package.
`-DXRT_BUILD_SDK_DEMO=OFF` omits the console example.

## Verification and scope

The CTest integration test starts the actual service in isolation, loads the
actual native SDK using Python's standard-library `ctypes`, and verifies:

1. gRPC service connection and callback delivery.
2. Original PICO TCP serial handshake and tracking JSON delivery to the SDK.
3. SDK device-control JSON traveling back to the simulated headset over TCP.
4. Second-instance detection and SDK cleanup.

It does not require a headset or a Python gRPC package. Passing it validates the
host-side protocol path; physical PICO tracking, discovery on your LAN and headset
sleep/neck-worn behavior still require headset testing.

The separate `XRoboToolkit-PC-Service-Pybind` repository is **not modified** here.
Its published `xrobotoolkit_sdk` package still rejects macOS. To keep that Python
API, its build must accept this native SDK, link the `.dylib`, configure loader
paths and package matching dependencies. A successful build here alone does not
make `pip install xrobotoolkit-sdk` work on macOS. The C SDK and console example
can be used immediately for host-side testing.

Existing upstream packed-protocol copy warnings may appear with AppleClang; this
port does not rewrite packet serialization or add GUI/video-demo packaging.

## Local validation record

Validated on Apple Silicon with AppleClang 21, Qt 6.12.0, gRPC 1.85.1 and
Protobuf 36.2: Release build and the simulated PICO/C SDK integration test.
The macOS/Linux CI workflow is included but has not been run remotely by this
local change. Windows and physical-headset behavior are not claimed as tested.
