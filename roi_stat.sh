#!/bin/bash

file=$1
echo $file
nums=`cat $file | grep -P "CURR_MINSTRET\s*=\s*\d+" -o | grep -P "\d+$" -o`
# 将字符串分割为数组
arr=($nums)

# 获取数组长度
len=${#arr[@]}

# 遍历数组，每两个数字一组
for ((i=0; i<$len-1; i+=2)); do
    # 计算相邻元素的差值
    diff=$(( ${arr[$i+1]} - ${arr[$i]} ))
    # 打印结果
    echo "$diff"
done