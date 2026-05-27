# Planet Simulation

這個專案已拆成前端與後端：

- `index.html` / `main.js`：2D 粒子模擬與渲染
- `three.html` / `main3d.js`：3D Three.js 模擬與渲染
- `server.js`：Express 後端，提供 `/api/config` 與 `/api/step`
- `api.js`：前端 API 客戶端封裝

## 目前行為

- 前端繼續負責畫面與互動
- 後端負責計算每個粒子的「下一步狀態」
- 前端每一幀向 `POST /api/step` 請求下一個模擬狀態
- 如果後端無法回應，前端會使用本地物理計算作備援

## 安裝與啟動

> 需要安裝 Node.js

```powershell
cd "c:\Users\ginga\OneDrive\Desktop\index"
npm install
npm start
```

然後開啟瀏覽器：

- `http://localhost:3000/index.html`
- `http://localhost:3000/three.html`

## 進階

- 若要調整後端物理參數，可編輯 `server.js`
- 若要新增 API，可在 `server.js` 加上新的 Express 路由
