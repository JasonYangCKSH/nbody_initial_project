"""
比對 uniformgrid、octree 兩種演算法的性能
繪製折線圖：X軸為 bodynum，Y軸為時間(ms)
"""

import matplotlib.pyplot as plt

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

uniformgrid_body, uniformgrid_time = read_data('uniformgrid.csv')
octree_body, octree_time = read_data('octree.csv')

plt.figure(figsize=(10, 6))

plt.plot(uniformgrid_body, uniformgrid_time, marker='s', linewidth=2,
         label='Uniform Grid', color='#4ECDC4')
plt.plot(octree_body, octree_time, marker='^', linewidth=2,
         label='Octree', color='#A29BFE')

plt.xlabel('Body Number', fontsize=12, fontweight='bold')
plt.ylabel('Time (ms)', fontsize=12, fontweight='bold')
plt.title('Performance Comparison: Uniform Grid vs Octree',
          fontsize=13, fontweight='bold')

plt.grid(True, alpha=0.3, linestyle='--')
plt.legend(fontsize=11, loc='upper left')
plt.xticks(uniformgrid_body, rotation=45)
plt.tight_layout()

plt.savefig('comparison_grid_octree.png', dpi=300, bbox_inches='tight')
print("圖表已保存為 comparison_grid_octree.png")
plt.show()

print("\n=== 性能統計 ===")
print(f"{'Body Num':<12} {'Uniform Grid (ms)':<20} {'Octree (ms)':<15} {'Grid/Octree':<12}")
print("-" * 62)
for i in range(len(uniformgrid_body)):
    speedup = uniformgrid_time[i] / octree_time[i]
    print(f"{uniformgrid_body[i]:<12} "
          f"{uniformgrid_time[i]:<20.3f} "
          f"{octree_time[i]:<15.3f} "
          f"{speedup:<12.2f}x")