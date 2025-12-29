# OpenIDM

<div align="center">

![OpenIDM Logo](resources/icons/app_icon.svg)

**High-Performance, Cross-Platform Download Manager**

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Qt 6](https://img.shields.io/badge/Qt-6.5+-green.svg)](https://www.qt.io/)
[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux%20%7C%20macOS%20%7C%20Android-lightgrey.svg)]()

</div>

---

## 🚀 Features

### Core Download Engine
- **Multi-segment downloading** - Up to 32 parallel connections per file
- **Dynamic re-segmentation** - Work-stealing algorithm maximizes bandwidth
- **Reliable pause/resume** - Survives crashes and power loss
- **Smart retry logic** - Automatic retry with exponential backoff
- **HTTPS support** - Full certificate validation

### Streaming Support
- **YouTube** and 1000+ sites via yt-dlp integration
- **HLS/M3U8** stream support
- **Automatic format selection**

### Modern UI
- **Glassmorphism design** - Beautiful dark theme
- **Responsive layout** - Desktop and mobile friendly
- **Real-time progress** - Live speed and ETA updates
- **Native notifications** - System integration

### Cross-Platform
- **Windows** 10/11 (x64)
- **Linux** (X11/Wayland)
- **macOS** 11+
- **Android** 7.0+

---

## 📸 Screenshots

<div align="center">
<img src="docs/screenshots/main_window.png" width="800" alt="Main Window">
</div>

---

## 🏗️ Architecture

OpenIDM uses a clean MVVM architecture:

```
┌─────────────────────────────────────────────────────────┐
│                    QML UI (View)                        │
├─────────────────────────────────────────────────────────┤
│               ViewModel (Qt/C++)                        │
│         DownloadListModel, ViewModels                   │
├─────────────────────────────────────────────────────────┤
│              Download Engine (C++)                      │
│   DownloadManager → DownloadTask → SegmentWorker       │
│              ↓                                          │
│        SegmentScheduler (Work-Stealing)                 │
├─────────────────────────────────────────────────────────┤
│            Persistence (SQLite + WAL)                   │
└─────────────────────────────────────────────────────────┘
```

For detailed architecture documentation, see [ARCHITECTURE.md](docs/design/ARCHITECTURE.md).

---

## 🛠️ Building from Source

### Prerequisites

| Platform | Requirements |
|----------|--------------|
| **Windows** | Visual Studio 2022, Qt 6.5+, CMake 3.21+ |
| **Linux** | GCC 12+ / Clang 15+, Qt 6.5+, CMake 3.21+ |
| **macOS** | Xcode 14+, Qt 6.5+, CMake 3.21+ |
| **Android** | Android NDK r25+, Qt 6.5+ |

### Quick Start

```bash
# Clone repository
git clone https://github.com/openidm/openidm.git
cd openidm

# Create build directory
mkdir build && cd build

# Configure (adjust Qt path as needed)
cmake .. -DCMAKE_PREFIX_PATH=/path/to/Qt/6.6.0/gcc_64

# Build
cmake --build . --parallel

# Run
./bin/OpenIDM
```

### Platform-Specific Guides

- **Windows:** [Getting Started Guide](docs/guides/WINDOWS_GETTING_STARTED.md)
- **Linux:** [Linux Build Guide](docs/guides/LINUX_BUILD.md)
- **macOS:** [macOS Build Guide](docs/guides/MACOS_BUILD.md)
- **Android:** [Android Build Guide](docs/guides/ANDROID_BUILD.md)

---

## 📖 Usage

### Basic Usage

1. Click **"+ Add Download"**
2. Paste URL
3. Click **"Add Download"**

OpenIDM automatically:
- Detects file size and name
- Determines optimal segment count
- Starts parallel download

### Keyboard Shortcuts

| Key | Action |
|-----|--------|
| `Ctrl+V` | Add URL from clipboard |
| `Ctrl+P` | Pause selected |
| `Ctrl+R` | Resume selected |
| `Delete` | Remove selected |
| `Ctrl+,` | Open settings |

### Command Line

```bash
# Add download
openidm --add "https://example.com/file.zip"

# Add with output path
openidm --add "https://example.com/file.zip" --output ~/Downloads/

# Start minimized
openidm --minimized
```

---

## 🔧 Configuration

Settings are stored in:
- **Windows:** `%APPDATA%/OpenIDM/`
- **Linux:** `~/.config/OpenIDM/`
- **macOS:** `~/Library/Application Support/OpenIDM/`

### Key Settings

```json
{
  "maxConcurrentDownloads": 3,
  "maxSegmentsPerDownload": 16,
  "defaultDownloadDirectory": "~/Downloads",
  "speedLimit": 0,
  "startMinimized": false,
  "closeToTray": true
}
```

---

## 🧪 Testing

```bash
# Build with tests
cmake .. -DOPENIDM_BUILD_TESTS=ON
cmake --build .

# Run tests
ctest --output-on-failure
```

---

## 📦 Packaging

### Windows Installer
```bash
cpack -G NSIS
```

### Linux AppImage
```bash
cpack -G External
# Or use linuxdeploy
```

### macOS DMG
```bash
cpack -G DragNDrop
```

---

## 🤝 Contributing

We welcome contributions! Please see [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

### Development Setup

1. Fork the repository
2. Create a feature branch: `git checkout -b feature/my-feature`
3. Make your changes
4. Run tests: `ctest`
5. Submit a pull request

### Code Style

- Follow Qt coding conventions
- Use clang-format with the provided `.clang-format`
- Document public APIs with Doxygen comments

---

## 📄 License

OpenIDM is licensed under the **MIT License**. See [LICENSE](LICENSE) for details.

### Third-Party Licenses

| Library | License |
|---------|---------|
| Qt | LGPL v3 / Commercial |
| libcurl | MIT/X derivative |
| SQLite | Public Domain |
| yt-dlp | Unlicense |

---

## 🙏 Acknowledgments

- Qt Project for the excellent framework
- libcurl developers
- yt-dlp maintainers
- All contributors and users

---

## 📬 Contact

- **Issues:** [GitHub Issues](https://github.com/openidm/openidm/issues)
- **Discussions:** [GitHub Discussions](https://github.com/openidm/openidm/discussions)
- **Email:** contact@openidm.org

---

<div align="center">
Made with ❤️ by the OpenIDM Team
</div>
