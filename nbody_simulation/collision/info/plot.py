#!/usr/bin/env python3
"""
繪製「K 值取捨」三聯圖：重建次數、每次重建的平均候選配對數、總執行時間。

用法：
    python plot_k_tradeoff.py benchmark.csv
    python plot_k_tradeoff.py benchmark.csv -o 圖B_K值取捨.png
    python plot_k_tradeoff.py benchmark.csv --no-annotate

CSV 需包含欄位：structure_mode, K, rebuild_count, avg_candidate_per_rebuild, total_time_avg_s
只會使用 structure_mode 為 uniform_grid_skin 與 octree_skin 的列（K 為 NA 的列會被忽略）。
"""
import argparse
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.ticker
from matplotlib import font_manager
import pandas as pd

# ---------- 樣式設定 ----------
UG_COLOR = "#3D63A8"   # Uniform Grid（藍）
OT_COLOR = "#B5652A"   # Octree（橘）
INK = "#222222"
MUTED = "#6b6b6b"
GRID = "#e6e6e3"

SERIES = [
    # (structure_mode, 顏色, 圖例名稱, 標記)
    ("uniform_grid_skin", UG_COLOR, "UG + Verlet", "o"),
    ("octree_skin",       OT_COLOR, "OT + Verlet", "s"),
]

PANELS = [
    # (欄位, 子圖標題, y 軸是否用對數)
    ("rebuild_count",             "重建次數",                 False),
    ("avg_candidate_per_rebuild", "每次重建的平均候選配對數", True),
    ("total_time_avg_s",          "總執行時間（秒）",         False),
]


# 直接以檔案路徑載入的中文字型（WSL 可直接讀取 Windows 的字型）
FONT_FILES = [
    "/mnt/c/Windows/Fonts/msjh.ttc",                               # WSL：微軟正黑體
    "C:/Windows/Fonts/msjh.ttc",                                   # Windows：微軟正黑體
    "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",      # Ubuntu：fonts-noto-cjk
    "/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc",
    "/System/Library/Fonts/PingFang.ttc",                          # macOS
]


def pick_cjk_font():
    """先以檔案路徑註冊字型，再依名稱尋找，避免 matplotlib 快取找不到新裝的字型。"""
    for path in FONT_FILES:
        if Path(path).exists():
            try:
                font_manager.fontManager.addfont(path)
                return font_manager.FontProperties(fname=path).get_name()
            except Exception:
                pass
    candidates = [
        "Noto Sans CJK TC", "Noto Sans TC", "Microsoft JhengHei",
        "PingFang TC", "Heiti TC", "Noto Sans CJK JP", "WenQuanYi Zen Hei",
    ]
    installed = {f.name for f in font_manager.fontManager.ttflist}
    for name in candidates:
        if name in installed:
            return name
    print("警告：找不到中文字型，中文會顯示為方塊。\n"
          "  Ubuntu/WSL：sudo apt install fonts-noto-cjk && rm -rf ~/.cache/matplotlib")
    return None


def load(csv_path):
    df = pd.read_csv(csv_path, dtype={"K": str})
    df = df[df["structure_mode"].isin([s[0] for s in SERIES])].copy()
    df = df[df["K"].str.upper() != "NA"]
    df["K"] = df["K"].astype(float)
    return df.sort_values("K")


def style_axis(ax):
    ax.grid(axis="y", color=GRID, lw=0.6)
    ax.set_axisbelow(True)
    for side in ("top", "right"):
        ax.spines[side].set_visible(False)


def plot(df, out_path, annotate=True):
    font = pick_cjk_font()
    plt.rcParams.update({
        "font.family": [font] if font else plt.rcParams["font.family"],
        "axes.unicode_minus": False,
        "axes.edgecolor": "#9a9a9a", "axes.linewidth": 0.6,
        "xtick.color": "#555", "ytick.color": "#555",
        "axes.titleweight": "bold",
    })

    fig, axes = plt.subplots(1, 3, figsize=(9, 2.9), dpi=300)

    for ax, (col, title, logy) in zip(axes, PANELS):
        for mode, color, label, marker in SERIES:
            d = df[df["structure_mode"] == mode]
            ax.plot(d["K"], d[col], color=color, lw=2, marker=marker, ms=4, label=label)
        ax.set_xscale("log")
        if logy:
            ax.set_yscale("log")
            ax.yaxis.set_minor_formatter(matplotlib.ticker.NullFormatter())
            # 資料範圍不到兩個數量級時，改用 1、2、5 刻度並顯示一般數字，避免只剩一個刻度
            vals = df[col]
            if vals.max() / max(vals.min(), 1e-12) < 100:
                ax.yaxis.set_major_locator(matplotlib.ticker.LogLocator(subs=(1, 2, 5)))
                ax.yaxis.set_major_formatter(
                    matplotlib.ticker.FuncFormatter(lambda v, _: f"{v:,.0f}"))
        ax.set_title(title, fontsize=10)
        ax.set_xlabel("K", fontsize=9)
        ax.tick_params(labelsize=8)
        style_axis(ax)

    # 重建次數從 0 開始，上限略高於最大值
    max_rebuild = df["rebuild_count"].max()
    axes[0].set_ylim(0, max_rebuild * 1.05)
    axes[2].set_ylim(0, df["total_time_avg_s"].max() * 1.08)

    # 標註 Octree 的最佳 K
    if annotate:
        ot = df[df["structure_mode"] == "octree_skin"]
        best = ot.loc[ot["total_time_avg_s"].idxmin()]
        k, t = best["K"], best["total_time_avg_s"]
        ymax = axes[2].get_ylim()[1]
        # 最佳點在下半部就把文字放上方，反之放下方，避免壓到曲線
        text_y = 0.85 if t < ymax / 2 else 0.15
        axes[2].annotate(
            f"OT 最佳：K={k:g}\n{t:.2f} 秒",
            xy=(k, t), xytext=(0.58, text_y), textcoords="axes fraction",
            fontsize=8, color=INK,
            arrowprops=dict(arrowstyle="->", color=MUTED, lw=0.8),
        )

    handles, labels = axes[0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="upper center", ncol=2, frameon=False,
               fontsize=9, bbox_to_anchor=(0.5, 1.0))
    fig.tight_layout(rect=(0, 0, 1, 0.9))
    fig.savefig(out_path, facecolor="white")
    plt.close(fig)
    print(f"已輸出：{out_path}")


def main():
    ap = argparse.ArgumentParser(description="繪製 K 值取捨三聯圖")
    ap.add_argument("csv", help="benchmark CSV 檔路徑")
    ap.add_argument("-o", "--output", help="輸出圖檔路徑（預設為 CSV 同名 _K取捨.png）")
    ap.add_argument("--no-annotate", action="store_true", help="不標註 Octree 最佳 K")
    args = ap.parse_args()

    out = args.output or str(Path(args.csv).with_name(Path(args.csv).stem + "_K取捨.png"))
    plot(load(args.csv), out, annotate=not args.no_annotate)


if __name__ == "__main__":
    main()