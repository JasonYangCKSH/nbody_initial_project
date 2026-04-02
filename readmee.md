## 務實的週末計畫

---

## 核心原則：先求正確，再求快速

你現在最大的資產是 brute force，它是正確的。所有其他實作都應該以它為基準來驗證，而不是靠肉眼看輸出。

---

## 4/5 第一天：清理地基

**只做一件事：寫 verifier。**

```cpp
bool Verify(const vector<NeighborPair>& ground_truth,
            const vector<NeighborPair>& candidate) {
    // 把兩個結果都排序後比對
    // 回傳是否完全一致
}
```

brute force 的輸出就是 ground_truth。往後每次測試 uniform grid 或 octree，都跑這個 verifier。結果一致才算正確，沒有商量餘地。

這一天結束你應該有：可靠的 verifier，以及對 brute force 輸出的完整信心。

---

## 4/6 第二天：從零手寫 Uniform Grid

**不要看 AI 寫的舊版本，重新寫。**

按照這週討論的邏輯，依序實作：

第一步，BodyToCell，用 floor 計算格子座標。

第二步，HashCell，先用最簡單的質數 hash，不要過度設計。

第三步，建 flat array，sort + cellStart。

第四步，27 格掃描，加 Narrow Phase 距離篩選。

每完成一步就用 verifier 測，不要等全部寫完才測。失敗時縮小測資，從 3 個 body 開始，不要一上來就測幾千個。

這一天結束你應該有：通過 verifier 的 uniform grid。

---

## 4/7 第三天：Benchmark

用計時器比較 brute force 和 uniform grid 在不同 N 下的時間。

```
N = 100, 500, 1000, 5000, 10000
```

畫出或記錄數字，確認 uniform grid 的成長斜率明顯低於 brute force。這才是真正的驗收。

octree 這個週末先不碰，把一件事做扎實比三件事做一半有意義。

---

## 關於不依賴 AI 的原則

遇到卡住，先問自己這三個問題再來找 AI：

第一，我能不能用紙筆畫出預期的記憶體狀態？

第二，我有沒有縮小測資到最小可重現的錯誤？

第三，我有沒有在關鍵步驟加 assert 或 print 確認中間結果？

這三個問題能解決 80% 的 bug。剩下 20% 再來討論。

---

## 最後一句話

verifier 是你這個週末最重要的工具。有了它，你的進展是可以被客觀量測的，而不是靠感覺。