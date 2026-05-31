#!/usr/bin/env python3
"""
比對 bruteforce、uniformgrid、octree 三種演算法的性能
繪製折線圖：X軸為 bodynum，Y軸為時間(ms)
"""

import matplotlib.pyplot as plt
import numpy as np

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

bruteforce_body, bruteforce_time = read_data('bruteforce.csv')
uniformgrid_body, uniformgrid_time = read_data('uniformgrid.csv')
octree_body, octree_time = read_data('octree.csv')

plt.figure(figsize=(10, 6))

plt.plot(bruteforce_body, bruteforce_time, marker='o', linewidth=2,
         label='Brute Force', color='#FF6B6B')
plt.plot(uniformgrid_body, uniformgrid_time, marker='s', linewidth=2,
         label='Uniform Grid', color='#4ECDC4')
plt.plot(octree_body, octree_time, marker='^', linewidth=2,
         label='Octree', color='#A29BFE')

plt.xlabel('Body Number', fontsize=12, fontweight='bold')
plt.ylabel('Time (ms)', fontsize=12, fontweight='bold')
plt.title('Performance Comparison: Brute Force vs Uniform Grid vs Octree',
          fontsize=13, fontweight='bold')

plt.grid(True, alpha=0.3, linestyle='--')
plt.legend(fontsize=11, loc='upper left')
plt.xticks(bruteforce_body, rotation=45)
plt.tight_layout()

plt.savefig('comparison.png', dpi=300, bbox_inches='tight')
print("圖表已保存為 comparison.png")
plt.show()

print("\n=== 性能統計 ===")
print(f"{'Body Num':<12} {'Brute Force (ms)':<20} {'Uniform Grid (ms)':<20} {'Octree (ms)':<15} {'BF/Grid':<10} {'BF/Octree':<10}")
print("-" * 90)
for i in range(len(bruteforce_body)):
    speedup_grid   = bruteforce_time[i] / uniformgrid_time[i]
    speedup_octree = bruteforce_time[i] / octree_time[i]
    print(f"{bruteforce_body[i]:<12} "
          f"{bruteforce_time[i]:<20.3f} "
          f"{uniformgrid_time[i]:<20.3f} "
          f"{octree_time[i]:<15.3f} "
          f"{speedup_grid:<10.2f}x "
          f"{speedup_octree:<10.2f}x")