# NewLastReader / 新一代电子书阅读器

一个现代化的跨平台命令行电子书阅读器，支持多种格式并具备云同步功能。

A modern cross-platform command-line ebook reader with multi-format support and cloud synchronization.

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Version](https://img.shields.io/badge/version-1.0.0-green.svg)

## 更新日志 / Update Log

### 2025-08-18 最新更新
- ✨ **PDF 智能检测**: 新增图像型PDF自动检测功能
- 🎮 **vim 导航支持**: 添加 j/k 键翻页功能
- 🔧 **Ubuntu 构建优化**: 提供专用 CMakeLists 和自动构建脚本
- 📦 **依赖管理改进**: 修正包管理器安装命令，添加 nlohmann-json 依赖
- 🚀 **一键构建**: Ubuntu 用户可使用 `./build-ubuntu.sh` 自动编译

## 核心特性 / Core Features

- **多格式支持**: EPUB、PDF、MOBI、TXT
- **现代化 TUI**: 基于 FTXUI 的直观终端用户界面  
- **云同步**: Google Drive 集成，跨设备同步
- **PDF 智能识别**: 自动检测文本型/图像型PDF
- **vim 风格导航**: 支持 hjkl 导航键

## 🔐 Google Drive 云同步授权设置

本程序使用标准的 OAuth2 授权方式访问 Google Drive，您需要自行创建 Google Cloud 项目并获取授权凭据。

### 获取 Google Drive API 凭据

1. **访问 Google Cloud Console** - 前往 [Google Cloud Console](https://console.cloud.google.com/)
2. **创建或选择项目** - 创建新项目或选择现有项目
3. **启用 Google Drive API** - 导航到 "API和服务" > "库"，搜索并启用 "Google Drive API"
4. **创建 OAuth 2.0 凭据** - 应用类型选择"桌面应用程序"，重定向URI设为 `http://localhost`
5. **⚠️ 重要：测试用户设置** - 将您的 Google 账号添加到 "测试用户" 列表
6. **记录凭据信息** - 记录 Client ID 和 Client Secret

### 程序中配置凭据

程序启动时会自动检测是否已配置 Google Drive 凭据：
- 首次运行：会询问是否要配置云同步（可选择跳过）
- 运行时配置：在主界面按 `c` 键即可配置云同步

## 依赖安装 / Dependencies Installation

### macOS

```bash
brew install cmake pkg-config libmobi poppler zlib ncurses nlohmann-json
```

### Ubuntu/Debian

```bash
sudo apt update && sudo apt install -y \
    build-essential cmake pkg-config \
    libmobi-dev libpoppler-cpp-dev \
    zlib1g-dev libncurses-dev nlohmann-json3-dev
```

### Arch Linux

```bash
sudo pacman -S cmake pkg-config gcc zlib ncurses libmobi poppler nlohmann-json
```

### Ubuntu/Debian 特殊构建步骤

**Ubuntu 用户请注意**：由于 Ubuntu 官方源中缺少 FTXUI 和 CPR 包，需要使用专用的 CMakeLists 文件。

#### 方法一：自动化脚本（推荐）

```bash
# 1. 克隆项目
git clone https://github.com/guyiicn/cli-reader.git
cd cli-reader

# 2. 运行自动构建脚本
./build-ubuntu.sh
```

#### 方法二：手动构建

```bash
# 1. 克隆项目
git clone https://github.com/guyiicn/cli-reader.git
cd cli-reader

# 2. 使用 Ubuntu 专用 CMakeLists 文件
cp CMakeLists.txt.linux CMakeLists.txt

# 3. 创建 libsrc 目录并下载源码依赖
mkdir -p libsrc
cd libsrc
git clone https://github.com/ArthurSonzogni/FTXUI.git ftxui
git clone https://github.com/libcpr/cpr.git cpr
cd ..

# 4. 编译
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

### macOS/其他 Linux 发行版构建

对于 macOS 和其他已有完整包管理器支持的 Linux 发行版：

```bash
# 1. 安装系统依赖（见上方依赖安装部分）
# 2. 克隆项目
git clone https://github.com/guyiicn/cli-reader.git
cd cli-reader

# 3. 编译项目
mkdir build && cd build

# 动态构建（推荐）
cmake -DCMAKE_BUILD_TYPE=Release ..

# 或静态构建（完全自包含）
cmake -DCMAKE_BUILD_TYPE=Release -DSTATIC_BUILD=ON ..

# 编译
make -j$(nproc)

# 可选：安装到系统
sudo make install
```

### 构建说明

- **Ubuntu 专用构建**：使用 `CMakeLists.txt.linux` 和 libsrc 目录解决依赖问题
- **动态构建**：使用系统已安装的依赖库
- **静态构建**：所有依赖都通过 FetchContent 自动下载编译，生成完全自包含的可执行文件

## 程序使用指南 / Getting Started

### 启动程序

```bash
./new_lastreader
```

程序首次运行会进入初始化向导，自动创建数据目录和配置文件。

### 基本使用

#### 主界面操作

- `Enter`: 打开选中的书籍
- `a`: 添加新书籍（打开文件选择器）
- `d`: 删除书籍
- `r`: 刷新书库（云同步状态下会触发云同步）
- `c`: 配置/切换云同步状态
- `q`: 退出程序

#### 阅读界面操作

- `↑↓` / `j k`: 上下滚动
- `←→` / `h l`: 前后翻页
- `Space`: 下一页
- `b`: 上一页
- `g`: 跳转到指定页面
- `t`: 显示目录
- `q`: 返回书库

### 云同步

启用云同步后，程序会：
- 自动上传本地书籍到 Google Drive
- 同步阅读进度和书库数据  
- 从云端下载其他设备的书籍

## 许可证 / License

MIT License - 详见 [LICENSE](LICENSE) 文件

## 作者与支持 / Author & Support

- **作者**: guyiicn@gmail.com  
- **项目主页**: https://github.com/guyiicn/cli-reader
- **问题反馈**: https://github.com/guyiicn/cli-reader/issues

---

**享受阅读！ / Happy Reading!** 📖✨

---

*本程序使用 AI 技术开发，主要使用了 [Claude Code](https://claude.ai/code) 和 [Gemini CLI](https://ai.google.dev/gemini-api/docs/cli) 进行代码生成和优化。*