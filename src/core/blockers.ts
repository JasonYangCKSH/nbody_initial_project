import type { Vec3 } from './types';

// 阻擋者小遊戲的純邏輯（不含繪圖），座標使用 3D 場景座標（模擬空間中心為原點）
// 約為觀眾（1.8）的 3.3 倍；站在最佳位置前 2～3 時，往上看煙火的視線才會被擋住
export const BLOCKER_HEIGHT = 6;
export const BLOCKER_RADIUS = 0.55;

export type BlockerState = 'standing' | 'falling' | 'down';
export interface Blocker {
  id: number;
  x: number;
  z: number;
  state: BlockerState;
  // falling：倒下動畫經過的秒數；down：躺著的秒數
  timer: number;
  // 決定站在視線上的哪個位置，讓多個阻擋者錯開
  slot: number;
}

type Flat = { x: number; z: number };

// 從眼睛到目標的視線是否被這個阻擋者擋住：
// 水平面上視線與阻擋者中心的距離小於半徑，且視線經過那裡時的高度低於阻擋者的頭頂
export function blocksLine(eye: Vec3, target: Vec3, blocker: Flat, groundY: number): boolean {
  const dx = target.x - eye.x, dz = target.z - eye.z;
  const len2 = dx * dx + dz * dz;
  if (len2 === 0) return false;
  const t = ((blocker.x - eye.x) * dx + (blocker.z - eye.z) * dz) / len2;
  if (t <= 0 || t >= 1) return false;
  const closestX = eye.x + t * dx, closestZ = eye.z + t * dz;
  if (Math.hypot(blocker.x - closestX, blocker.z - closestZ) > BLOCKER_RADIUS) return false;
  return eye.y + t * (target.y - eye.y) < groundY + BLOCKER_HEIGHT;
}

// 視野清晰度：沒有被任何「站著的」阻擋者擋住的目標比例（0～1）
export function viewClarity(eye: Vec3, targets: Vec3[], blockers: Blocker[], groundY: number): number {
  if (!targets.length) return 1;
  const standing = blockers.filter((b) => b.state === 'standing');
  const clear = targets.filter((target) => !standing.some((b) => blocksLine(eye, target, b, groundY))).length;
  return clear / targets.length;
}

// 阻擋者想站的位置：在玩家與煙火中心的連線上、玩家前方，依 slot 前後左右錯開
const SLOT_DISTANCE = [2.4, 3.2, 4.0, 2.8, 3.6, 4.4];
const SLOT_SIDE = [0, -0.8, 0.8, -1.6, 1.6, 0];
export function blockSpot(player: Flat, focus: Flat, slot: number): Flat {
  let dx = focus.x - player.x, dz = focus.z - player.z;
  const len = Math.hypot(dx, dz) || 1;
  dx /= len; dz /= len;
  const distance = SLOT_DISTANCE[slot % SLOT_DISTANCE.length];
  const side = SLOT_SIDE[slot % SLOT_SIDE.length];
  // (−dz, dx) 是視線方向的水平垂直方向
  return { x: player.x + dx * distance - dz * side, z: player.z + dz * distance + dx * side };
}

// 走到多近才打得到守衛（水平距離）
export const REACH = 3.5;
export const inReach = (player: Flat, blocker: Flat) => Math.hypot(player.x - blocker.x, player.z - blocker.z) <= REACH;

// 守衛站崗位置：第 j 個守衛守第 j % 發數 發煙火。每發的第一個守衛正好站在「最佳位置 → 該發煙火」的視線上
// （距離 2），所以站在最佳位置時每發都被擋住；同一發的其他守衛在視線前方兩側錯開。
// [沿視線的距離, 往側邊的距離]，全部都在攻擊範圍 REACH 以內
const GUARD_LAYOUT: [number, number][] = [[2, 0], [3, 0], [2.5, -1], [2.5, 1], [3, -1.2], [3, 1.2], [1.8, -1.8], [1.8, 1.8]];
export function guardPosts(spot: Flat, targets: Vec3[], count: number): Flat[] {
  return Array.from({ length: count }, (_, j) => {
    const target = targets[j % targets.length];
    let dx = target.x - spot.x, dz = target.z - spot.z;
    const len = Math.hypot(dx, dz) || 1;
    dx /= len; dz /= len;
    const [distance, side] = GUARD_LAYOUT[Math.floor(j / targets.length) % GUARD_LAYOUT.length];
    // (−dz, dx) 是視線方向的水平垂直方向
    return { x: spot.x + dx * distance - dz * side, z: spot.z + dz * distance + dx * side };
  });
}

// 往目標走一步（不超過目標）
export function stepToward(from: Flat, to: Flat, maxStep: number): Flat {
  const dx = to.x - from.x, dz = to.z - from.z;
  const len = Math.hypot(dx, dz);
  if (len <= maxStep || len === 0) return { x: to.x, z: to.z };
  return { x: from.x + (dx / len) * maxStep, z: from.z + (dz / len) * maxStep };
}
