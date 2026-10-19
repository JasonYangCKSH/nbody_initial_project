"""繪製四種情境下 uniform grid / octree (skin) 的 Rebuild Count 對 K 曲線 (2x2 子圖)。

用法:
    python plot_rebuild_count.py [-o rebuild_count.png]
"""

import argparse
import csv
import os

import matplotlib.pyplot as plt
from matplotlib import font_manager
from matplotlib.ticker import ScalarFormatter

HERE = os.path.dirname(os.path.abspath(__file__))

# (CSV 檔名, 子圖標題)
PANELS = [
    ("benchmark.csv", "場景一"),
    ("benchmark2.csv", "場景二"),
    ("benchmark3.csv", "場景三"),
    ("benchmark4.csv", "場景四"),
]

# (structure_mode, 圖例, 顏色, 線型)
SERIES = [
    ("uniform_grid_skin", "uniform grid", "#3b6fb6", "-"),
    ("octree_skin", "octree", "#ee8a2f", "--"),
]

CJK_FONT_FILES = [
    "/mnt/c/Windows/Fonts/msjh.ttc",  # 微軟正黑體 (WSL)
    "/mnt/c/Windows/Fonts/mingliu.ttc",
]
CJK_FONT_NAMES = ["Microsoft JhengHei", "Noto Sans CJK TC", "Noto Sans TC", "PingFang TC", "SimHei"]


def setup_font():
    for path in CJK_FONT_FILES:
        if os.path.exists(path):
            font_manager.fontManager.addfont(path)
    plt.rcParams["font.sans-serif"] = CJK_FONT_NAMES + plt.rcParams["font.sans-serif"]
    plt.rcParams["axes.unicode_minus"] = False


def load_rebuild(path):
    """回傳 {structure_mode: ([K...], [rebuild_count...])},依 K 排序。"""
    data = {}
    with open(path, newline="") as f:
        for row in csv.DictReader(f):
            if row["K"] == "NA" or row["rebuild_count"] == "NA":
                continue
            data.setdefault(row["structure_mode"], []).append(
                (int(row["K"]), int(row["rebuild_count"]))
            )
    return {mode: tuple(zip(*sorted(pts))) for mode, pts in data.items()}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("-o", "--output", default=os.path.join(HERE, "rebuild_count.png"))
    args = parser.parse_args()

    setup_font()
    fig, axes = plt.subplots(2, 2, figsize=(10, 7), sharex=True)

    for ax, (fname, title) in zip(axes.flat, PANELS):
        data = load_rebuild(os.path.join(HERE, fname))
        for mode, label, color, ls in SERIES:
            if mode not in data:
                continue
            ks, counts = data[mode]
            ax.plot(ks, counts, ls, marker="o", markersize=4, color=color, label=label)
        ax.set_xscale("log")
        ax.xaxis.set_major_formatter(ScalarFormatter())
        ax.set_title(title)
        ax.grid(True, which="both", alpha=0.3)
        ax.legend(loc="lower left", frameon=False)

    for ax in axes[:, 0]:
        ax.set_ylabel("Rebuild Count")
    for ax in axes[1, :]:
        ax.set_xlabel("K")

    fig.tight_layout()
    fig.savefig(args.output, dpi=1000)
    print(f"saved: {args.output}")


if __name__ == "__main__":
    main()
