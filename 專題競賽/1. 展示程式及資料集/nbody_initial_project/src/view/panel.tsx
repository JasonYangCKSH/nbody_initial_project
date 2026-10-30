// =============================================================================
// 側邊欄共用的小元件：可收合的區塊、耗時曲線
// =============================================================================
import { ChevronDown } from 'lucide-react';
import type { ReactNode } from 'react';
import type { StepMetrics } from '../core/types';
import type { Strings } from '../i18n';

export function PhaseChart({ history, metricKey, color, label, markRebuilds, t }: { history: StepMetrics[]; metricKey: 'broadPhaseMs' | 'narrowPhaseMs'; color: string; label: string; markRebuilds?: boolean; t: Strings }) {
  const width = 330;
  const height = 84;
  const points = history.slice(-40);
  if (points.length < 2) return <div className="phase-chart"><svg viewBox={`0 0 ${width} ${height}`} /></div>;
  const maxMs = Math.max(0.05, ...points.map((item) => item[metricKey]));
  const xAt = (index: number) => (index / (points.length - 1)) * width;
  const yAt = (item: StepMetrics) => height - (item[metricKey] / maxMs) * height;
  const path = points.map((item, index) => `${index === 0 ? 'M' : 'L'}${xAt(index).toFixed(1)},${yAt(item).toFixed(1)}`).join(' ');
  const latest = points.at(-1)!;
  return <div className="phase-chart">
    <svg viewBox={`0 0 ${width} ${height}`} preserveAspectRatio="none">
      <path d={path} fill="none" stroke={color} strokeWidth="1.5" />
    </svg>
    <div className="phase-chart-axis"><span>{t.axisStep(points[0].step)}</span><span>{t.axisStep(points.at(-1)!.step)}</span></div>
    <div className="phase-chart-legend"><span style={{ color }}>● {label} {latest[metricKey].toFixed(2)}ms</span>{markRebuilds && <span className={latest.rebuilt ? 'rebuild-flag on' : 'rebuild-flag'}>● {t.rebuiltThisStep(latest.rebuilt)}</span>}</div>
  </div>;
}

// 近 n 幀的平均（偵測時間每幀起伏很大，重建那一幀特別高，比較時看平均）
export const recentAverage = (history: StepMetrics[], key: 'detectionMs' | 'distanceChecks', n = 60) => {
  const recent = history.slice(-n);
  return recent.reduce((sum, item) => sum + item[key], 0) / Math.max(1, recent.length);
};

// 數據面板：重建次數放最顯眼；下面兩組各用一種顏色框起來：
//   效率指標：碰撞偵測時間、距離比對次數（附近 60 幀平均與每幀偵測時間的長條圖，重建的幀是紫色）
//   配對：候選配對數量（Verlet list size）、實際碰撞數
export function StatsPanel({ metrics, history, t }: { metrics: StepMetrics; history: StepMetrics[]; t: Strings }) {
  const bars = history.slice(-40);
  const maxMs = Math.max(0.01, ...bars.map((item) => item.detectionMs));
  return <div className="stats-panel">
    <div className="stat-hero">
      <span className="stat-hero-label">{t.rebuilds}</span>
      <strong key={metrics.rebuildCount} className="pop">{metrics.rebuildCount.toLocaleString()}</strong>
      <p>{t.rebuildsOfFrames(metrics.rebuildCount, metrics.step)}</p>
    </div>
    <div className="stat-group efficiency">
      <div className="stat-group-title">{t.groupEfficiency}</div>
      <div className="stat-row">
        <span>{t.detectionTime}</span>
        <b>{metrics.detectionMs.toFixed(2)}<i> ms</i></b>
        <small>{t.thisFrame} · {t.avg60(`${recentAverage(history, 'detectionMs').toFixed(2)} ms`)}</small>
      </div>
      <div className="stat-row">
        <span>{t.distanceChecks}</span>
        <b>{metrics.distanceChecks.toLocaleString()}</b>
        <small>{t.thisFrame} · {t.avg60(Math.round(recentAverage(history, 'distanceChecks')).toLocaleString())}</small>
      </div>
      <div className="detect-bars">
        {bars.map((item) => <span key={item.step} className={item.rebuilt ? 'rebuilt' : ''} style={{ height: `${Math.max(4, (item.detectionMs / maxMs) * 100)}%` }} />)}
      </div>
      <p className="stat-note">{t.barsNote}</p>
    </div>
    <div className="stat-group pairs">
      <div className="stat-group-title">{t.groupPairs}</div>
      <div className="stat-row"><span>{t.candidatePairs}</span><b>{metrics.candidatePairs.toLocaleString()}</b></div>
      <div className="stat-row"><span>{t.collisionsLabel}</span><b>{metrics.collisions.toLocaleString()}</b></div>
    </div>
  </div>;
}

// 劇本二的並排數據：同一組初始條件，兩種方法各一欄。重建次數、偵測時間、距離比對次數越少越好，較少的那一欄標綠色
export function CompareStats({ columns, t }: { columns: { name: string; metrics: StepMetrics; history: StepMetrics[] }[]; t: Strings }) {
  const rows: { label: string; values: number[]; format: (value: number) => string; lowerIsBetter?: boolean }[] = [
    { label: t.rebuilds, values: columns.map(({ metrics }) => metrics.rebuildCount), format: (value) => value.toLocaleString(), lowerIsBetter: true },
    { label: t.candidatePairs, values: columns.map(({ metrics }) => metrics.candidatePairs), format: (value) => value.toLocaleString() },
    { label: t.avgDetectionTime, values: columns.map(({ history }) => recentAverage(history, 'detectionMs')), format: (value) => `${value.toFixed(2)} ms`, lowerIsBetter: true },
    { label: t.avgDistanceChecks, values: columns.map(({ history }) => recentAverage(history, 'distanceChecks')), format: (value) => Math.round(value).toLocaleString(), lowerIsBetter: true },
    { label: t.collisionsLabel, values: columns.map(({ metrics }) => metrics.collisions), format: (value) => value.toLocaleString() },
  ];
  const [first, second] = columns.map(({ history }) => recentAverage(history, 'detectionMs'));
  return <div className="compare-stats">
    <table>
      <thead><tr><th />{columns.map(({ name }) => <th key={name}>{name}</th>)}</tr></thead>
      <tbody>
        {rows.map(({ label, values, format, lowerIsBetter }) => {
          const best = Math.min(...values);
          return <tr key={label}><th>{label}</th>{values.map((value, index) => <td key={index} className={lowerIsBetter && value === best && values.some((other) => other !== best) ? 'better' : ''}>{format(value)}</td>)}</tr>;
        })}
      </tbody>
    </table>
    {first > 0 && second > 0 && <p className="stat-note">{t.timeRatio(columns[1].name, columns[0].name, second / first)}</p>}
    <p className="stat-note">{t.compareNote}</p>
  </div>;
}

// 側邊欄的一個區塊：標題、一行說明；collapsible 時點標題可收合（badge 在收合時也看得到，例如事件筆數）
export function PanelSection({ index, title, hint, className = '', collapsible = false, open = true, onToggle, badge, t, children }: { index: string; title: string; hint?: string; className?: string; collapsible?: boolean; open?: boolean; onToggle?: () => void; badge?: string; t?: Strings; children: ReactNode }) {
  return <div className={`control-block ${className} ${collapsible && !open ? 'collapsed' : ''}`}>
    {collapsible ? (
      <button className="block-heading block-toggle" onClick={onToggle} aria-expanded={open} title={open ? t?.collapse : t?.expand}>
        <ChevronDown size={12} className="chevron" />{title}
        {badge && <b className="block-badge">{badge}</b>}
        <span>{index}</span>
      </button>
    ) : <div className="block-heading">{title} <span>{index}</span></div>}
    {hint && <p className="block-hint">{hint}</p>}
    {open && children}
  </div>;
}
