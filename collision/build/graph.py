import pandas as pd
import matplotlib.pyplot as plt

# ===== 設定：分別指向 Local Velocity 跟 Fixed Radius 兩份 CSV =====
LV_CSV_PATH = "bench_results_local_velocity.csv"
FR_CSV_PATH = "bench_results_fixed_radius.csv"
OUTPUT_PATH = "k_sweep_plot.png"

def load_and_clean(path):
    df = pd.read_csv(path)
    df_numeric = df[df["K"] != "radius"].copy()
    df_numeric["K"] = df_numeric["K"].astype(int)
    return df_numeric.sort_values("K")

lv = load_and_clean(LV_CSV_PATH)
fr = load_and_clean(FR_CSV_PATH)

scenarios = lv["scenario"].unique()
n = len(scenarios)

fig, axes = plt.subplots(n, 3, figsize=(16, 5 * n))
if n == 1:
    axes = axes.reshape(1, -1)

for i, scenario in enumerate(scenarios):
    lv_sub = lv[lv["scenario"] == scenario]
    fr_sub = fr[fr["scenario"] == scenario]

    # ---- 圖一：總執行時間 vs K，兩條曲線疊在一起 ----
    ax = axes[i, 0]
    ax.plot(lv_sub["K"], lv_sub["total_s"], marker="o", color="#2a78d6",
            label="Local Velocity (dynamic skin)")
    ax.plot(fr_sub["K"], fr_sub["total_s"], marker="s", color="#eb6834",
            label="Fixed Radius (skin = radius)")
    ax.set_xscale("log")
    ax.set_xlim(left=0.8)  # K=0 那個點改用文字標註，避免 log(0) 問題
    ax.set_xlabel("K factor")
    ax.set_ylabel("Total time (s)")
    ax.set_title(f"{scenario}: total time vs K")
    ax.legend()
    ax.grid(True, alpha=0.3)

    # ---- 圖二：broad-phase 執行次數 vs K，兩條曲線疊在一起 ----
    ax = axes[i, 1]
    ax.plot(lv_sub["K"], lv_sub["broadphase_execs"], marker="o", color="#2a78d6",
            label="Local Velocity")
    ax.plot(fr_sub["K"], fr_sub["broadphase_execs"], marker="s", color="#eb6834",
            label="Fixed Radius")
    ax.set_xscale("log")
    ax.set_xlim(left=0.8)
    ax.set_xlabel("K factor")
    ax.set_ylabel("Broad-phase executions")
    ax.set_title(f"{scenario}: rebuild count vs K")
    ax.legend()
    ax.grid(True, alpha=0.3)

    # ---- 圖三：broad/narrow trade-off，只看 Local Velocity ----
    ax = axes[i, 2]
    ax.plot(lv_sub["K"], lv_sub["broadphase_s"], marker="o", color="#eb6834",
            label="broad-phase")
    ax.plot(lv_sub["K"], lv_sub["narrowphase_s"], marker="^", color="#1baf7a",
            label="narrow-phase")
    ax.set_xscale("log")
    ax.set_xlim(left=0.8)
    ax.set_yscale("log")
    ax.set_xlabel("K factor")
    ax.set_ylabel("Time (s, log scale)")
    ax.set_title(f"{scenario}: broad/narrow trade-off (Local Velocity)")
    ax.legend()
    ax.grid(True, alpha=0.3)

plt.tight_layout()
plt.savefig(OUTPUT_PATH, dpi=150)
print(f"Saved to {OUTPUT_PATH}")