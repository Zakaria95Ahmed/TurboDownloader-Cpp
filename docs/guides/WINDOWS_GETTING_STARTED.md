# OpenIDM - Windows Getting Started Guide

This guide walks you through setting up a complete development environment for OpenIDM on a **fresh Windows 10/11 machine**. Follow each step carefully.

---

## Table of Contents

1. [Prerequisites Overview](#1-prerequisites-overview)
2. [Install Visual Studio Build Tools](#2-install-visual-studio-build-tools)
3. [Install Qt 6](#3-install-qt-6)
4. [Install CMake](#4-install-cmake)
5. [Install Git](#5-install-git)
6. [Install vcpkg (Optional - for libcurl)](#6-install-vcpkg-optional---for-libcurl)
7. [Clone and Build OpenIDM](#7-clone-and-build-openidm)
8. [Run OpenIDM](#8-run-openidm)
9. [Troubleshooting](#9-troubleshooting)

---

## 1. Prerequisites Overview

| Software | Version | Purpose |
|----------|---------|---------|
| **Visual Studio Build Tools** | 2022 (v17.x) | MSVC compiler (C++20 support) |
| **Qt** | 6.6.x or newer | GUI framework + Qt Quick/QML |
| **CMake** | 3.21 or newer | Build system |
| **Git** | Latest | Version control |
| **vcpkg** | Latest | Package manager (optional, for libcurl) |

**Disk Space Required:** ~15 GB (Qt is large)
**Time Required:** ~1-2 hours (mostly download time)

---

## 2. Install Visual Studio Build Tools

OpenIDM requires a C++20 compliant compiler. We recommend **MSVC** from Visual Studio 2022.

### Option A: Full Visual Studio 2022 (Recommended for beginners)

1. Download **Visual Studio 2022 Community** (free):
   - https://visualstudio.microsoft.com/downloads/

2. Run the installer and select:
   - **Workload:** "Desktop development with C++"
   
3. In the individual components, ensure these are selected:
   - MSVC v143 - VS 2022 C++ x64/x86 build tools (Latest)
   - Windows 11 SDK (10.0.22621.0 or newer)
   - C++ CMake tools for Windows

4. Click **Install** and wait for completion.

### Option B: Build Tools Only (Smaller download)

1. Download **Build Tools for Visual Studio 2022**:
   - https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022

2. Run the installer and select:
   - **Workload:** "Desktop development with C++"

3. Click **Install**.

### Verify Installation

Open **"Developer Command Prompt for VS 2022"** and run:

```cmd
cl
```

You should see output like:
```
Microsoft (R) C/C++ Optimizing Compiler Version 19.xx.xxxxx for x64
```

---

## 3. Install Qt 6

Qt provides the GUI framework for OpenIDM.

### Step-by-Step Installation

1. **Create a Qt Account** (required for installer):
   - Go to https://login.qt.io/register
   - Create a free account

2. **Download Qt Online Installer**:
   - https://www.qt.io/download-qt-installer
   - Download the Windows installer

3. **Run the Installer**:
   - Sign in with your Qt account
   - Select **"Custom installation"**

4. **Select Components** (IMPORTANT):
   
   Under **Qt 6.6.x** (or latest 6.x):
   - ✅ `MSVC 2019 64-bit` (works with VS 2022)
   - ✅ `Qt Quick 3D` (optional but recommended)
   - ✅ `Qt Shader Tools`
   - ✅ `Additional Libraries` → `Qt Multimedia`
   
   Under **Developer and Designer Tools**:
   - ✅ `Qt Creator` (optional IDE)
   - ✅ `CMake` (can use this instead of separate install)
   - ✅ `Ninja`

5. **Installation Path**:
   - Default: `C:\Qt`
   - Remember this path for later!

6. Click **Install** (this will take 30-60 minutes).

### Set Environment Variables

After installation, add Qt to your PATH:

1. Open **System Properties** → **Environment Variables**
2. Under **User variables**, find `Path` and click **Edit**
3. Add these entries (adjust version number if different):
   ```
   C:\Qt\6.6.2\msvc2019_64\bin
   C:\Qt\Tools\CMake_64\bin
   C:\Qt\Tools\Ninja
   ```

4. Add a new **User variable**:
   - Name: `Qt6_DIR`
   - Value: `C:\Qt\6.6.2\msvc2019_64`

### Verify Installation

Open a **new** Command Prompt and run:

```cmd
qmake --version
```

Expected output:
```
QMake version 3.1
Using Qt version 6.6.x in C:/Qt/6.6.2/msvc2019_64/lib
```

---

## 4. Install CMake

If you didn't install CMake with Qt:

1. Download CMake from https://cmake.org/download/
   - Get the **Windows x64 Installer** (`.msi` file)

2. Run the installer
   - Select **"Add CMake to the system PATH for all users"**

3. Verify installation:
   ```cmd
   cmake --version
   ```
   Expected: `cmake version 3.28.x` (or newer)

---

## 5. Install Git

1. Download Git from https://git-scm.com/download/win

2. Run the installer with default options
   - When asked about PATH, select **"Git from the command line and also from 3rd-party software"**

3. Verify installation:
   ```cmd
   git --version
   ```

---

## 6. Install vcpkg (Optional - for libcurl)

vcpkg makes it easy to install C++ libraries like libcurl.

### Install vcpkg

```cmd
cd C:\
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
```

### Install libcurl

```cmd
.\vcpkg install curl:x64-windows
.\vcpkg integrate install
```

### Set Environment Variable

Add a new **User variable**:
- Name: `CMAKE_TOOLCHAIN_FILE`
- Value: `C:\vcpkg\scripts\buildsystems\vcpkg.cmake`

---

## 7. Clone and Build OpenIDM

### Clone the Repository

Open **Developer Command Prompt for VS 2022** and run:

```cmd
cd C:\Projects
git clone https://github.com/openidm/openidm.git
cd openidm
```

### Configure with CMake

```cmd
mkdir build
cd build

cmake .. -G "Ninja" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_PREFIX_PATH="C:/Qt/6.6.2/msvc2019_64" ^
    -DCMAKE_TOOLCHAIN_FILE="C:/vcpkg/scripts/buildsystems/vcpkg.cmake"
```

**Note:** Adjust paths if your Qt installation is different.

### Alternative: Using Visual Studio Generator

```cmd
cmake .. -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_PREFIX_PATH="C:/Qt/6.6.2/msvc2019_64"
```

This creates a `.sln` file you can open in Visual Studio.

### Build the Project

With Ninja:
```cmd
cmake --build . --config Release
```

Or with Visual Studio:
```cmd
cmake --build . --config Release --target OpenIDM
```

### Build Output

After successful build, you'll find:
```
build/bin/Release/OpenIDM.exe
```

---

## 8. Run OpenIDM

### First Run

```cmd
cd build\bin\Release
OpenIDM.exe
```

### Deploy Qt Dependencies

To run OpenIDM outside the build directory, you need to deploy Qt DLLs:

```cmd
cd build\bin\Release
C:\Qt\6.6.2\msvc2019_64\bin\windeployqt.exe --qmldir ..\..\..\qml OpenIDM.exe
```

This copies all required Qt DLLs next to the executable.

### Create Installer (Optional)

```cmd
cd build
cpack -G NSIS
```

This creates an installer in the `build` directory.

---

## 9. Troubleshooting

### "Qt6 not found" Error

**Solution:** Ensure `CMAKE_PREFIX_PATH` points to your Qt installation:

```cmd
cmake .. -DCMAKE_PREFIX_PATH="C:/Qt/6.6.2/msvc2019_64"
```

### "CURL not found" Error

**Solution 1:** Install via vcpkg (see Section 6)

**Solution 2:** Use Qt Network instead:
```cmd
cmake .. -DOPENIDM_USE_SYSTEM_CURL=OFF
```

### "Cannot find -lssl" or SSL Errors

**Solution:** Install OpenSSL or use vcpkg:
```cmd
vcpkg install openssl:x64-windows
```

### Application crashes on startup

**Possible causes:**
1. Missing Qt DLLs → Run `windeployqt`
2. Missing Visual C++ Runtime → Install from Microsoft
3. OpenGL issues → Update graphics drivers

### Build is slow

**Solution:** Use Ninja instead of MSBuild:
```cmd
cmake .. -G "Ninja" ...
```

### "Access denied" errors

**Solution:** Run Command Prompt as Administrator, or check antivirus settings.

---

## Development Tips

### Using Qt Creator

1. Open Qt Creator
2. File → Open File or Project
3. Select `CMakeLists.txt` from the OpenIDM root
4. Configure the kit (select MSVC 2019 64-bit)
5. Build and run from Qt Creator

### Hot Reload QML

During development, you can reload QML without recompiling:
1. Set environment variable: `QML_DISABLE_DISK_CACHE=1`
2. Modify QML files
3. Restart the application

### Debug Build

```cmd
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build .
```

### Enable Address Sanitizer

```cmd
cmake .. -DOPENIDM_ENABLE_SANITIZERS=ON -DCMAKE_BUILD_TYPE=Debug
```

---

## Next Steps

- Read the [Architecture Documentation](../design/ARCHITECTURE.md)
- Check out the [API Reference](../api/README.md)
- Join our [Discord community](#) for help

---

## Version History

| Date | Version | Changes |
|------|---------|---------|
| 2024-01-15 | 1.0 | Initial guide |

---

**Happy downloading! 🚀**
