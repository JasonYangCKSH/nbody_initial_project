// =============================================================================
// 候選配對分群：把「彼此在候選列表裡」的粒子分成一群一群，畫面用來替候選配對上色
// -----------------------------------------------------------------------------
// 相連的配對算同一群，例如配對 (1,2)、(2,5)、(7,8) → 群組 {1,2,5} 和 {7,8}。
// 大部分群組就是一對兩顆；一顆球同時和好幾顆球配對時，整群用同一個顏色。
// 不在任何配對裡的粒子不屬於任何群（-1）。
// 做法是「並查集」(union-find)：每顆粒子記一個「代表」，合併兩群就是讓其中一個代表指向另一個。
// =============================================================================
export function candidateGroups(pairs: [number, number][], count: number): { groupOf: Int32Array; groupCount: number } {
  const parent = Int32Array.from({ length: count }, (_, i) => i);
  // 往上找到代表；順便把路徑縮短（指向祖父），下次找得更快
  const find = (i: number): number => { while (parent[i] !== i) { parent[i] = parent[parent[i]]; i = parent[i]; } return i; };
  const inPair = new Uint8Array(count);
  pairs.forEach(([a, b]) => {
    inPair[a] = 1; inPair[b] = 1;
    const ra = find(a), rb = find(b);
    // 編號小的當代表：群組的代表一定是群裡最小的編號
    if (ra !== rb) parent[Math.max(ra, rb)] = Math.min(ra, rb);
  });
  // 由小到大給群組編號，所以群組 0 是「最小編號粒子所在的那群」，結果固定
  const groupOf = new Int32Array(count).fill(-1);
  const indexOfRoot = new Map<number, number>();
  for (let i = 0; i < count; i += 1) {
    if (!inPair[i]) continue;
    const root = find(i);
    if (!indexOfRoot.has(root)) indexOfRoot.set(root, indexOfRoot.size);
    groupOf[i] = indexOfRoot.get(root)!;
  }
  return { groupOf, groupCount: indexOfRoot.size };
}
