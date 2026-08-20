import csv
import matplotlib.pyplot as plt
from collections import defaultdict

LV_CSV_PATH = "bench_results_local_velocity.csv"
FR_CSV_PATH = "bench_results_fixed_radius.csv"
NONE_CSV_PATH = "bench_results_none.csv"
OUTPUT_PATH = "k_sweep_plot.png"


def load_and_clean(path):
    data = defaultdict(list)
    with open(path, newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            if row["K"] == "radius":
                continue
            K = int(row["K"])
            total_s = float(row["total_s"])
            broadphase_s = float(row["broadphase_s"])
            narrowphase_s = float(row["narrowphase_s"])
            broadphase_execs = int(row["broadphase_execs"])
            data[row["scenario"]].append(
                (K, total_s, broadphase_s, narrowphase_s, broadphase_execs)
            )
    for scenario in data:
        data[scenario].sort(key=lambda r: r[0])
    return data


def unpack(rows):
    K = [r[0] for r in rows]
    total_s = [r[1] for r in rows]
    broadphase_s = [r[2] for r in rows]
    narrowphase_s = [r[3] for r in rows]
    broadphase_execs = [r[4] for r in rows]
    return K, total_s, broadphase_s, narrowphase_s, broadphase_execs


lv = load_and_clean(LV_CSV_PATH)
fr = load_and_clean(FR_CSV_PATH)
none = load_and_clean(NONE_CSV_PATH)

scenarios = list(lv.keys())
n = len(scenarios)

fig, axes = plt.subplots(n, 3, figsize=(16, 5 * n))
if n == 1:
    axes = axes.reshape(1, -1)

for i, scenario in enumerate(scenarios):
    lv_K, lv_total, lv_bp, lv_np, lv_execs = unpack(lv[scenario])
    fr_K, fr_total, fr_bp, fr_np, fr_execs = unpack(fr[scenario])
    none_K, none_total, none_bp, none_np, none_execs = unpack(none[scenario])

    ax = axes[i, 0]
    ax.plot(lv_K, lv_total, marker="o", color="#2a78d6", label="Local Velocity (dynamic skin)")
    ax.plot(fr_K, fr_total, marker="s", color="#eb6834", label="Fixed Radius (skin = radius)")
    ax.plot(none_K, none_total, marker="^", color="#8a8a8a", linestyle="--", label="None (no Verlet buffer)")
    ax.set_xscale("log")
    ax.set_xlim(left=0.8)
    ax.set_xlabel("K factor")
    ax.set_ylabel("Total time (s)")
    ax.set_title(f"{scenario}: total time vs K")
    ax.legend()
    ax.grid(True, alpha=0.3)

    ax = axes[i, 1]
    ax.plot(lv_K, lv_execs, marker="o", color="#2a78d6", label="Local Velocity")
    ax.plot(fr_K, fr_execs, marker="s", color="#eb6834", label="Fixed Radius")
    ax.plot(none_K, none_execs, marker="^", color="#8a8a8a", linestyle="--", label="None (no Verlet buffer)")
    ax.set_xscale("log")
    ax.set_xlim(left=0.8)
    ax.set_xlabel("K factor")
    ax.set_ylabel("Broad-phase executions")
    ax.set_title(f"{scenario}: rebuild count vs K")
    ax.legend()
    ax.grid(True, alpha=0.3)

    ax = axes[i, 2]
    ax.plot(lv_K, lv_bp, marker="o", color="#eb6834", label="broad-phase")
    ax.plot(lv_K, lv_np, marker="^", color="#1baf7a", label="narrow-phase")
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
