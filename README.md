# All Reader - Command-Line Ebook Reader

A versatile, cross-platform ebook reader for the terminal. Supports EPUB, MOBI, AZW3, and TXT files with a clean, distraction-free reading experience.

## Features

- **Multi-Format Support**: EPUB, MOBI, AZW3, and TXT files
- **Smart Navigation**: Automatic table of contents extraction and pagination
- **Persistent Library**: Local database to manage your book collection
- **Cross-Platform**: Works on macOS and Linux

## Quick Start

### Linux (Recommended)

```bash
# Install basic build tools
sudo apt install build-essential cmake pkg-config git autotools-dev autoconf libtool libxml2-dev

# One-command build
./setup_build.sh && ./build_linux_static.sh
```

The executable `ebook_reader_static` will be created in the `build/` directory.

### macOS

```bash
# Install dependencies
brew install cmake pkg-config ftxui libmobi

# Build
./setup_build.sh && ./build_macos_dynamic.sh
```

The executable `ebook_reader` will be created in the `build/` directory.

## Manual Build (Advanced)

If you prefer manual control:

```bash
mkdir build && cd build
cmake -DSTATIC_BUILD=ON ..  # Linux static build
# cmake ..                  # macOS dynamic build
make -j$(nproc)
```

## Usage

```bash
./ebook_reader_static  # Linux
./ebook_reader         # macOS
```

Navigate your library, select books, and enjoy reading in the terminal!

## Dependencies

The automated build handles all dependencies. For manual builds, you need:
- CMake 3.16+
- C++17 compiler
- System libraries (automatically managed)

## Troubleshooting

- **Build Issues**: Use the automated build scripts (`./setup_build.sh`)
- **Dependency Check**: Run `./check_linux_deps.sh` on Linux
- **Support**: Check [Issues](https://github.com/your-repo/issues) for common problems

## License

This project follows standard open source practices. See source files for specific licensing information.