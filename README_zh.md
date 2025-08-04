# All Reader - 命令行电子书阅读器

一个多功能的跨平台终端电子书阅读器。支持 EPUB、MOBI、AZW3 和 TXT 文件，提供简洁、专注的阅读体验。

## 功能特性

- **多格式支持**：EPUB、MOBI、AZW3 和 TXT 文件
- **智能导航**：自动提取目录和智能分页
- **持久化书库**：本地数据库管理您的图书收藏
- **跨平台**：支持 macOS 和 Linux

## 快速开始

### Linux（推荐）

```bash
# 安装基础构建工具
sudo apt install build-essential cmake pkg-config git autotools-dev autoconf libtool libxml2-dev

# 一键构建
./setup_build.sh && ./build_linux_static.sh
```

可执行文件 `ebook_reader_static` 将在 `build/` 目录中生成。

### macOS

```bash
# 安装依赖
brew install cmake pkg-config ftxui libmobi

# 构建
./setup_build.sh && ./build_macos_dynamic.sh
```

可执行文件 `ebook_reader` 将在 `build/` 目录中生成。

## 手动构建（高级用户）

如果您偏好手动控制：

```bash
mkdir build && cd build
cmake -DSTATIC_BUILD=ON ..  # Linux 静态构建
# cmake ..                  # macOS 动态构建
make -j$(nproc)
```

## 使用方法

```bash
./ebook_reader_static  # Linux
./ebook_reader         # macOS
```

浏览您的书库，选择图书，在终端中享受阅读吧！

## 依赖说明

自动化构建会处理所有依赖项。手动构建需要：
- CMake 3.16+
- C++17 编译器
- 系统库（自动管理）

## 故障排除

- **构建问题**：使用自动化构建脚本（`./setup_build.sh`）
- **依赖检查**：在 Linux 上运行 `./check_linux_deps.sh`
- **技术支持**：查看 [Issues](https://github.com/your-repo/issues) 了解常见问题

## 不同发行版安装命令

### Debian/Ubuntu
```bash
sudo apt install build-essential cmake pkg-config git autotools-dev autoconf libtool libxml2-dev
```

### Fedora/RHEL/CentOS
```bash
sudo dnf groupinstall "Development Tools"
sudo dnf install cmake pkg-config git autoconf automake libtool libxml2-devel
```

### Arch Linux
```bash
sudo pacman -S base-devel cmake pkg-config git autoconf automake libtool libxml2
```

## 许可证

本项目遵循标准开源实践。具体许可信息请查看源文件。