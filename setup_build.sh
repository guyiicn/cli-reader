#!/bin/bash

# 跨平台构建配置脚本
# 自动检测操作系统并配置相应的构建方式

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

# 检测操作系统
detect_os() {
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        echo "linux"
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        echo "macos"
    else
        echo "unknown"
    fi
}

# 创建依赖目录结构
setup_deps_directory() {
    local deps_dir="$1"
    log_info "创建依赖目录结构: $deps_dir"
    
    mkdir -p "$deps_dir"/{src,build,install}
    mkdir -p "$deps_dir"/src/{ftxui,libmobi}
    mkdir -p "$deps_dir"/build/{ftxui,libmobi}
    mkdir -p "$deps_dir"/install/{ftxui,libmobi}
}

# 下载FTXUI源码
download_ftxui() {
    local src_dir="$1"
    log_info "下载FTXUI源码..."
    
    if [ ! -d "$src_dir/ftxui/.git" ]; then
        git clone --depth 1 --branch v5.0.0 https://github.com/ArthurSonzogni/FTXUI.git "$src_dir/ftxui"
        log_success "FTXUI源码下载完成"
    else
        log_info "FTXUI源码已存在，跳过下载"
    fi
}

# 下载libmobi源码
download_libmobi() {
    local src_dir="$1"
    log_info "下载libmobi源码..."
    
    if [ ! -d "$src_dir/libmobi/.git" ]; then
        git clone --depth 1 --branch v0.12 https://github.com/bfabiszewski/libmobi.git "$src_dir/libmobi"
        log_success "libmobi源码下载完成"
    else
        log_info "libmobi源码已存在，跳过下载"
    fi
}

# 编译FTXUI (Linux静态)
build_ftxui_static() {
    local src_dir="$1"
    local build_dir="$2"
    local install_dir="$3"
    
    log_info "静态编译FTXUI..."
    
    cd "$build_dir/ftxui"
    cmake "$src_dir/ftxui" \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=OFF \
        -DFTXUI_BUILD_EXAMPLES=OFF \
        -DFTXUI_BUILD_TESTS=OFF \
        -DFTXUI_ENABLE_INSTALL=ON \
        -DCMAKE_INSTALL_PREFIX="$install_dir/ftxui"
    
    make -j$(nproc)
    make install
    
    log_success "FTXUI静态编译完成"
}

# 编译libmobi (Linux静态)  
build_libmobi_static() {
    local src_dir="$1"
    local build_dir="$2"
    local install_dir="$3"
    
    log_info "静态编译libmobi..."
    
    cd "$src_dir/libmobi"
    
    # 如果没有configure脚本，运行autogen
    if [ ! -f "configure" ]; then
        ./autogen.sh
    fi
    
    cd "$build_dir/libmobi"
    "$src_dir/libmobi/configure" \
        --prefix="$install_dir/libmobi" \
        --enable-static \
        --disable-shared \
        --disable-tools
    
    make -j$(nproc)
    make install
    
    log_success "libmobi静态编译完成"
}

# Linux静态构建配置
setup_linux_static_build() {
    log_info "配置Linux静态构建环境..."
    
    local project_dir=$(pwd)
    local deps_dir="$project_dir/deps"
    
    # 创建目录结构
    setup_deps_directory "$deps_dir"
    
    # 下载源码
    download_ftxui "$deps_dir/src"
    download_libmobi "$deps_dir/src"
    
    # 编译依赖
    build_ftxui_static "$deps_dir/src" "$deps_dir/build" "$deps_dir/install"
    build_libmobi_static "$deps_dir/src" "$deps_dir/build" "$deps_dir/install"
    
    # 创建构建脚本
    cat > build_linux_static.sh << 'EOF'
#!/bin/bash
set -e

log_info() {
    echo -e "\033[0;34m[INFO]\033[0m $1"
}

log_info "开始Linux静态构建..."

# 清理构建目录
rm -rf build
mkdir build
cd build

# 配置CMake（强制静态构建模式）
cmake .. \
    -DSTATIC_BUILD=ON \
    -DCMAKE_BUILD_TYPE=Release \
    -DFTXUI_ROOT="${PWD}/../deps/install/ftxui" \
    -DLIBMOBI_ROOT="${PWD}/../deps/install/libmobi"

# 编译
make -j$(nproc)

log_info "Linux静态构建完成！可执行文件: build/ebook_reader_static"
EOF
    
    chmod +x build_linux_static.sh
    log_success "Linux静态构建环境配置完成"
}

# macOS动态构建配置
setup_macos_dynamic_build() {
    log_info "配置macOS动态构建环境..."
    
    # 检查Homebrew依赖
    if ! command -v brew &> /dev/null; then
        log_error "请先安装Homebrew: https://brew.sh/"
        exit 1
    fi
    
    log_info "检查Homebrew依赖..."
    brew list cmake pkg-config ftxui libmobi &> /dev/null || {
        log_info "安装必要依赖..."
        brew install cmake pkg-config ftxui libmobi
    }
    
    # 创建构建脚本
    cat > build_macos_dynamic.sh << 'EOF'
#!/bin/bash
set -e

log_info() {
    echo -e "\033[0;34m[INFO]\033[0m $1"
}

log_info "开始macOS动态构建..."

# 清理构建目录
rm -rf build
mkdir build
cd build

# 配置CMake（动态构建模式）
cmake .. \
    -DSTATIC_BUILD=OFF \
    -DCMAKE_BUILD_TYPE=Release

# 编译
make -j$(sysctl -n hw.ncpu)

log_info "macOS动态构建完成！可执行文件: build/ebook_reader"
EOF
    
    chmod +x build_macos_dynamic.sh
    log_success "macOS动态构建环境配置完成"
}

# 主函数
main() {
    echo "========================================="
    echo "    All Reader 跨平台构建配置工具"
    echo "========================================="
    echo
    
    local os=$(detect_os)
    log_info "检测到操作系统: $os"
    
    case $os in
        "linux")
            setup_linux_static_build
            log_success "Linux构建环境配置完成！运行 ./build_linux_static.sh 开始构建"
            ;;
        "macos")
            setup_macos_dynamic_build
            log_success "macOS构建环境配置完成！运行 ./build_macos_dynamic.sh 开始构建"
            ;;
        *)
            log_error "不支持的操作系统: $OSTYPE"
            log_info "支持的系统: Linux, macOS"
            exit 1
            ;;
    esac
}

# 运行主函数
main "$@"