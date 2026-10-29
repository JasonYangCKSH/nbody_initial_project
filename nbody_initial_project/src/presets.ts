// =============================================================================
// Demo 劇本：一鍵套用預先調好的設定，現場不用手動調參數
// =============================================================================
import { DEFAULT_ACCELERATION, DEFAULT_SPEED, PARTICLE_RADIUS, type Scene } from './core/ParticleSystem';
import type { SimConfig } from './core/SimulationRunner';

export type ScriptId = 'micro' | 'compare' | 'octree';

export interface Script {
  id: ScriptId;
  config: SimConfig;
  playbackSpeed: number;  // 播放速度（0.5 = 慢動作）
  pairColors: boolean;    // 候選配對上色
  dual?: boolean;         // 上下兩個畫面：上用 config 的方法（均勻網格），下用八元樹，同一組初始條件
  showOctree?: boolean;   // 一開始就畫出八元樹切割
  allSplits?: boolean;    // 每一層已切開的格子都用虛線畫出來（不必先選層級）
  colorMode?: 'plain' | 'rich';
  autoRotate?: boolean;   // 鏡頭自動繞著轉，看得出 3D 結構
}

// 劇本一：Verlet list 概念（微觀、證明正確性）
//   邊長 8 的小空間、35 顆大球（半徑 0.35）、等速運動（沒有加速度）、0.5× 慢動作、候選配對上色。
//   第 0、1 顆是焦點球：約第 25 步變成候選配對、第 37 步正面相撞並反彈（見 scenario.ts 的 microScene）。
//   K = 12：等速運動時，每顆球大約走 12 步就走到自己的 skin 邊緣，所以大約每 12 步重建一次（碰撞改變速度時會提早）。
//   八元樹在球少時葉節點很大，skin 上界幾乎不會截斷 skin，K 和重建間隔的關係最清楚
export const MICRO_SCRIPT: Script = {
  id: 'micro',
  config: { scene: 'micro', count: 35, worldSize: 8, radius: 0.35, speed: 1.5, clusterFactor: 0, clusters: 1, acceleration: 0, seed: 7, algorithm: 'Octree', bufferEnabled: true, K: 12, leafCapacity: 8, maxDepth: 6 },
  playbackSpeed: 0.5,
  pairColors: true,
};

// 劇本二：方法比較（巨觀）
//   上下兩個同步畫面、同一組初始條件：上均勻網格、下八元樹。兩邊找到的碰撞完全相同，所以粒子的運動一模一樣，
//   差別只在「找候選配對」花的功夫。參數與 C++ 實驗一致：邊長 25、半徑 0.075、格長 0.3、葉容量 8、最大深度 6。
//   3,000 顆粒子、群聚（異質密度）、有加速度；畫面上可以即時調整 K、切換均勻／群聚、開關加速度
export const COMPARE_CLUSTER_FACTOR = 0.6;
export const COMPARE_ACCELERATION = 0.5;
export const COMPARE_SCRIPT: Script = {
  id: 'compare',
  config: { scene: 'cloud', count: 3000, worldSize: 25, radius: PARTICLE_RADIUS, speed: DEFAULT_SPEED, clusterFactor: COMPARE_CLUSTER_FACTOR, clusters: 3, acceleration: COMPARE_ACCELERATION, seed: 17, algorithm: 'Uniform Grid', bufferEnabled: true, K: 12, leafCapacity: 8, maxDepth: 6 },
  playbackSpeed: 1,
  pairColors: true,
  dual: true,
};

// 劇本三（選做）：八元樹結構
//   150 顆粒子擠成 2 團、葉節點容量 6（超過 6 顆就切成 8 格）、最大深度 5，邊長 12。
//   密集處切到第 4、5 層的小格子，稀疏處停在淺層的大格子；不同層用不同顏色，葉節點實線、已切開的格子虛線，鏡頭自動旋轉。
//   半徑 0.08：群聚中心很擠，球太大一開始會大量重疊、整團都是碰撞色。
//   最大深度 5 時最小的葉節點半邊長 0.1875 仍大於半徑，skin 上界不會變成 0
export const OCTREE_SCRIPT: Script = {
  id: 'octree',
  config: { scene: 'cloud', count: 150, worldSize: 12, radius: 0.08, speed: 0.5, clusterFactor: 0.7, clusters: 2, acceleration: 0, seed: 17, algorithm: 'Octree', bufferEnabled: true, K: 12, leafCapacity: 6, maxDepth: 5 },
  playbackSpeed: 1,
  pairColors: false,
  showOctree: true,
  allSplits: true,
  colorMode: 'rich',
  autoRotate: true,
};

export const SCRIPTS: Script[] = [MICRO_SCRIPT, COMPARE_SCRIPT, OCTREE_SCRIPT];

// 切換場景時，空間大小、半徑、速度跟著換成該場景適合的值；粒子數不在合理範圍才調整
const SCENE_DEFAULTS: Record<Scene, Pick<SimConfig, 'worldSize' | 'radius' | 'speed'> & { count: number; minCount: number; maxCount: number }> = {
  cloud: { worldSize: 25, radius: PARTICLE_RADIUS, speed: DEFAULT_SPEED, count: 1200, minCount: 200, maxCount: 50000 },
  explosion: { worldSize: 25, radius: PARTICLE_RADIUS, speed: DEFAULT_SPEED, count: 1200, minCount: 200, maxCount: 50000 },
  micro: { worldSize: 8, radius: 0.35, speed: 1.5, count: 35, minCount: 2, maxCount: 150 },
};

export const withScene = (config: SimConfig, scene: Scene, clusters: number): SimConfig => {
  const { count, minCount, maxCount, ...defaults } = SCENE_DEFAULTS[scene];
  return {
    ...config, ...defaults, scene, clusters,
    count: config.count < minCount || config.count > maxCount ? count : config.count,
    acceleration: scene === 'micro' ? 0 : config.acceleration || DEFAULT_ACCELERATION,
  };
};
