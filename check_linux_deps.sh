#!/bin/bash

# Linux动态构建依赖检查脚本
# 检查系统是否安装了必要的开发包

set -e

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

echo "========================================="
echo "    Linux动态构建依赖检查工具"
echo "========================================="
echo

# 检查操作系统
if [[ "$OSTYPE" != "linux-gnu"* ]]; then
    log_warning "此脚本专为Linux系统设计，当前系统: $OSTYPE"
fi

# 检查pkg-config
log_info "检查 pkg-config..."
if command -v pkg-config >/dev/null 2>&1; then
    log_success "pkg-config 已安装: $(pkg-config --version)"
else
    log_error "pkg-config 未安装"
    echo "  Ubuntu/Debian: sudo apt-get install pkg-config"
    echo "  RHEL/CentOS: sudo yum install pkg-config"
    exit 1
fi

# 检查FTXUI
log_info "检查 FTXUI..."
if pkg-config --exists ftxui; then
    log_success "FTXUI 已安装"
    echo "  版本: $(pkg-config --modversion ftxui)"
    echo "  库文件: $(pkg-config --libs ftxui)"
    echo "  头文件: $(pkg-config --cflags ftxui)"
    
    # 验证库文件是否存在
    FTXUI_LIBS=$(pkg-config --libs ftxui)
    log_info "验证FTXUI库文件..."
    if echo "$FTXUI_LIBS" | grep -q "ftxui-component\|ftxui::component"; then
        log_success "FTXUI组件库正常"
    else
        log_warning "FTXUI库配置可能不完整: $FTXUI_LIBS"
    fi
else
    log_error "FTXUI 未安装或未正确配置pkg-config"
    echo "  安装方法:"
    echo "    Ubuntu 22.04+: sudo apt-get install libftxui-dev"
    echo "    其他系统: 需要从源码编译安装"
    echo "    源码地址: https://github.com/ArthurSonzogni/FTXUI"
    MISSING_DEPS=true
fi

# 检查libmobi
log_info "检查 libmobi..."
if pkg-config --exists libmobi; then
    log_success "libmobi 已安装"
    echo "  版本: $(pkg-config --modversion libmobi)"
    echo "  库文件: $(pkg-config --libs libmobi)"
    echo "  头文件: $(pkg-config --cflags libmobi)"
else
    log_error "libmobi 未安装或未正确配置pkg-config"
    echo "  安装方法:"
    echo "    Ubuntu/Debian: sudo apt-get install libmobi-dev"
    echo "    RHEL/CentOS: sudo yum install libmobi-devel"
    echo "    或从源码编译: https://github.com/bfabiszewski/libmobi"
    MISSING_DEPS=true
fi

# 检查其他必要依赖
log_info "检查其他系统依赖..."

# 检查cmake
if command -v cmake >/dev/null 2>&1; then
    CMAKE_VERSION=$(cmake --version | head -n1 | grep -o '[0-9]\+\.[0-9]\+\.[0-9]\+')
    log_success "CMake 已安装: $CMAKE_VERSION"
    
    # 检查版本是否满足要求
    if [[ $(echo "$CMAKE_VERSION 3.16" | awk '{print ($1 >= $2)}') == "1" ]]; then
        log_success "CMake版本满足要求 (>= 3.16)"
    else
        log_error "CMake版本过低，需要 >= 3.16，当前: $CMAKE_VERSION"
        MISSING_DEPS=true
    fi
else
    log_error "CMake 未安装"
    echo "  Ubuntu/Debian: sudo apt-get install cmake"
    echo "  RHEL/CentOS: sudo yum install cmake"
    MISSING_DEPS=true
fi

# 检查编译器
if command -v g++ >/dev/null 2>&1; then
    GCC_VERSION=$(g++ --version | head -n1)
    log_success "GCC 已安装: $GCC_VERSION"
elif command -v clang++ >/dev/null 2>&1; then
    CLANG_VERSION=$(clang++ --version | head -n1)
    log_success "Clang 已安装: $CLANG_VERSION"
else
    log_error "C++ 编译器未安装"
    echo "  Ubuntu/Debian: sudo apt-get install build-essential"
    echo "  RHEL/CentOS: sudo yum groupinstall 'Development Tools'"
    MISSING_DEPS=true
fi

# 检查zlib
if pkg-config --exists zlib; then
    log_success "zlib 已安装: $(pkg-config --modversion zlib)"
else
    log_error "zlib 开发包未安装"
    echo "  Ubuntu/Debian: sudo apt-get install zlib1g-dev"
    echo "  RHEL/CentOS: sudo yum install zlib-devel"
    MISSING_DEPS=true
fi

# 检查ncurses
if pkg-config --exists ncurses; then
    log_success "ncurses 已安装: $(pkg-config --modversion ncurses)"
else
    log_warning "ncurses pkg-config未找到，尝试查找库文件..."
    if ldconfig -p | grep -q libncurses; then
        log_success "ncurses 库文件存在"
    else
        log_error "ncurses 开发包未安装"
        echo "  Ubuntu/Debian: sudo apt-get install libncurses5-dev"
        echo "  RHEL/CentOS: sudo yum install ncurses-devel"
        MISSING_DEPS=true
    fi
fi

echo
echo "========================================="
echo "            检查结果总结"
echo "========================================="

if [[ "$MISSING_DEPS" == "true" ]]; then
    log_error "存在缺失的依赖项，请安装后重试"
    echo
    log_info "Ubuntu/Debian 系统快速安装命令:"
    echo "  sudo apt-get update"
    echo "  sudo apt-get install build-essential cmake pkg-config"
    echo "  sudo apt-get install zlib1g-dev libncurses5-dev"
    echo "  sudo apt-get install libmobi-dev"
    echo "  # FTXUI可能需要从源码编译或使用更新的包管理器"
    echo
    log_info "构建建议:"
    echo "  如果动态依赖难以满足，建议使用静态构建:"
    echo "  cmake -DSTATIC_BUILD=ON ."
    exit 1
else
    log_success "所有依赖项检查通过！"
    echo
    log_info "可以开始动态构建:"
    echo "  mkdir build && cd build"
    echo "  cmake .."
    echo "  make -j$(nproc)"
    echo
    log_info "如果仍有问题，请尝试静态构建:"
    echo "  cmake -DSTATIC_BUILD=ON .."
fi