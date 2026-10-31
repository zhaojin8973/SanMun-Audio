# P4 上线部署指南（Vercel + 域名 + 统计 + 下载真链）

目标顺序：建仓库 → 绑域名 → Vercel 上线 → 接统计 → 接下载真链。

## 1. GitHub 仓库

1. 在 GitHub 建仓库 `sanmu-site`（Private 或 Public 均可；Public 更方便 GitHub Releases 分发）。
2. 把 `site/` 推上去：

```
cd "SanMu Website/site"
git init
git add .
git commit -m "SanMu 官网：Astro 双语站 + Supabase 留言区（P2/P3）"
git remote add origin https://github.com/<你>/sanmu-site.git
git push -u origin main
```

> 注意：`.env` 不要提交（已在代码里留 `.env.example`）；如建 `.gitignore`，至少包含 `.env`、`node_modules/`、`dist/`、`.astro/`。

## 2. Vercel 上线

1. https://vercel.com 用 GitHub 登录 → Add New → Project → 导入 `sanmu-site`。
2. Framework Preset 选 **Astro**（自动识别），Build Command `npm run build`，Output `dist`（一般自动填好）。
3. Environment Variables 填：
   - `PUBLIC_SUPABASE_URL`、`PUBLIC_SUPABASE_ANON_KEY`（P3 配置时已有）
   - `PUBLIC_UMAMI_WEBSITE_ID`（第 4 步拿到后补）
4. Deploy。每次 push 到 main 自动发布；预览部署也有独立链接。

## 3. 域名 sanmuaudio.com

1. 购买：推荐 Cloudflare Registrar（成本价）或 Porkbun，约 ¥60–80/年。
2. Vercel → 项目 → Settings → Domains → Add `sanmuaudio.com` 与 `www.sanmuaudio.com`（www 自动跳转主域）。
3. 按提示在域名商处改 DNS（CNAME / A 记录）；等证书自动签发。
4. 完成后到 Vercel → Domains 设置默认域，并把 `astro.config.mjs` 里的 `site` 确认为 `https://sanmuaudio.com`（已是）。
5. 域名邮箱：Zoho Mail 免费版（contact@sanmuaudio.com 转发到你的常用邮箱），按 Zoho 指引在 DNS 加 MX/TXT 记录即可。

## 4. 访问统计（Umami）

1. https://umami.is → Cloud 免费版建站，绑定 `sanmuaudio.com`，拿到 **Website ID**。
2. 把 ID 填入 Vercel 环境变量 `PUBLIC_UMAMI_WEBSITE_ID` → Redeploy。完毕（无 Cookie 弹窗）。

## 5. 下载真链（GitHub Releases）

1. 在 `sanmu-site` 仓库 Releases 里发布安装包（把 pkg/dmg/exe 拖进去即可；或建 `sanmu-releases` 独立仓库更整洁）。
2. 改 `site/src/data/downloads.ts` 的 `macos` / `windows` / `repo` 为真实直链 → push → 自动发布。

## 6. 上线检查清单（对照方案 §G）

- [ ] 全部已发布产品页内容齐全、双语无缺块
- [ ] 留言区注册 → 发留言 → 待审 → 审核 → 投票 → 导出 全链路走通
- [ ] 手机端不破版、无横向滚动
- [ ] 下载按钮直达真实安装包
- [ ] 无死链（可跑 `npx linkinator` 或手工点一遍导航/页脚）
- [ ] 无错别字（zh/en 各过一遍）
- [ ] OG 卡片在微信/Twitter/Discord 分享预览正常
- [ ] 隐私政策与留言区口径一致（已写）
- [ ] sitemap 可访问（/sitemap-index.xml）并提交到 Google Search Console

## 7. 国内提速（将来需要时）

按 `docs/01-网站方案_v1.md` §2.3 三步走：Cloudflare 优化路由 →（需要时）ICP 备案 + 国内 CDN → 国内静态镜像。技术选型已兼容，无需返工。
