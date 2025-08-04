# Linux FTXUI 动态编译错误修复指南

## 问题描述

在Linux x86_64系统上进行动态编译时遇到FTXUI库链接错误，所有`ftxui::`符号都无法找到定义。

### 错误症状
```
/usr/bin/ld: main.cpp:(.text+0x14a2): undefined reference to `ftxui::border(std::shared_ptr<ftxui::Node>)'
/usr/bin/ld: main.cpp:(.text+0x14ca): undefined reference to `ftxui::hcenter(std::shared_ptr<ftxui::Node>)'
/usr/bin/ld: main.cpp:(.text+0x14e6): undefined reference to `ftxui::bold(std::shared_ptr<ftxui::Node>)'
... (更多类似的undefined reference错误)
```

## 根本原因分析

1. **错误的库链接方式**：原始CMakeLists.txt在动态构建时硬编码了错误的库名
2. **pkg-config配置问题**：没有正确使用pkg-config找到的库变量
3. **缺少备用查找机制**：当pkg-config失败时没有备用方案

## 修复方案

### 1. CMakeLists.txt 修改 - 库链接顺序问题

#### 修改前（有问题的代码）：
```cmake
else()
    # Be explicit for dynamic builds as well, as ${FTXUI_LIBRARIES} can be inconsistent.
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE ftxui-screen ftxui-dom ftxui-component ${MOBI_LIBRARIES} ${CURSES_LIBRARIES})
endif()
```

#### 第一次修复尝试（仍有问题）：
```cmake
else()
    # Dynamic build: use discovered libraries
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE ${FTXUI_LIBRARIES} ${MOBI_LIBRARIES} ${CURSES_LIBRARIES})
endif()
```

#### 最终修复（解决库顺序和目录问题）：
```cmake
# 在创建可执行文件之前添加库目录
if(NOT STATIC_BUILD)
    if(FTXUI_LIBRARY_DIRS)
        link_directories(${FTXUI_LIBRARY_DIRS})
    endif()
    if(MOBI_LIBRARY_DIRS)
        link_directories(${MOBI_LIBRARY_DIRS})
    endif()
endif()

# ... 创建可执行文件 ...

# 链接库时使用正确的顺序
else()
    # Dynamic build: Link libraries with correct order 
    # FTXUI链接顺序很重要：component -> dom -> screen
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE 
        ftxui-component 
        ftxui-dom 
        ftxui-screen 
        ${MOBI_LIBRARIES} 
        ${CURSES_LIBRARIES}
    )
    
    # Add compiler flags from pkg-config if available
    if(FTXUI_METHOD STREQUAL "pkg-config")
        target_compile_options(${EXECUTABLE_NAME} PRIVATE ${FTXUI_CFLAGS_OTHER})
    endif()
    if(MOBI_FOUND)
        target_compile_options(${EXECUTABLE_NAME} PRIVATE ${MOBI_CFLAGS_OTHER})
    endif()
endif()
```

**关键修复点：**
1. **库顺序**：FTXUI必须按照 `component -> dom -> screen` 的顺序链接
2. **库目录**：使用 `link_directories()` 在创建可执行文件之前添加库搜索路径
3. **显式库名**：直接使用 `ftxui-component` 等库名，而不是变量

### 2. 增强的FTXUI查找机制

#### 修改前（简单的pkg-config检查）：
```cmake
pkg_check_modules(FTXUI REQUIRED ftxui)
link_directories(${FTXUI_LIBRARY_DIRS})
```

#### 修改后（带备用方案的查找）：
```cmake
# Find FTXUI with fallback options
pkg_check_modules(FTXUI ftxui)
if(FTXUI_FOUND)
    message(STATUS "FTXUI found via pkg-config")
    message(STATUS "  FTXUI_LIBRARIES: ${FTXUI_LIBRARIES}")
    message(STATUS "  FTXUI_LIBRARY_DIRS: ${FTXUI_LIBRARY_DIRS}")
    message(STATUS "  FTXUI_INCLUDE_DIRS: ${FTXUI_INCLUDE_DIRS}")
    link_directories(${FTXUI_LIBRARY_DIRS})
    set(FTXUI_METHOD "pkg-config")
else()
    # Fallback: try to find FTXUI manually
    message(STATUS "FTXUI not found via pkg-config, trying manual detection...")
    
    find_library(FTXUI_SCREEN_LIB NAMES ftxui-screen)
    find_library(FTXUI_DOM_LIB NAMES ftxui-dom)  
    find_library(FTXUI_COMPONENT_LIB NAMES ftxui-component)
    find_path(FTXUI_INCLUDE_PATH ftxui/screen/screen.hpp)
    
    if(FTXUI_SCREEN_LIB AND FTXUI_DOM_LIB AND FTXUI_COMPONENT_LIB AND FTXUI_INCLUDE_PATH)
        message(STATUS "FTXUI found manually")
        message(STATUS "  Screen lib: ${FTXUI_SCREEN_LIB}")
        message(STATUS "  DOM lib: ${FTXUI_DOM_LIB}")
        message(STATUS "  Component lib: ${FTXUI_COMPONENT_LIB}")
        message(STATUS "  Include path: ${FTXUI_INCLUDE_PATH}")
        
        set(FTXUI_LIBRARIES "${FTXUI_COMPONENT_LIB};${FTXUI_DOM_LIB};${FTXUI_SCREEN_LIB}")
        set(FTXUI_INCLUDE_DIRS "${FTXUI_INCLUDE_PATH}")
        set(FTXUI_METHOD "manual")
    else()
        message(FATAL_ERROR "FTXUI not found. Please install libftxui-dev or build from source.")
    endif()
endif()
```

### 3. 依赖检查增强

添加了libmobi的详细检查：
```cmake
# Find libmobi with detailed error reporting  
pkg_check_modules(MOBI REQUIRED libmobi)
if(MOBI_FOUND)
    message(STATUS "libmobi found via pkg-config")
    message(STATUS "  MOBI_LIBRARIES: ${MOBI_LIBRARIES}")
    message(STATUS "  MOBI_LIBRARY_DIRS: ${MOBI_LIBRARY_DIRS}")
    message(STATUS "  MOBI_INCLUDE_DIRS: ${MOBI_INCLUDE_DIRS}")
    link_directories(${MOBI_LIBRARY_DIRS})
else()
    message(FATAL_ERROR "libmobi not found via pkg-config. Install libmobi-dev or similar package.")
endif()
```

## Linux系统依赖安装

### Ubuntu/Debian 系统：
```bash
# 基础开发工具
sudo apt-get update
sudo apt-get install build-essential cmake pkg-config

# 基础库
sudo apt-get install zlib1g-dev libncurses5-dev

# MOBI支持
sudo apt-get install libmobi-dev

# FTXUI支持（Ubuntu 22.04+）
sudo apt-get install libftxui-dev
```

### 如果包管理器中没有FTXUI，从源码编译：
```bash
# 编译安装FTXUI
git clone https://github.com/ArthurSonzogni/FTXUI.git
cd FTXUI
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
sudo make install
sudo ldconfig  # 更新动态库缓存
```

### RHEL/CentOS 系统：
```bash
# 基础开发工具
sudo yum groupinstall 'Development Tools'
sudo yum install cmake pkg-config

# 基础库
sudo yum install zlib-devel ncurses-devel

# libmobi（可能需要从EPEL或源码编译）
sudo yum install libmobi-devel

# FTXUI需要从源码编译（参见上面的编译步骤）
```

## 使用修复后的构建系统

### 1. 检查依赖：
```bash
./check_linux_deps.sh
```

### 2. 动态构建：
```bash
./build_linux_dynamic.sh --verbose
```

### 3. 如果动态构建仍有问题，使用静态构建：
```bash
# 使用原有CMakeLists.txt的静态构建选项
mkdir build && cd build
cmake -DSTATIC_BUILD=ON ..
make -j$(nproc)
```

## 故障排除

### 如果仍然遇到链接错误：

1. **检查pkg-config配置**：
```bash
pkg-config --list-all | grep ftxui
pkg-config --libs ftxui
pkg-config --cflags ftxui
```

2. **手动查找FTXUI库**：
```bash
find /usr -name "*ftxui*" 2>/dev/null
ldconfig -p | grep ftxui
```

3. **检查库文件权限**：
```bash
ls -la /usr/lib/x86_64-linux-gnu/*ftxui*
ls -la /usr/local/lib/*ftxui*
```

4. **更新动态库缓存**：
```bash
sudo ldconfig
```

### 调试CMake配置：
```bash
cd build
cmake .. -DCMAKE_VERBOSE_MAKEFILE=ON -DSTATIC_BUILD=OFF
```

这会显示详细的编译器和链接器命令，帮助诊断问题。

## 验证修复

成功构建后，应该看到类似输出：
```
-- FTXUI found via pkg-config
--   FTXUI_LIBRARIES: ftxui-component;ftxui-dom;ftxui-screen
--   FTXUI_LIBRARY_DIRS: /usr/lib/x86_64-linux-gnu
-- Dynamic build configuration:
--   FTXUI method: pkg-config
--   FTXUI libraries: ftxui-component;ftxui-dom;ftxui-screen
[100%] Linking CXX executable ebook_reader
```

构建的二进制文件应该能正常运行，不再出现undefined reference错误。

## 更新：最终解决方案（针对库顺序问题）

如果你的系统显示FTXUI已正确安装（如pkg-config检查通过），但仍然出现链接错误，这通常是库链接顺序的问题。

### 问题特征：
- pkg-config能找到FTXUI：`pkg-config --libs ftxui` 返回 `-lftxui-component -lftxui-dom -lftxui-screen`
- 编译过程正常，只在最后链接阶段失败
- 出现大量`undefined reference to ftxui::`错误

### 根本原因：
FTXUI的三个库之间有依赖关系：
- `ftxui-component` 依赖 `ftxui-dom`
- `ftxui-dom` 依赖 `ftxui-screen`
- 链接顺序必须是：`component -> dom -> screen`

### 现在可以尝试的修复：
1. **清理之前的构建**：
```bash
rm -rf build/
mkdir build && cd build
```

2. **重新配置和构建**：
```bash
cmake .. -DSTATIC_BUILD=OFF
make -j$(nproc)
```

3. **如果仍有问题，手动验证库文件**：
```bash
# 检查库文件是否存在
find /usr -name "*ftxui*" 2>/dev/null
ldd /usr/lib/x86_64-linux-gnu/libftxui-component.so* 2>/dev/null
```

### 备用解决方案：
如果动态链接仍有问题，强烈建议使用静态构建：
```bash
cmake .. -DSTATIC_BUILD=ON
make -j$(nproc)
```

静态构建会避免所有动态链接的复杂性问题。