#!/bin/bash
# 手动测试脚本

echo "========================================="
echo "测试 1: 启动xv6并检查vmprint输出"
echo "========================================="
timeout 10 make qemu CPUS=1 <<EOF 2>&1 | head -25
EOF

pkill -9 qemu 2>/dev/null
sleep 1

echo ""
echo "========================================="
echo "测试 2: 运行usertests检查内核功能"
echo "========================================="
echo "输入 'usertests' 并等待..."

