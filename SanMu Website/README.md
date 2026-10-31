# SanMu Website（三牧音频官网）

SanMu Audio 插件套件的官方宣发网站：展示产品（EQ6 / Comp / Reverb / Delay）、下载分发、用户使用体验留言区。

## 当前阶段

**阶段 2：P2 站点骨架（进行中）** —— P1 六个原型全部完成并实测通过；**Astro 正式工程已搭好并构建通过**（20 个页面，中英双语）。

### P1 原型（`prototypes/`，双击可看）
- `p1-home-v1.html` 单屏首页 · `p1-eq6-v1.html` EQ6 产品页 · `p1-comp-v1.html` Comp 产品页 · `p1-coming-soon-v1.html` Coming Soon（Reverb/Delay 双变体）· `p1-about-v1.html` About 页
- `DESIGN.md` —— 网站设计规范（token / 几何 / 动效 / 组件 / 红线），P2 已按其实现

### P2 Astro 工程（`site/`）
- 技术：Astro 5 静态站 + @astrojs/preact + @astrojs/sitemap + @supabase/supabase-js；i18n 路由 `/zh`（默认）+ `/en`
- 页面：双语首页（单屏舞台）、EQ6/Comp 产品页、Reverb/Delay Coming Soon、About、FAQ、Changelog、Privacy、404、`/admin` 管理后台、根路径跳转（共 21 页）
- 组件：`Base` 布局、`Nav`/`Footer`、`EQ6Mockup`/`CompMockup`、`CommentSection`（P3 留言组件）、`CommentWidget`（Preact island：注册登录/发留言/投票/官方回复）、`AdminApp`（待审队列/状态标记/置顶/删除/CSV 导出）
- 统一口径：`src/data/downloads.ts`（下载链接 / 试用 15 天 / 定价）
- 命令：`cd site && npm run dev`（开发）/ `npm run build`（构建）/ `npm run preview`（预览）
- P4 已就绪：OG 分享图（og-default.png 2x）、apple-touch-icon、JSON-LD（Organization/WebSite/Product）、Umami 条件接入、`DEPLOY.md` 上线指南、`.gitignore`
- 待办：下载链接接 GitHub Releases 真链、英文文案待你找 agent 审校

### P3 留言区（`site/supabase/`）—— 代码就绪，等你创建 Supabase 项目
- `migrations/0001_init.sql`：profiles / comments / votes / feedback_subscriptions 四表 + RLS + 首发言审核触发器 + 投票 RPC
- `functions/notify-admin/`：Resend 邮件通知 Edge Function（Webhook 触发）
- `SETUP.md`：云端 15 分钟配置指南（建项目 → 跑 SQL → 关邮箱验证 → 填 .env → 设管理员）
- 未配置 Supabase 时站点照常构建，留言区自动降级为示意态
- **本地联调环境（当前运行中）**：Docker + `supabase start` 本地栈 + `site/.env` 指向 `127.0.0.1:54321`；预览站 http://localhost:4322/zh/ 的留言区/订阅/管理后台可真实使用；停止：`cd site && npx supabase stop`

下一步：等你验收（原型 + `http://localhost:4322/zh/`）→ 发截图做可动动画 → P3 留言区（Supabase）。

路线图（每阶段你验收）：

| 阶段 | 内容 |
|---|---|
| P0 需求冻结 ✅ | 问卷 + 方案 v1 + 域名 sanmuaudio.com |
| P1 设计稿 ✅ | 六页原型 + 入场动效；EQ6 按实机截图 1:1 重绘（双主题 + Light/Dark 切换 + 全套动画）；Comp 双主题动画同款就绪（等实机截图换 1:1） |
| P2 站点骨架 ✅ | Astro 双语工程 21 页，构建通过；下载真链/OG 图待 P4 |
| P3 留言区 ✅ | 本地 Supabase 全链路联调通过（注册→留言→审核→投票→订阅）；云端建项目后填 .env 即用（`supabase/SETUP.md`） |
| P4 上线打磨 🚧 | OG 图/JSON-LD/Umami 接入/部署指南已就绪；等你建仓库与 Vercel 账号执行 `site/DEPLOY.md` |
| P5 后置 | 购买/授权码（15 天试用）、伙伴计划深化 |

## 已安装工具

- **awesome-design-md**（参考库）：`reference/awesome-design-md/` —— 74 份真实品牌 DESIGN.md 设计规范（apple / notion / elevenlabs / spotify / stripe 等），用于提取可借鉴的设计元素。
- **@fce/design-md**（CLI，全局安装）：`design-md` 命令，可交互式地把参考 DESIGN.md 拉进项目目录。

## 网站设计方向（已确认口径）

- 视觉基底：Quantum² 参考稿结构（白底、黑胶囊导航、两行大标题、全宽色带、产品卡片横带、入场编排），**不复刻插件玻璃拟物**。
- 品牌色：薄荷绿 `#34D399`（参考稿青色 `#38c6ec` 的替换）；字体 Figtree（拉丁）+ PingFang SC（中文）；Light 单主题。
- 气质：干净通透的"数字 EQ"。
- 插件玻璃拟物规范（`../SanMu 资料库/02-UI设计规范.md`）仅用于产品截图/动画重绘的保真参考，网站本身不套用。
