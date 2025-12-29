# OpenIDM - Windows Build Guide

This guide will walk you through setting up a complete build environment for OpenIDM on a **fresh Windows 10/11 machine**.

---

## Required Software

| Software | Version | Purpose |
|----------|---------|---------|
| **Visual Studio 2022** | 17.8+ | C++ Compiler (MSVC) |
| **Qt** | 6.6.x or 6.7.x | UI Framework & Qt Quick |
| **CMake** | 3.21+ | Build System |
| **Git** | Latest | Version Control |
| **libcurl** | 8.x | Networking (via vcpkg) |
| **vcpkg** | Latest | Package Manager |

---

## Step 1: Install Visual Studio 2022

1. Download **Visual Studio 2022 Community** (free) from:
   ```
   https://visualstudio.microsoft.com/downloads/
   ```

2. Run the installer and select these workloads:
   - ✅ **Desktop development with C++**

3. Under "Individual components", ensure these are selected:
   - ✅ MSVC v143 - VS 2022 C++ x64/x86 build tools
   - ✅ Windows 10/11 SDK (latest)
   - ✅ C++ CMake tools for Windows

4. Click **Install** and wait for completion (~10-20 minutes)

---

## Step 2: Install Qt 6

### Option A: Qt Online Installer (Recommended)

1. Download the Qt Online Installer from:
   ```
   https://www.qt.io/download-qt-installer
   ```

2. Run `qt-unified-windows-x64-online.exe`

3. Sign in or create a free Qt Account

4. In the installer, select:
   - **Qt 6.6.x** or **Qt 6.7.x** (latest LTS recommended)
   
5. Under your chosen Qt version, select:
   - ✅ MSVC 2019 64-bit (works with VS 2022)
   - ✅ Qt Quick
   - ✅ Qt Quick Controls
   - ✅ Qt Network
   - ✅ Qt SQL

6. Under "Developer and Designer Tools":
   - ✅ Qt Creator (optional but recommended)
   - ✅ CMake
   - ✅ Ninja

7. Click **Install** (~5-10 GB, takes 20-40 minutes)

8. **Note the installation path**, typically:
   ```
   C:\Qt\6.6.3\msvc2019_64
   ```

---

## Step 3: Install Git

1. Download Git for Windows from:
   ```
   https://git-scm.com/download/win
   ```

2. Run the installer with default options

3. Verify installation:
   ```powershell
   git --version
   # Should output: git version 2.x.x
   ```

---

## Step 4: Install vcpkg & Dependencies

1. Open **PowerShell as Administrator**

2. Clone vcpkg:
   ```powershell
   cd C:\
   git clone https://github.com/microsoft/vcpkg.git
   cd vcpkg
   ```

3. Bootstrap vcpkg:
   ```powershell
   .\bootstrap-vcpkg.bat
   ```

4. Install libcurl:
   ```powershell
   .\vcpkg install curl:x64-windows
   .\vcpkg install sqlite3:x64-windows
   ```

5. Integrate with Visual Studio:
   ```powershell
   .\vcpkg integrate install
   ```

6. **Note the toolchain file path**:
   ```
   C:\vcpkg\scripts\buildsystems\vcpkg.cmake
   ```

---

## Step 5: Set Environment Variables

1. Open **System Properties** → **Advanced** → **Environment Variables**

2. Add to **PATH** (User variables):
   ```
   C:\Qt\6.6.3\msvc2019_64\bin
   C:\Qt\Tools\CMake_64\bin
   C:\vcpkg
   ```

3. Add new variable **Qt6_DIR**:
   ```
   C:\Qt\6.6.3\msvc2019_64\lib\cmake\Qt6
   ```

4. Add new variable **CMAKE_TOOLCHAIN_FILE**:
   ```
   C:\vcpkg\scripts\buildsystems\vcpkg.cmake
   ```

5. Click **OK** and restart any open terminals

---

## Step 6: Clone OpenIDM

1. Open **Git Bash** or **PowerShell**

2. Navigate to your projects folder:
   ```powershell
   cd C:\Projects  # or your preferred location
   ```

3. Clone the repository:
   ```powershell
   git clone https://github.com/openidm/openidm.git
   cd openidm
   ```

---

## Step 7: Build with CMake (Command Line)

### Using PowerShell / Developer Command Prompt

1. Open **"x64 Native Tools Command Prompt for VS 2022"** from Start Menu

2. Navigate to the project:
   ```cmd
   cd C:\Projects\openidm
   ```

3. Create build directory:
   ```cmd
   mkdir build
   cd build
   ```

4. Configure with CMake:
   ```cmd
   cmake .. -G "Visual Studio 17 2022" -A x64 ^
     -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake ^
     -DCMAKE_PREFIX_PATH=C:/Qt/6.6.3/msvc2019_64
   ```

5. Build:
   ```cmd
   cmake --build . --config Release
   ```

6. The executable will be at:
   ```
   build\bin\Release\OpenIDM.exe
   ```

---

## Step 8: Build with Qt Creator (GUI)

1. Open **Qt Creator**

2. **File** → **Open File or Project**

3. Navigate to `C:\Projects\openidm\CMakeLists.txt`

4. In the "Configure Project" dialog:
   - Select **Desktop Qt 6.6.3 MSVC2019 64bit**
   - Click **Configure Project**

5. Wait for CMake configuration to complete

6. Click the **Build** button (hammer icon) or press `Ctrl+B`

7. Click the **Run** button (play icon) or press `Ctrl+R`

---

## Step 9: Deploy for Distribution

To create a standalone executable that runs on any Windows machine:

1. Open **x64 Native Tools Command Prompt for VS 2022**

2. Navigate to the build output:
   ```cmd
   cd C:\Projects\openidm\build\bin\Release
   ```

3. Run Qt deployment tool:
   ```cmd
   C:\Qt\6.6.3\msvc2019_64\bin\windeployqt.exe --qmldir C:\Projects\openidm\qml OpenIDM.exe
   ```

4. The folder now contains all required DLLs and can be distributed

---

## Troubleshooting

### "Qt6 not found"
- Ensure `CMAKE_PREFIX_PATH` points to your Qt installation
- Verify Qt was installed with MSVC kit (not MinGW)

### "CURL not found"
- Run vcpkg install again: `vcpkg install curl:x64-windows`
- Ensure `CMAKE_TOOLCHAIN_FILE` is set correctly

### "QML module not found"
- Add Qt QML path to `QML_IMPORT_PATH` environment variable
- Verify Qt Quick was selected during Qt installation

### Build errors about C++20
- Ensure you're using Visual Studio 2022 (not 2019)
- Check that MSVC v143 toolset is installed

### Application won't start
- Run `windeployqt` to copy required DLLs
- Check Windows Event Viewer for crash details

---

## Optional: Install yt-dlp

For YouTube and streaming site support:

1. Download yt-dlp from:
   ```
   https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp.exe
   ```

2. Place `yt-dlp.exe` in one of:
   - Same folder as OpenIDM.exe
   - A folder in your PATH
   - `%APPDATA%\OpenIDM\`

3. OpenIDM will automatically detect yt-dlp on startup

---

## Project Structure After Build

```
openidm/
├── build/
│   ├── bin/
│   │   └── Release/
│   │       ├── OpenIDM.exe          ← Main executable
│   │       ├── Qt6Core.dll          ← Qt dependencies
│   │       ├── Qt6Quick.dll
│   │       ├── libcurl.dll          ← CURL dependency
│   │       └── qml/                 ← QML modules
│   └── lib/
├── src/                             ← C++ source code
├── qml/                             ← QML UI files
├── resources/                       ← Icons, translations
└── CMakeLists.txt                   ← Build configuration
```

---

## Quick Reference Commands

```powershell
# Configure (from build directory)
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_PREFIX_PATH=C:/Qt/6.6.3/msvc2019_64

# Build Release
cmake --build . --config Release

# Build Debug
cmake --build . --config Debug

# Clean rebuild
cmake --build . --target clean
cmake --build . --config Release

# Deploy
windeployqt --qmldir ../qml bin/Release/OpenIDM.exe
```

---

## Need Help?

- **Qt Documentation**: https://doc.qt.io/qt-6/
- **CMake Documentation**: https://cmake.org/documentation/
- **vcpkg Documentation**: https://vcpkg.io/en/docs/
- **OpenIDM Issues**: https://github.com/openidm/openidm/issues

Happy coding! 🚀
