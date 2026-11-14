#!/bin/bash
set -e
# 检查cmake 和 make
status=0
if ! command -v cmake >/dev/null 2>&1; then
    echo "找不到 cmake"
    status=1
fi

if ! command -v make >/dev/null 2>&1; then
    echo "找不到 make"
    status=1
fi

# 检查编译器
if ! command -v g++ >/dev/null 2>&1; then
    echo "找不到 g++"
    status=1
fi

# 检查fastdfs环境
if ! command -v fdfs_upload_file >/dev/null 2>&1; then
    echo "找不到 fdfs_upload_file"
    status=1
fi

if ! command -v fdfs_delete_file >/dev/null 2>&1; then
    echo "找不到 fdfs_delete_file"
    status=1
fi

# 检查配置文件
if [ ! -f ../conf/config.local.json ]; then
    echo "缺少 ../conf/config.local.json配置文件"
    status=1
elif [ ! -r ../conf/config.local.json ]; then
    echo "无法读取 ../conf/config.local.json配置文件"
    status=1
fi

# 检查FDFSCLIENTCONF
if [ -z "${FDFSCLIENTCONF:-}" ]; then
    echo "未设置FDFSCLIENTCONF"
    status=1
elif [ ! -f "${FDFSCLIENTCONF}" ] || [ ! -r "${FDFSCLIENTCONF}" ]; then
    echo "FDFSCLIENTCONF指向配置文件不存在或不可读"
    status=1
fi

# 检查SMTP_PASS
if [ -z "${SMTP_PASS:-}" ]; then
    echo "未设置 SMTP_PASS"
    status=1
fi

if [ $status -eq 0 ]; then
    echo "配置检查通过"
fi

exit "$status"