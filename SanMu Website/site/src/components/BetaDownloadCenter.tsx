type Lang = 'zh' | 'en';

const macProducts = [
  { name: 'SanMu EQ6', version: '0.1.5', formats: 'VST3 / AU / AAX' },
  { name: 'SanMu Comp', version: '0.1.17', formats: 'VST3 / AU / AAX' },
  { name: 'SanMu Delay', version: '0.1.5', formats: 'VST3 / AU / AAX' },
  { name: 'SanMu Reverb', version: '1.0.4', formats: 'VST3 / AU / AAX' },
];

const winProducts = [
  { name: 'SanMu EQ6', version: '0.1.5', formats: 'VST3 / AAX' },
  { name: 'SanMu Comp', version: '0.1.17', formats: 'VST3 / AAX' },
  { name: 'SanMu Delay', version: '0.1.5', formats: 'VST3 / AAX' },
  { name: 'SanMu Reverb', version: '1.0.4', formats: 'VST3 / AAX' },
];

const copy = {
  zh: {
    title: '官方发售筹备中 · 暂未开放下载',
    body: 'SanMu Audio 四款效果器插件（SanMu EQ6、SanMu Comp、SanMu Delay、SanMu Reverb）已全部完成 Apple 官方公证（Notarized）与 Avid PACE 官方签名。目前团队正在最终整理定价方案与商业发售策略，安装包下载通道暂时关闭，敬请期待正式公布！您可先查阅下方产品说明与用户手册了解插件功能。',
    suiteTitle: '🏷️ 定价与发售方案制定中',
    suiteDesc: '全套四款插件（EQ6、Comp、Delay、Reverb）将连同官方定价与授权方式统一正式公布。',
    comingSoonBadge: '敬请期待 · 暂未开放下载',
    macReady: 'macOS Universal 2 · VST3 / AU / AAX（签名公证已就绪）',
    windowsPending: 'Windows x64 · VST3 / AAX（筹备中）',
    statusTag: '发售筹备中',
    docLink: '查阅产品文档与手册 ↗',
    feedbackLink: '关注发售动态 / 联系工作室 ↗',
  },
  en: {
    title: 'Official Launch in Preparation · Downloads Paused',
    body: 'The SanMu Audio plug-in suite (EQ6, Comp, Delay, Reverb) has completed Apple Notarization and Avid PACE signing. Official pricing and launch packages are currently being finalized. Downloads are temporarily paused. Stay tuned for the official release! In the meantime, feel free to explore the product overviews and manuals below.',
    suiteTitle: '🏷️ Pricing & Launch Details Pending',
    suiteDesc: 'The complete 4-plugin suite will be officially launched alongside pricing and licensing options.',
    comingSoonBadge: 'Coming Soon · Downloads Closed',
    macReady: 'macOS Universal 2 · VST3 / AU / AAX (Signed & Notarized)',
    windowsPending: 'Windows x64 · VST3 / AAX (In preparation)',
    statusTag: 'In Preparation',
    docLink: 'Read Documentation & Manuals ↗',
    feedbackLink: 'Contact Studio ↗',
  },
} as const;

export default function BetaDownloadCenter({ lang }: { lang: Lang; releaseId?: string }) {
  const t = copy[lang];

  return (
    <div class="download-center">
      <div class="download-auth">
        <div>
          <p class="eyebrow">SANMU AUDIO · OFFICIAL LAUNCH PREPARATION</p>
          <h2>{t.title}</h2>
          <p>{t.body}</p>
        </div>
        <div style="display:flex; flex-direction:column; justify-content:center; gap:14px; background:#fff; padding:24px; border-radius:8px; border:1px solid #e2e2e4;">
          <strong style="font-size:15px; font-weight:600; color:#111;">{t.suiteTitle}</strong>
          <p style="font-size:12px; color:#666; margin:0; line-height:1.6;">{t.suiteDesc}</p>
          <div style="display:flex; gap:10px; flex-wrap:wrap; align-items:center; margin-top:4px;">
            <span
              style="display:inline-flex; align-items:center; font-size:12px; font-weight:600; padding:8px 16px; background:#f0f0f2; color:#555; border-radius:6px;"
            >
              {t.comingSoonBadge}
            </span>
            <a class="text-link" href="#documents" style="font-size:11px;">
              {t.docLink}
            </a>
          </div>
        </div>
      </div>

      <div class="download-release-meta">
        <span>macOS Universal 2 · Apple Silicon (M1~M4) & Intel · macOS 10.15+</span>
        <span>代码签名与 Apple 官方公证已就绪 · 发售方案准备中</span>
      </div>

      <div class="download-platform-grid">
        <section class="download-platform">
          <div class="download-platform-heading">
            <span class="platform-mark platform-mark--mac"></span>
            <h3>macOS</h3>
            <span class="download-platform-note">{t.macReady}</span>
          </div>
          <div class="download-artifact-list">
            {macProducts.map((product) => (
              <article class="download-artifact" key={product.name}>
                <div>
                  <strong>{product.name}</strong>
                  <small>
                    v{product.version} · Universal 2 · {product.formats} · 签名公证已就绪
                  </small>
                </div>
                <div style="display:flex; gap:8px; align-items:center;">
                  <span
                    class="download-artifact-status"
                    style="font-size:11px; padding:6px 12px; background:#f0f0f2; color:#666; border-radius:5px; font-weight:500;"
                  >
                    {t.statusTag}
                  </span>
                </div>
              </article>
            ))}
          </div>
        </section>

        <section class="download-platform">
          <div class="download-platform-heading">
            <span class="platform-mark platform-mark--windows"></span>
            <h3>Windows</h3>
            <span class="download-platform-note">{t.windowsPending}</span>
          </div>
          <div class="download-artifact-list">
            {winProducts.map((product) => (
              <article class="download-artifact" key={product.name}>
                <div>
                  <strong>{product.name}</strong>
                  <small>v{product.version} · Windows 10/11 x64 · {product.formats}</small>
                </div>
                <span
                  class="download-artifact-status"
                  style="font-size:11px; padding:6px 12px; background:#f0f0f2; color:#666; border-radius:5px; font-weight:500;"
                >
                  {t.statusTag}
                </span>
              </article>
            ))}
          </div>
        </section>
      </div>
    </div>
  );
}

