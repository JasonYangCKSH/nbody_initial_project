import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

# 讀取 CSV
df = pd.read_csv("../build/particles.csv")

# 從 DataFrame 取出欄位
x = df["x"].values
y = df["y"].values
z = df["z"].values

# --- 建立 3D 圖 ---
fig = plt.figure()
ax = fig.add_subplot(111, projection="3d")
ax.scatter(x, y, z, s=10)

ax.set_title("Particle Positions")
ax.set_xlabel("x")
ax.set_ylabel("y")
ax.set_zlabel("z")

# ---- Axis equal 設定 ----
max_range = np.array([
    x.max() - x.min(),
    y.max() - y.min(),
    z.max() - z.min()
]).max() / 2.0

mid_x = (x.max() + x.min()) * 0.5
mid_y = (y.max() + y.min()) * 0.5
mid_z = (z.max() + z.min()) * 0.5

ax.set_xlim(mid_x - max_range, mid_x + max_range)
ax.set_ylim(mid_y - max_range, mid_y + max_range)
ax.set_zlim(mid_z - max_range, mid_z + max_range)

plt.show()
