#!/bin/bash

# Ubuntu 自动构建脚本 - NewLastReader
# 这个脚本会自动处理 Ubuntu 系统上的编译环境设置和构建过程

set -e  # 遇到错误立即退出

echo "=== NewLastReader Ubuntu 自动构建脚本 ==="
echo ""

# 检查是否在项目根目录
if [ ! -f "CMakeLists.txt.linux" ]; then
    echo "❌ 错误: 未找到 CMakeLists.txt.linux 文件"
    echo "请确保在项目根目录执行此脚本"
    exit 1
fi

# 检查系统依赖
echo "📦 检查系统依赖..."
MISSING_DEPS=""

# 检查是否安装了必要的包
check_package() {
    if ! dpkg -l | grep -q "^ii  $1 "; then
        MISSING_DEPS="$MISSING_DEPS $1"
    fi
}

check_package "build-essential"
check_package "cmake"
check_package "pkg-config"
check_package "libmobi-dev"
check_package "libpoppler-cpp-dev"
check_package "zlib1g-dev"
check_package "libncurses-dev"
check_package "nlohmann-json3-dev"

if [ -n "$MISSING_DEPS" ]; then
    echo "❌ 缺少以下系统依赖:$MISSING_DEPS"
    echo ""
    echo "请先安装依赖："
    echo "sudo apt update && sudo apt install -y build-essential cmake pkg-config libmobi-dev libpoppler-cpp-dev zlib1g-dev libncurses-dev nlohmann-json3-dev"
    echo ""
    read -p "是否现在自动安装依赖? (y/N): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo "🔄 正在安装系统依赖..."
        sudo apt update && sudo apt install -y build-essential cmake pkg-config libmobi-dev libpoppler-cpp-dev zlib1g-dev libncurses-dev nlohmann-json3-dev
        echo "✅ 系统依赖安装完成"
    else
        echo "❌ 请手动安装依赖后重新运行此脚本"
        exit 1
    fi
else
    echo "✅ 系统依赖检查通过"
fi

echo ""
echo "🔄 开始构建过程..."

# 1. 替换 CMakeLists.txt
echo "📝 使用 Ubuntu 专用 CMakeLists.txt..."
if [ -f "CMakeLists.txt" ]; then
    mv CMakeLists.txt CMakeLists.txt.backup
    echo "   已备份原 CMakeLists.txt 为 CMakeLists.txt.backup"
fi
cp CMakeLists.txt.linux CMakeLists.txt
echo "✅ CMakeLists.txt 替换完成"

# 2. 创建 libsrc 目录并下载依赖
echo ""
echo "📂 准备 libsrc 目录..."

if [ -d "libsrc" ]; then
    echo "   libsrc 目录已存在，跳过创建"
else
    mkdir -p libsrc
    echo "✅ 创建 libsrc 目录完成"
fi

cd libsrc

# 下载 FTXUI
if [ -d "ftxui" ]; then
    echo "   FTXUI 已存在，跳过下载"
else
    echo "⬇️  下载 FTXUI..."
    git clone https://github.com/ArthurSonzogni/FTXUI.git ftxui
    echo "✅ FTXUI 下载完成"
fi

# 下载 CPR
if [ -d "cpr" ]; then
    echo "   CPR 已存在，跳过下载"
else
    echo "⬇️  下载 CPR..."
    git clone https://github.com/libcpr/cpr.git cpr
    echo "✅ CPR 下载完成"
fi

cd ..

# 3. 创建构建目录
echo ""
echo "🏗️  准备构建目录..."
if [ -d "build" ]; then
    echo "   检测到现有 build 目录，是否清理重新构建?"
    read -p "   清理 build 目录? (y/N): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        rm -rf build
        echo "   已清理 build 目录"
    fi
fi

mkdir -p build
cd build

# 4. 配置和编译
echo ""
echo "⚙️  配置项目 (cmake)..."
cmake -DCMAKE_BUILD_TYPE=Release ..

echo ""
echo "🔨 开始编译 (使用 4 个并行任务)..."
make -j4

# 5. 完成
echo ""
echo "🎉 构建完成！"
echo ""
echo "可执行文件位置: $(pwd)/new_lastreader"
echo ""
echo "运行程序："
echo "  cd $(pwd)"
echo "  ./new_lastreader"
echo ""
echo "可选：安装到系统 (需要 sudo 权限):"
echo "  sudo make install"
echo ""

# 显示构建信息
if [ -f "new_lastreader" ]; then
    echo "📊 构建信息:"
    ls -lh new_lastreader
    echo ""
    echo "✅ 构建成功完成！"
else
    echo "❌ 构建可能失败，未找到可执行文件"
    exit 1
fi