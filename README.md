# NewLastReader / 新一代电子书阅读器

一个现代化的跨平台命令行电子书阅读器，支持多种格式并具备云同步功能。

A modern cross-platform command-line ebook reader with multi-format support and cloud synchronization.

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Version](https://img.shields.io/badge/version-1.0.0-green.svg)

## 🔐 Google Drive 云同步授权设置 / Google Drive Cloud Sync Authorization

本程序使用标准的 OAuth2 授权方式访问 Google Drive，您需要自行创建 Google Cloud 项目并获取授权凭据。

This application uses standard OAuth2 authorization to access Google Drive. You need to create your own Google Cloud project and obtain authorization credentials.

### 获取 Google Drive API 凭据 / Obtaining Google Drive API Credentials

1. **访问 Google Cloud Console** / **Visit Google Cloud Console**
   - 前往 [Google Cloud Console](https://console.cloud.google.com/)
   - Go to [Google Cloud Console](https://console.cloud.google.com/)

2. **创建或选择项目** / **Create or Select Project**
   - 创建新项目或选择现有项目
   - Create a new project or select an existing project

3. **启用 Google Drive API** / **Enable Google Drive API**
   - 导航到 "API和服务" > "库"
   - 搜索 "Google Drive API" 并启用
   - Navigate to "APIs & Services" > "Library"
   - Search for "Google Drive API" and enable it

4. **创建 OAuth 2.0 凭据** / **Create OAuth 2.0 Credentials**
   - 导航到 "API和服务" > "凭据"
   - 点击 "创建凭据" > "OAuth 2.0 客户端ID"
   - 应用类型选择："桌面应用程序"
   - 设置重定向URI：`http://localhost`
   - Navigate to "APIs & Services" > "Credentials"
   - Click "Create Credentials" > "OAuth 2.0 Client ID"
   - Application type: "Desktop Application"
   - Set redirect URI: `http://localhost`

5. **⚠️ 重要：测试用户设置** / **⚠️ Important: Test User Setup**
   - 在项目设置的 "OAuth同意屏幕" 中
   - 将您的 Google 账号添加到 "测试用户" 列表
   - 这是必需的，因为项目处于测试状态
   - In your project's "OAuth consent screen" settings
   - Add your Google account to the "Test users" list
   - This is required as the project is in testing status

6. **记录凭据信息** / **Record Credential Information**
   - 记录 Client ID（客户端ID）
   - 记录 Client Secret（客户端密钥）
   - Record the Client ID
   - Record the Client Secret

### 程序中配置凭据 / Configure Credentials in Application

程序启动时会自动检测是否已配置 Google Drive 凭据：
- 首次运行：会询问是否要配置云同步（可选择跳过）
- 运行时配置：在主界面按 `c` 键即可配置云同步

The application automatically detects if Google Drive credentials are configured:
- First run: Will ask if you want to configure cloud sync (can skip)
- Runtime configuration: Press `c` key in main interface to configure cloud sync

**这是标准的 OAuth2 授权方式，确保数据安全性和隐私保护。**

**This is the standard OAuth2 authorization method, ensuring data security and privacy protection.**

## 项目介绍 / Project Introduction

NewLastReader 是一个基于终端界面的现代化电子书阅读器，为命令行用户提供完整的阅读体验。

NewLastReader is a modern terminal-based ebook reader that provides a complete reading experience for command-line users.

### 核心特性 / Core Features

- **多格式支持 / Multi-format Support**: EPUB、PDF、MOBI、TXT
- **现代化 TUI / Modern TUI**: 基于 FTXUI 的直观终端用户界面
- **云同步 / Cloud Sync**: Google Drive 集成，跨设备同步阅读进度和书库
- **智能书库管理 / Smart Library Management**: 本地书籍管理和元数据存储
- **阅读进度跟踪 / Reading Progress Tracking**: 自动保存和恢复阅读位置
- **PDF 智能识别 / PDF Intelligence**: 自动检测文本型/图像型PDF并提供相应处理
- **用户自定义路径 / Custom Data Path**: 启动时可自定义数据存储位置

## 程序初始化说明 / Program Initialization Guide

### 首次启动流程 / First-Time Startup Process

当您首次运行程序时，会自动进入初始化向导：

When you run the program for the first time, it will automatically enter the initialization wizard:

1. **数据目录选择** / **Data Directory Selection**
   ```
   --- Welcome to new_lastreader (First-Time Setup) ---
   
   Please specify a directory to store your library and configuration.
   Press ENTER to use the default path (~/.all_reader).
   Enter path: [输入自定义路径或按回车使用默认]
   ```

2. **自动创建目录结构** / **Automatic Directory Structure Creation**
   ```
   {your_chosen_path}/
   ├── books/          # 书籍文件存储 / Book files storage
   └── config/         # 配置和数据库 / Configuration and database
       ├── library.db  # 主数据库 / Main database
       └── debug.log   # 调试日志 / Debug logs
   ```

3. **配置文件生成** / **Configuration File Generation**
   - 在用户主目录创建 `~/.cli_reader.json`
   - 记录数据存储路径以供后续启动使用
   - Creates `~/.cli_reader.json` in user home directory
   - Records data storage path for subsequent startups

4. **云同步设置（可选）** / **Cloud Sync Setup (Optional)**
   ```
   --- Cloud Sync Setup (Optional) ---
   Would you like to configure Google Drive cloud synchronization now?
   You can always set this up later by pressing 'c' in the main interface.
   Configure now? (y/N): 
   ```
   - 默认选择 "N"（跳过），可稍后配置
   - Default choice is "N" (skip), can be configured later

### 后续启动 / Subsequent Startups

- 程序会自动读取 `~/.cli_reader.json` 中的配置
- 直接使用已设置的数据目录启动
- The program automatically reads configuration from `~/.cli_reader.json`
- Starts directly using the previously set data directory

### 支持的格式 / Supported Formats

| 格式 | 功能 | 特性 |
|------|------|------|
| **EPUB** | 完整支持 | 目录导航、元数据提取、文本渲染 |
| **PDF** | 智能处理 | 文本提取、健康检查、分类识别 |
| **MOBI** | 原生支持 | Kindle格式兼容 |
| **TXT** | 基础支持 | 自动分页、编码检测 |

### 云同步功能 / Cloud Sync Features

- **标准授权**: 使用标准 OAuth2 流程，安全可靠
- **增量同步**: 只同步变更数据，节省带宽
- **多设备支持**: 在不同设备间无缝切换阅读
- **自定义凭据**: 使用您自己的 Google Drive API 凭据

## 依赖安装 / Dependencies Installation

### macOS

使用 Homebrew 安装所有依赖：

```bash
brew install cmake pkg-config ftxui libmobi poppler cpr zlib ncurses
```

### Linux

#### Ubuntu/Debian

```bash
sudo apt update && sudo apt install -y \
    build-essential cmake pkg-config \
    libftxui-dev libmobi-dev libpoppler-cpp-dev libcpr-dev \
    zlib1g-dev libncurses-dev
```

#### Arch Linux

```bash
sudo pacman -S cmake pkg-config gcc zlib ncurses
yay -S ftxui libmobi poppler cpr
```

### 从源码编译依赖（如果包管理器中缺少）

```bash
# FTXUI
git clone https://github.com/ArthurSonzogni/FTXUI.git && cd FTXUI
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc) && sudo cmake --install build

# CPR (如果需要)
git clone https://github.com/libcpr/cpr.git && cd cpr  
cmake -B build -DCPR_USE_SYSTEM_CURL=ON
cmake --build build -j$(nproc) && sudo cmake --install build
```

## 编译安装 / Build & Installation

### 从源码构建

```bash
git clone https://github.com/yourusername/new_lastreader.git
cd new_lastreader && mkdir build && cd build

# 动态构建 (推荐)
cmake -DCMAKE_BUILD_TYPE=Release ..

# 静态构建 (自包含)  
cmake -DCMAKE_BUILD_TYPE=Release -DSTATIC_BUILD=ON ..

make -j$(nproc)
sudo make install  # 可选
```

## 程序使用指南 / Getting Started

### 启动程序 / Starting the Program

```bash
./new_lastreader
```

程序首次运行会进入初始化向导（详见上方"程序初始化说明"部分）。
On first run, the program will enter the initialization wizard (see "Program Initialization Guide" section above).

### 基本使用

#### 主界面操作

- `Enter`: 打开选中的书籍
- `a`: 添加新书籍（打开文件选择器）
- `d`: 删除书籍
- `r`: 刷新书库（云同步状态下会触发云同步）
- `s`: 显示系统信息
- `c`: 配置/切换云同步状态
- `Tab`: 切换界面面板
- `q`: 退出程序

#### 阅读界面操作

- `↑↓` / `j k`: 上下滚动
- `←→` / `h l`: 前后翻页
- `Space`: 下一页
- `b`: 上一页
- `g`: 跳转到指定页面
- `t`: 显示目录
- `q`: 返回书库

#### 文件选择器

- `↑↓`: 浏览文件/目录
- `Enter`: 选择文件或进入目录
- `Backspace`: 返回上级目录
- `Escape`: 取消选择

### 添加书籍

1. 在主界面按 `a` 打开文件选择器
2. 浏览到书籍文件位置
3. 选择书籍文件（支持 EPUB/PDF/MOBI/TXT）
4. 程序自动复制文件到书库并提取元数据

### 云同步 / Cloud Synchronization

启用云同步后，程序会：
- 自动上传本地书籍到 Google Drive
- 同步阅读进度和书库数据
- 从云端下载其他设备的书籍

After enabling cloud sync, the program will:
- Automatically upload local books to Google Drive
- Sync reading progress and library data
- Download books from other devices via cloud

## 故障排除 / Troubleshooting

### 编译问题

**依赖库未找到**：
```bash
# 检查pkg-config
pkg-config --list-all | grep -E "(ftxui|mobi|poppler)"

# 手动指定路径
cmake -DCMAKE_PREFIX_PATH="/usr/local" ..
```

**链接错误**：
```bash
# 更新库缓存
sudo ldconfig

# 使用静态构建
cmake -DSTATIC_BUILD=ON ..
```

### 运行时问题

**数据库访问错误**：
- 检查数据目录权限
- 确保磁盘空间充足

**Google Drive 授权失败**：
- 检查网络连接
- 验证Client ID/Secret格式
- 确认重定向URI设置为 `http://localhost`

**文件格式不支持**：
- 检查文件是否损坏
- 确认文件扩展名正确

## 技术架构 / Architecture

### 核心组件

- **AppController**: 应用生命周期管理
- **DatabaseManager**: SQLite数据库操作
- **LibraryManager**: 本地书库管理
- **SyncController**: 云同步协调
- **UIComponents**: FTXUI界面组件
- **Parser系列**: 多格式文件解析

### 数据存储

- **library.db**: 主数据库
  - `books`: 书籍元数据和进度
  - `systemInfo`: 系统配置信息
- **books/**: 本地书籍文件存储
- **~/.cli_reader.json**: 数据路径配置

## 开发信息 / Development

### 构建选项

```bash
# 调试构建
cmake -DCMAKE_BUILD_TYPE=Debug ..

# 发布构建  
cmake -DCMAKE_BUILD_TYPE=Release ..

# 静态构建
cmake -DSTATIC_BUILD=ON ..
```

### 依赖库版本

| 库 | 版本 | 用途 |
|---|------|------|
| FTXUI | v5.0.0 | 终端UI框架 |
| libmobi | v0.12 | MOBI格式支持 |
| Poppler | latest | PDF处理 |
| CPR | latest | HTTP客户端 |
| SQLite | 3.42.0 | 数据库 |

## 许可证 / License

MIT License - 详见 [LICENSE](LICENSE) 文件

## 作者与支持 / Author & Support

- **作者**: guyiicn@gmail.com  
- **项目主页**: https://github.com/yourusername/new_lastreader
- **问题反馈**: https://github.com/yourusername/new_lastreader/issues

---

**享受阅读！ / Happy Reading!** 📖✨