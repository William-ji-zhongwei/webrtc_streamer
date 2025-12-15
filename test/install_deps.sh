#!/bin/bash
set -e

# 获取脚本所在目录
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

echo "🐍 安装 Python 接收端依赖..."
pip3 install --user -r "$SCRIPT_DIR/requirements.txt"

echo "✅ 接收端依赖安装完成！"
