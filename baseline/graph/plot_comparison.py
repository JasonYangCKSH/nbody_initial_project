#!/usr/bin/env python3
"""
比對bruteforce和uniformgrid兩種演算法的性能
繪製折線圖：X軸為bodynum，Y軸為時間(ms)
"""

import matplotlib.pyplot as plt
import numpy as np

# 讀取數據
def read_data(filename):
    bodynum = []
    time = []
    with open(filename, 'r') as f:
        for line in f:
            line = line.strip()
            if line:
                parts = line.split()
                bodynum.append(int(parts[0]))
                time.append(float(parts[1]))
    return bodynum, time

# 讀取兩個檔案的數據
bruteforce_body, bruteforce_time = read_data('bruteforce.csv')
uniformgrid_body, uniformgrid_time = read_data('uniformgrid.csv')

# 創建圖表
plt.figure(figsize=(10, 6))

# 繪製折線圖
plt.plot(bruteforce_body, bruteforce_time, marker='o', linewidth=2, 
         label='Brute Force', color='#FF6B6B')
plt.plot(uniformgrid_body, uniformgrid_time, marker='s', linewidth=2, 
         label='Uniform Grid', color='#4ECDC4')

# 設置軸標籤和標題
plt.xlabel('Body Number', fontsize=12, fontweight='bold')
plt.ylabel('Time (ms)', fontsize=12, fontweight='bold')
plt.title('Performance Comparison: Brute Force vs Uniform Grid', 
          fontsize=14, fontweight='bold')

# 設置網格
plt.grid(True, alpha=0.3, linestyle='--')

# 設置圖例
plt.legend(fontsize=11, loc='upper left')

# 設置座標軸的刻度
plt.xticks(bruteforce_body, rotation=45)

# 調整佈局以防止標籤被裁剪
plt.tight_layout()

# 保存圖表
plt.savefig('comparison.png', dpi=300, bbox_inches='tight')
print("圖表已保存為 comparison.png")

# 顯示圖表
plt.show()

# 打印統計信息
print("\n=== 性能統計 ===")
print(f"{'Body Num':<12} {'Brute Force (ms)':<18} {'Uniform Grid (ms)':<18} {'加速比':<10}")
print("-" * 60)
for i in range(len(bruteforce_body)):
    speedup = bruteforce_time[i] / uniformgrid_time[i]
    print(f"{bruteforce_body[i]:<12} {bruteforce_time[i]:<18.3f} {uniformgrid_time[i]:<18.3f} {speedup:<10.2f}x")
