#!/bin/bash
set -e

log_info() {
    echo -e "\033[0;34m[INFO]\033[0m $1"
}

log_success() {
    echo -e "\033[0;32m[SUCCESS]\033[0m $1"
}

log_error() {
    echo -e "\033[0;31m[ERROR]\033[0m $1"
}

log_info "开始Linux静态构建..."

# 检查依赖目录是否存在
if [ ! -d "deps/install/ftxui" ] || [ ! -d "deps/install/libmobi" ]; then
    log_error "依赖没有正确安装，请先运行 ./setup_build.sh"
    exit 1
fi

# 清理构建目录
log_info "清理构建目录..."
rm -rf build
mkdir build
cd build

# 配置CMake（强制静态构建模式）
log_info "配置CMake..."
cmake .. \
    -DSTATIC_BUILD=ON \
    -DCMAKE_BUILD_TYPE=Release \
    -DFTXUI_ROOT="${PWD}/../deps/install/ftxui" \
    -DLIBMOBI_ROOT="${PWD}/../deps/install/libmobi"

# 编译
log_info "开始编译..."
make -j$(nproc)

if [ $? -eq 0 ]; then
    log_success "Linux静态构建完成！"
    log_success "可执行文件位置: $(pwd)/ebook_reader_static"
    
    # 显示文件信息
    if [ -f "ebook_reader_static" ]; then
        echo ""
        log_info "可执行文件信息:"
        ls -lh ebook_reader_static
        file ebook_reader_static
    fi
else
    log_error "编译失败！"
    exit 1
fi