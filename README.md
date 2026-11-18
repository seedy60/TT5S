# TeamTalk 5

Repository for TeamTalk 5 development.

## Overview
* [Download TeamTalk 5 SDK](#download-teamtalk-5-sdk)
  * Get the client and server binaries for building your own software without
    having to build all the TeamTalk dependencies yourself.
* [Building from Source](#building-from-source)
  * Build the TeamTalk 5 libraries, clients and servers from this repository
* [TeamTalk 5 Libraries](#teamtalk-5-libraries)
  * The libraries (APIs) that can be used for developing your client and server
* [TeamTalk 5 Clients](#teamtalk-5-clients)
  * Example projects that show how to build your own TeamTalk client
* [TeamTalk 5 Servers](#teamtalk-5-servers)
  * Example projects that show how to build your own TeamTalk server

## Building from Source

TeamTalk 5 binaries are built with [CMake](http://www.cmake.org). All
dependencies (OpenSSL, ACE, OPUS, Speex, SpeexDSP, FFmpeg, libvpx, OGG,
PortAudio, TinyXML2, miniupnpc and ZLib) are built from source as CMake
*ExternalProjects*, so the first configuration requires network access.

Every platform needs Git, CMake 3.15 or later and a C++17 compiler. The
sections below install the platform specific tools and build the binaries:

* [Windows](#building-for-windows)
* [Ubuntu 24.04 and 26.04](#building-for-ubuntu-2404-and-2604)
* [Raspberry Pi OS](#building-for-raspberry-pi-os)
* [macOS](#building-for-macos)
* [Android](#building-for-android)
* [iOS](#building-for-ios)
* [Docker](#building-with-docker)
* [Build options](#build-options)

### Building for Windows

[Visual Studio 2022](https://visualstudio.microsoft.com) or later with the
*Desktop development with C++* workload and the Windows 11 SDK
(10.0.26100.0) is required, as well as:

* [Strawberry Perl](https://strawberryperl.com/) (or ActivePerl) with
  `perl.exe` in %PATH% - OpenSSL and ACE use Perl
* [MSYS2](https://www.msys2.org/) in `C:/MSYS64` with the `base-devel` and
  `pkg-config` packages installed - FFmpeg and libvpx need `make`
* [NASM](https://www.nasm.us) - the libvpx assembler, downloaded
  automatically if not found

In an *x64 Native Tools Command Prompt for VS*, from the repository root:

```
cmake -S . -B build-win64 -A x64
cmake --build build-win64 --config Release
```

Use `-A Win32` for 32-bit binaries and a separate build directory. To build
only the console server use e.g.:

```
cmake --build build-win64 --config Release --target tt5srv
```

The binaries are written to `Server\tt5srv.exe` and
`Library\TeamTalk_DLL\TeamTalk5.dll`, and a `TeamTalk5SDK.sln` solution file
is generated in the build directory. The WebRTC dependency is checked out
into `C:\webrtc` during the first build.

### Building for Ubuntu 24.04 and 26.04

```
sudo make -C Build depend-ubuntu24    # or depend-ubuntu26
make -C Build ubuntu24                # or ubuntu26
```

The `depend-ubuntu*` target installs the compiler, CMake, Qt 6, OpenJDK,
Doxygen, NASM and the audio libraries with `apt`. The build is configured in
`Build/build-release-ubuntu24` and produces `Server/tt5srv` and
`Library/TeamTalk_DLL/libTeamTalk5.so`. `JAVA_HOME` may have to be set so
CMake can find the Java SDK.

### Building for Raspberry Pi OS

```
sudo make -C Build depend-raspios12
make -C Build rasp
```

The `rasp` target builds with `FEATURE_WEBRTC=OFF`.

### Building for macOS

Xcode and [Homebrew](https://brew.sh) are required:

```
xcode-select --install
make -C Build depend-mac
make -C Build mac
```

This produces universal *arm64* and *x86_64* binaries in
`Build/build-release-mac`, i.e. `Server/tt5srv` and
`Library/TeamTalk_DLL/libTeamTalk5.dylib`.

### Building for Android

Android binaries are built on Ubuntu 24.04:

```
sudo make -C Build depend-ubuntu24-android
export ANDROID_NDK_HOME=~/android-ndk-r30
make -C Build android-all
```

[Android NDK r30](https://developer.android.com/ndk) must be unzipped and
`ANDROID_NDK_HOME` must point to the unzipped directory. `android-all` builds
the static `libTeamTalk5.a` and `libTeamTalk5Pro.a` for *armeabi-v7a*,
*arm64-v8a*, *x86* and *x86_64*. Single architectures are built with
`android-armeabi-v7a`, `android-arm64-v8a`, `android-x86` and `android-x64`.
The Java library is built separately, producing `TeamTalk5.jar` and
`TeamTalk5Pro.jar` in `Library/TeamTalkJNI/libs`:

```
make -C Build teamtalk-jar
```

### Building for iOS

On macOS, with the dependencies from `depend-mac` installed:

```
make -C Build lipo
```

This merges the *arm64* device architecture and the *x86_64* simulator
architecture into `Library/TeamTalk_DLL/libTeamTalk5.a`. To build simulator
architectures only:

```
make -C Build lipo-simulator
```

This produces `libTeamTalk5-simulator.a` for the *arm64* and *x86_64*
simulator architectures. Device and simulator *arm64* cannot be combined in
a single archive, which is why the two targets are separate. The first iOS
build downloads WebRTC into `$HOME/webrtc/iOS`. See
[README-iOS-XCFramework.md](Client/iTeamTalk/README-iOS-XCFramework.md) for
building the XCFrameworks used by the Swift package.

### Building with Docker

The [docker-compose](Build/Docker/docker-compose.yml) file provides
containers with the toolchain already installed, so no dependencies have to
be installed on the host:

```
cd Build/Docker
docker compose run --rm ubuntu24 make -C /TeamTalk5/Build ubuntu24
docker compose run --rm ubuntu26 make -C /TeamTalk5/Build ubuntu26
docker compose run --rm android make -C /TeamTalk5/Build android-all
```

The *android* container ships Android NDK r30 with `ANDROID_NDK_HOME` set.

### Build options

Parts of the repository are skipped with `BUILD_TEAMTALK_*` options, e.g. to
build only the libraries:

```
cmake -S . -B build -DBUILD_TEAMTALK_CLIENTS=OFF -DBUILD_TEAMTALK_SERVERS=OFF
```

`TOOLCHAIN_*` options choose whether a dependency is built by CMake or taken
from the host (recommended for OpenSSL on Linux), and `FEATURE_*` options
enable or disable codecs and platform features. All options are documented
in [Library/TeamTalkLib](Library/TeamTalkLib/README.md#toolchain-toggles-for-teamtalk-build-targets).

## Download TeamTalk 5 SDK

To build the TeamTalk client or server projects you must first
download the [TeamTalk 5 SDK](http://www.bearware.dk/?page_id=393) to
obtain the client and server binaries.

You can also try out the latest beta releases of the TeamTalk
5 SDK [here](http://www.bearware.dk/test/teamtalksdk). Some of the
TeamTalk 5 SDK beta releases are only available for certain platforms,
so if your platform is not listed then try a previously released beta.

## TeamTalk 5 Libraries
Projects wrapping the client and server binaries in the TeamTalk SDK.
* [**TeamTalkLib**](Library/TeamTalkLib)
  * Source code for building TeamTalk 5 DLL and server binaries
  * Read [License.txt](https://github.com/BearWare/TeamTalk5/blob/master/LICENSE.txt) for terms of use
  * Build using [CMake](http://www.cmake.org)
    * Follow build instructions in [Library/TeamTalkLib](Library/TeamTalkLib)
  * A [Makefile](Build/Makefile) is available in [Build](Build) for
      quickly building for macOS, Ubuntu, Android, iOS and Raspberry Pi OS (Debian) e.g.:
      * `make -C Build depend-mac mac` requires [Homebrew](https://brew.sh)
      * `sudo make -C Build depend-ubuntu26` followed by `make -C Build ubuntu26`
  * A [docker-compose](Build/Docker/docker-compose.yml) file is available in [Build/Docker](Build/Docker)
      for quickly building for Linux distributions e.g.:
      * `docker compose run --rm ubuntu26 make -C /TeamTalk5/Build ubuntu26`
      * `docker compose run --rm android make -C /TeamTalk5/Build android-all`
* [**TeamTalk_DLL**](Library/TeamTalk_DLL) (dependency: **TeamTalkLib**)
  * TeamTalk 5 C-API DLL
  * C-API header files for TeamTalk 5 DLL
    * API [Documentation](https://bearware.dk/?page_id=415)
  * Build using TeamTalkLib or [download TeamTalk 5 SDK](https://bearware.dk/?page_id=419)
* [**TeamTalk.NET**](Library/TeamTalk.NET) (dependency: **TeamTalk_DLL**)
  * TeamTalk 5 .NET DLL wrapper for C-API TeamTalk 5 DLL (**TeamTalk_DLL**)
    * API [Documentation](https://bearware.dk/?page_id=415)
  * Requires DLL file from **TeamTalk_DLL** project, either 32-bit or 64-bit
* [**TeamTalkJNI**](Library/TeamTalkJNI) (dependency: **TeamTalk_DLL**)
  * TeamTalk 5 JNI project with Java wrapper classes
    * API [Documentation](https://bearware.dk/?page_id=415)
* [**TeamTalkPy**](Library/TeamTalkPy) (dependency: **TeamTalk_DLL**)
  * Python wrappers for TeamTalk DLL
* [**teamtalk_rust**](Library/teamtalk_rust) (dependency: **TeamTalk_DLL**)
  * Rust library for TeamTalk DLL generated by [bindgen](https://github.com/rust-lang/rust-bindgen)

## TeamTalk 5 Clients
Projects containing client applications which use the TeamTalk 5 client DLL.
* [**qtTeamTalk**](Client/qtTeamTalk) (dependency: **TeamTalk_DLL**)
  * TeamTalk 5 client application written in C++ and based on [Qt](http://www.qt.io)
  * Requires **TeamTalk_DLL** project for DLL dependency
* [**TeamTalkClassic**](Client/TeamTalkClassic) (dependency: **TeamTalk_DLL**)
  * TeamTalk 5 accessible client application written in C++ and based on MFC
    * Works well with screen-readers
  * Requires **TeamTalk_DLL** project for DLL dependency
* [**TeamTalkApp.NET**](Client/TeamTalkApp.NET) (dependency: **TeamTalk.NET**)
  * TeamTalk 5 .NET client application written in C#
  * Requires **TeamTalk.NET** project for DLL dependency
* [**iTeamTalk**](Client/iTeamTalk) (dependency: **TeamTalk_DLL**)
  * TeamTalk 5 iOS client application written in Swift
  * Requires **TeamTalk_DLL** project for bridging header
  * Open project in [Xcode](https://developer.apple.com/xcode/)
* [**TeamTalkAndroid**](Client/TeamTalkAndroid) (dependency: **TeamTalkJNI**)
  * TeamTalk 5 Android client application written in Java
  * Open project in [Android Studio](https://developer.android.com/studio/intro/index.html)
  * Copy the TeamTalk 5 Java library to *TeamTalkAndroid/libs* directory:
    * TeamTalk5.jar
      * Located in `Library/TeamTalkJNI/libs` (Standard Edition)
    * TeamTalk5Pro.jar
      * Located in `Library/TeamTalkJNI/libs` (Professional Edition)
  * Copy the TeamTalk 5 JNI shared library to *TeamTalkAndroid/src/main/jniLibs/[armeabi-v7a|arm64-v8a|x86|x86_64]*
    * libTeamTalk5-jni.so
        * Located in `Library/TeamTalkJNI/libs/[armeabi-v7a|arm64-v8a|x86|x86_64]` (Standard Edition)
    * libTeamTalk5Pro-jni.so
        * Located in `Library/TeamTalkJNI/libs/[armeabi-v7a|arm64-v8a|x86|x86_64]` (Professional Edition)
* [**ttserverlogpy**](Client/ttserverlogpy) (dependency: **TeamTalkPy**
  * Python example client
* [**ttserverlog_rust**](Client/ttserverlog_rust) (dependency: **teamtalk_rust**)
  * Rust example client
* [**ttphpadmin**](Client/ttphpadmin)
  * Console PHP-script for administrating a TeamTalk 5 server.
  * Run `php ttphpadmin.php`

## TeamTalk 5 Servers
Sample applications for writing a TeamTalk 5 server are located in the Examples folder. Building a TeamTalk 5 server requires TeamTalk 5 Professional Edition.
* [**TeamTalkServer**](Server/TeamTalkServer) (dependency: **TeamTalk_DLL**)
  * TeamTalk 5 server application written in C++
  * Requires **TeamTalk_DLL** project for DLL dependency
* [**TeamTalkServer.NET**](Server/TeamTalkServer.NET) (dependency: **TeamTalk.NET**)
  * TeamTalk 5 server application written in C#
  * Requires **TeamTalk_DLL** and **TeamTalk.NET** projects for DLL dependencies
* [**jTeamTalkServer**](Server/jTeamTalkServer) (dependency: **TeamTalkJNI**)
  * TeamTalk 5 server application written in Java
  * Requires **TeamTalk_DLL** and **TeamTalkJNI** for DLL dependencies
