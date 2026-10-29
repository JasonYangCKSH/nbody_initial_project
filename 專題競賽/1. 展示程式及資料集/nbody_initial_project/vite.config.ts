import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

// 發布到 GitHub Pages 時，網站位於 https://<帳號>.github.io/<儲存庫名稱>/，
// 所以建置時要用 ./ 相對路徑，讓網頁不論放在哪個子路徑下都找得到 JS、CSS。
// 本機開發（npm run dev）不受影響。
export default defineConfig({
  base: './',
  plugins: [react()],
});
