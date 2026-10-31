# SanMu Website — DESIGN.md

> SanMu Audio 官网设计规范。结构工艺以 Quantum² 参考稿为基准，配色换为 SanMu 薄荷绿。
> 状态：v1（对应原型 `prototypes/p1-home-v1.html`，2026-09-02 实测验证）。改数值必须同步改原型与本文。
> 权威顺序：本文 + 原型代码 ＞ `docs/01-网站方案_v1.md` ＞ 插件玻璃拟物规范（网站不复刻玻璃拟物，仅色相延续）。

## 0. 定位与气质

- 网站气质：**干净通透的数字 EQ**——克制、精确、无染色。白底为主，彩色只做功能强调。
- 品牌主色：薄荷绿 `#34D399`（由参考稿青色 `#38c6ec` 替换而来），深色变体 `#0f766e`（色带渐变末端）。
- 字体：**Figtree 可变字体**（100–900，`display=block`，Google Fonts 加载）+ 中文 PingFang SC（随系统栈回退）。禁止用 Inter 作主字体。
- 主题：仅 Light。网站不复刻插件的玻璃拟物与双主题。
- Logo：三根圆角竖条（"三"字声波标，原创），薄荷绿填充；wordmark = `SanMu`（黑 783 字重）+ 上标 `AUDIO`（薄荷绿、0.10em 字距、上移 6px）。

## 1. 设计 token

### 颜色
| Token | 值 | 用途 |
|---|---|---|
| `--c-ink` | `#000000` | 主文本 |
| `--c-ink-soft` | `#2e2e2e` | 激活 tab 文本 |
| `--c-muted` | `#b8b8b8` | 元信息、闲置 tab |
| `--c-on-dark` | `#ffffff` | 深底上的文本 |
| `--c-on-light` | `#000000` | 浅底上的文本 |
| `--c-surface-nav` | `#000000` | 黑胶囊导航 |
| `--c-surface-card` | `#f6f8f7` | 产品卡片 |
| `--c-surface-panel` | `#ffffff` | 卡片内白色面板 |
| `--c-border-row` | `#ededed` | 行边框 |
| `--c-accent` | `#34D399` | 品牌强调 |
| `--c-accent-deep` | `#0f766e` | 强调色深变体（渐变末端、hover） |
| 页面底色 | `#fff` + 顶角极淡薄荷/天蓝氛围光（radial 5–7%） | #stage 背景 |

### 字体阶梯（桌面基准，卡片内按 `--dp` 缩放）
| Token | 值 | 字重 | 字距 | 行高 |
|---|---|---|---|---|
| `--fs-display` | 55.7px（hero h1） | 470 | `-0.0572em` | 61px |
| `--fs-title` | `--dp`×20 | 388 | `-0.0570em` | 1 |
| `--fs-heading` | `--dp`×17 | 511 | `-0.0200em` | 1 |
| `--fs-body` | 16.1px | 470 | `-0.0475em` | 20px |
| `--fs-eyebrow` | `--dp`×15.8 | 388 | `-0.0293em` | 1 |
| `--fs-ui` | 15px | 500（链接）/577（CTA） | `-0.0080em` / `-0.0200em` | 1 |
| `--fs-meta` | `--dp`×14.3 | 388 | `-0.0447em` | 1 |
| `--fs-item` | `--dp`×12.9 | 500 | `-0.0287em` | 1 |
| `--fs-caption` | `--dp`×12.2 | 388 | `-0.0295em` | 1 |
| `--fs-tab` | `--dp`×12.1 | 388 / 500(激活) | `-0.0398em` | 1 |
| `--fs-wordmark` | 17.1px | 783 | `-0.0075em` | 1 |
| 上标 | 11.4px | 470 | `0.10em` | — |

字重实例（可变字体实数）：light 388 · regular 470 · medium 500 · semibold 511 · bold 577 · black 783。**不新增字号/字重/字距档位。**

### 几何（桌面舞台 1290×860，等比缩放）
| 元素 | 位置（舞台坐标） | 尺寸 |
|---|---|---|
| 黑胶囊导航 | left 50%−439, top 43 | 880×52, 圆角 26 |
| 导航链接 l1–l4 | left 279 / 386.5 / 483 / 571（translateX(−50%) 居中） | 高 52 |
| 导航 CTA（白色胶囊） | left 718, top 9 | 153×34, 圆角 17 |
| hero h1 | top 203+hero-dy，左右通栏居中 | 两行, pre-line |
| hero sub | top 333+hero-dy | 两行 |
| hero CTA（黑色胶囊） | left 50%−64, top 388+hero-dy | 129×34, 圆角 17 |
| 色带 .band | top 473+band-dy 至 stage 底，满宽 | 薄荷绿→深青 135° 渐变 + 细纹网格(8%) + 顶部柔光 |
| EQ6 示意图 | band 内 left 60, top **80**+band-dy | 600×332 |
| 产品卡片 | band 内 left 682, top **76**+band-dy | 548×340, 圆角 16 |

⚠️ **mockup 与 card 在 `.band` 内部，top 是相对色带顶（473）的坐标**：舞台 553 = 80；舞台 549 = 76。改舞台坐标时两个都要换算。

### 卡片（548×340 设计像素，全部长度 = `--dp`×设计值）
- 投影：白描边 1px + 白色光晕 + `0 22dp 64dp rgba(2,72,105,.16)`。
- 内件：eyebrow(19,21) / title(18,47) / meta(15,81) / tabs(顶部 129，高 30) / panel(19,160, 511×169)。
- 激活 tab：白底、上圆角 8、底缘薄荷绿 2px 下划线；闲置 tab 灰色。
- 行：485×46，白底、`#ededed` 1px 边框、圆角 8；圆点 15×15（绿 `#34D399`/深青 `#0f766e`）。
- 卡片与示意图在色带上向右下越界出血，底部裁切是构图的一部分（不是 bug）。

### 动效 token
- 缓动：reveal `cubic-bezier(.22,1,.36,1)` · settle `cubic-bezier(.16,1,.30,1)`。
- 时长：nav .62s · navitem .5s · head .92s · copy .62s · cta .58s · panel .94s。
- **只动独立 `translate` 属性，绝不覆盖 `transform`**（stage 缩放、链接居中、卡片缩放都占着 transform）。
- 编排（data-enter=pending→run）：nav 下沉 .05s → logo/wordmark/链接 .18–.42s 渐显 → h1 clip-path 擦除 .30s → sub .62s → CTA .78s → mockup .82s → card .88s；结束（animationend/3s 兜底）置 `done` 清掉入场样式。
- `prefers-reduced-motion: reduce`：完全跳过入场动画，直接终态。
- 滚动触发类动效（产品页演示动画）后续按同一套缓动设计。

### 响应式断点
- 桌面 ≥940px：1290×860 舞台 + fit JS（`s = min(vh/860, vw/960)`；卡片缩放 `cs = max(1, min((H−549)/340, 880/548))`；`dy = max(0, H−549−340cs)`；hero 分 55% dy）。
- <940px：CSS Grid 单列（nav/head/sub/cta/art），band 内 flex 列：mockup `min(84vw,480px)` 等比 + card `--u = min(86vw,548px)` 上叠 `−40dp`；无滚动条（竖屏单屏构图）。
- <480px：hero 标题 `white-space: normal + text-wrap: balance`。
- 横屏矮视口（≤520px 高）：允许滚动，stage 恢复静态高度。

## 2. 组件速查

| 组件 | 规格 | 状态 |
|---|---|---|
| 黑胶囊导航 + 汉堡菜单 | 菜单从胶囊下方展开（`hidden` 属性驱动状态，opacity/transform 过渡），点击外/ESC 关闭 | ✅ 原型已实现 |
| 产品卡片 tabs | 纯展示切换（标题/元信息/两行特性），激活态白底+薄荷绿下划线 | ✅ 原型已实现 |
| EQ6 界面示意 | SVG 占位（频响曲线+六段旋钮+OUT 表，色值按插件频段色） | ⏳ 等 REAPER 截图后重绘为可动版 |
| 语言切换 / 登录 | 原型为静态链接；P2 接 i18n 路由，P3 接 Supabase Auth | ⏳ 规划 |

## 3. 文案口径（主页）

- hero 标题（两行 pre-line）：`把每一次混音\n变得干净、通透、精准`
- sub：`六段参量 EQ 与四模式压缩现已可用，混响与延迟即将到来。\n免费试用十五天，满意再购买。`
- CTA：`免费试用`（hero）/ `立即下载`（导航）。
- 卡片数据：EQ6/Comp 已发布；Reverb Coming Soon；Delay「免费 · Coming Soon」。
- 英文版 P2 由我代译、用户另找英语 agent 审校。

## 6. 产品演示动画（`AnimatedEQ6.astro`，v1 示意版）

- 结构：`EQ6Mockup.astro` 的 SVG 加动画钩子（`.aeq-curve` 主曲线 / `.aeq-curve-glow` 光晕 / `.aeq-dot-0..5` 频点 / `.aeq-knob-0..5` 旋钮弧 / `.aeq-meter-l/r` OUT 表 / `.aeq-sheen` 扫光），外包 `.aeq6-small` / `.aeq6-full` 决定用哪套关键帧。
- 动画（全部 CSS 关键帧，`translate`/`rotate`/`d`/`cy`/`scaleX`，不碰布局）：
  - 频响曲线 16s 三形态渐变（平坦 → 人声塑形 → 反相起伏），六频点 `cy` 同步跟随；
  - 六旋钮弧 ±14° 转动，周期 4.2–5.8s、相位错开；
  - OUT 表 `scaleX` 0.5↔0.88（L 3.3s / R 3.9s）；
  - 扫光 9s 横穿玻璃面板；曲线光晕 5s 呼吸；
  - `prefers-reduced-motion`：全部静态、扫光隐藏。
- 使用位置：首页色带（small）、EQ6 产品页界面区（full）；Hero 保持静态。
- **双主题参数化**：`EQ6Mockup.astro` 接受 `theme` prop，调色板集中在 `palettes`（light/dark 两套，含面板渐变、文字、网格、胶囊、工具条、曲线）；渐变 id 按 `名称-variant-theme` 唯一化避免同页冲突。
- 产品页界面区有 **Light / Dark 切换按钮**（双实例切换显示），Dark 稿参照用户实拍深色截图的灰色系。
- **待替换**：实机截图到位后按真实 UI 1:1 重绘（旋钮行程、曲线锚点、玻璃分层保真），动画骨架不变。Comp 页同样待做（`CompMockup` 加钩子即可复用本方案）。

## 4. 红线

- 不引入新字号/字重档位；颜色只用 token，禁止内联十六进制（SVG 插图除外）。
- 入场动效只用 `translate`；不给 band/视频加动画。
- 不复制参考稿的商标图形、视频素材与英文文案；内容全部为 SanMu 原创或授权。
- 双端验收视图：桌面 1512×982（≈1290×860 等比）与手机 390×844。

## 5. 产品页（EQ6 模板，对应 `prototypes/p1-eq6-v1.html`）

- **页面形态**：普通滚动长页（非单屏舞台），容器 `.wrap` max-width 1080 / 边距 40（移动 20）。
- **导航**：同黑胶囊 880×52，`position: sticky; top: 16px`，移动端满宽 + 汉堡（菜单同首页）。
- **区块顺序**：Hero（左文右图 46/54 分栏）→ 特性（3 列网格 6 卡）→ 界面（薄荷绿浅色带内大图）→ 系统需求（2 列清单）→ 版本历史（版本号列 + 列表行）→ 用户评价（3 列引用卡）→ FAQ（details 手风琴）→ 下载 CTA（全宽薄荷绿渐变圆角带）→ 留言区示意（P3 实装）→ 页脚（wordmark + 链接 + ©）。
- **入场动效**：区块 `.reveal` 由 IntersectionObserver 触发（threshold 0.12，一次性），`translate 0 14px → 0` + opacity .62s settle 缓动；`prefers-reduced-motion` 直接终态。首页的 load 编排只属于单屏首页。
- **区块标题**：序号 `01/02…`（caption 号 12.2 深青）+ h2 用 `--fs-title`（20px / 388 轻字重）。
- **卡片 hover**：`translate 0 -2px` + 描边薄荷绿 55% + 投影（不换 transform）。
- **留言区示意组件**：登录提示条 → 类型 chips（需求/Bug/反馈/闲聊，选中态薄荷绿 12%）→ textarea → 发布按钮；留言卡 = 昵称 + 类型徽章 + 状态徽章（已采纳/已修复）+ 时间 + 正文 + ▲票数 + 官方回复（左缘 2px 薄荷绿引用条）。**徽章/回复/票数即 P3 的视觉基准。**
- **下载 CTA**：`免费试用 15 天` + macOS/Windows 双按钮（GitHub Releases 直链）+ 说明"购买环节即将上线"（购买暂缓口径）。
- **Hero 视觉**：EQ6 SVG 示意（600×332 比例），等截图后替换为可动版。
- **Comp 页**（`prototypes/p1-comp-v1.html`）沿用同一模板；Hero/界面示意按 820×570 绘制：顶部 IN/GR/OUT 三表（GR 黄 `#e8c547`、峰值告警粉 `#ff7875`）、四模式按钮（Clean `#1E88E5` / VCA `#2058A8` / FET `#E64A19` / Opto `#D49947`，激活实底、闲置 12–14% 淡底 + 描边）、五旋钮（Threshold/Ratio/Attack/Release/Makeup，弧环用当前模式色）。示意 SVG 的频段色/模式色按插件规范取值。
- **Coming Soon 页**（`prototypes/p1-coming-soon-v1.html`）：居中排版——脉冲点 COMING SOON 徽章（Delay 变体文案"四款中唯一免费"）→ 产品名 → 一句话 → 描述 → 产品色渐变圆球（Reverb 雾青 `#93c5fd→#64748b`、Delay 青紫 `#c4b5fd→#6366f1`）→ 预告特性 chips → 邮箱订阅表单（P3 接 Supabase）。右下角 Reverb/Delay 切换器仅原型使用，正式站为两个独立页面。
- **About 页**（`prototypes/p1-about-v1.html`）：Hero（左文右 260px 头像占位）→ 01 我的故事（草稿文案待用户提供）→ 02 为什么是 SanMu → 03 一起做（合作邀请卡：三步说明 + 邮件申请 contact@sanmuaudio.com）→ 04 联系（2×2 卡片）。
