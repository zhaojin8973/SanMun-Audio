type Lang = 'zh' | 'en';

const macProducts = [
  {
    name: 'SanMu EQ6',
    version: '0.1.5',
    size: '13.7 MB',
    filename: 'SanMu_EQ6_v0.1.5_macOS_Signed.pkg',
    downloadUrl: '/downloads/macos/SanMu_EQ6_v0.1.5_macOS_Signed.pkg',
    mirrorUrl: 'https://github.com/zhaojin8973/SanMun-Audio/releases/download/v1.0.0-macos/SanMu_EQ6_v0.1.5_macOS_Signed.pkg',
    formats: 'VST3 / AU / AAX',
  },
  {
    name: 'SanMu Comp',
    version: '0.1.17',
    size: '13.5 MB',
    filename: 'SanMu_Comp_v0.1.17_macOS_Signed.pkg',
    downloadUrl: '/downloads/macos/SanMu_Comp_v0.1.17_macOS_Signed.pkg',
    mirrorUrl: 'https://github.com/zhaojin8973/SanMun-Audio/releases/download/v1.0.0-macos/SanMu_Comp_v0.1.17_macOS_Signed.pkg',
    formats: 'VST3 / AU / AAX',
  },
  {
    name: 'SanMu Delay',
    version: '0.1.5',
    size: '13.1 MB',
    filename: 'SanMu_Delay_v0.1.5_macOS_Signed.pkg',
    downloadUrl: '/downloads/macos/SanMu_Delay_v0.1.5_macOS_Signed.pkg',
    mirrorUrl: 'https://github.com/zhaojin8973/SanMun-Audio/releases/download/v1.0.0-macos/SanMu_Delay_v0.1.5_macOS_Signed.pkg',
    formats: 'VST3 / AU / AAX',
  },
  {
    name: 'SanMu Reverb',
    version: '1.0.4',
    size: '15.9 MB',
    filename: 'SanMu_Reverb_v1.0.4_macOS_Signed.pkg',
    downloadUrl: '/downloads/macos/SanMu_Reverb_v1.0.4_macOS_Signed.pkg',
    mirrorUrl: 'https://github.com/zhaojin8973/SanMun-Audio/releases/download/v1.0.0-macos/SanMu_Reverb_v1.0.4_macOS_Signed.pkg',
    formats: 'VST3 / AU / AAX',
  },
];

const winProducts = [
  { name: 'SanMu EQ6', version: '0.1.5', formats: 'VST3 / AAX' },
  { name: 'SanMu Comp', version: '0.1.17', formats: 'VST3 / AAX' },
  { name: 'SanMu Delay', version: '0.1.5', formats: 'VST3 / AAX' },
  { name: 'SanMu Reverb', version: '1.0.4', formats: 'VST3 / AAX' },
];

const copy = {
  zh: {
    title: '官方签名公证安装包与开源源码',
    body: 'SanMu Audio 四款效果器现已开放正式版一键下载。macOS 安装包均已完成 Apple Developer ID 签名、苹果官方公证（Notarized）与 Avid PACE 官方 AAX 签名，下载后直接双击安装即可，无需额外安全授权；原生支持 Apple Silicon（M1/M2/M3/M4）与 Intel 处理器，完美兼容 Logic Pro、Cubase、Pro Tools、Reaper、FL Studio 等宿主。',
    suiteTitle: '📦 四合一全套插件包 (macOS · 56 MB)',
    suiteDesc: '包含 EQ6、Comp、Delay、Reverb 全部四款已公证安装包，适合一键批量安装。',
    downloadSuite: '一键下载全套安装包 (.zip)',
    downloadPkg: '下载 .pkg',
    macReady: 'macOS Universal 2 · VST3 / AU / AAX（已签名公证）',
    windowsPending: 'Windows x64 · VST3 / AAX（支持源码编译）',
    expires: 'AGPLv3 开源正式版 · 永久无期限限制',
    checksums: 'SHA256 校验清单',
    releaseTag: 'GitHub Release 镜像 ↗',
    buildSource: '源码 / 构建 ↗',
    docLink: '查看版本与公证记录 ↗',
    notarizedTag: '已公证 · 门禁零拦截',
  },
  en: {
    title: 'Official Signed & Notarized Installers & Source Code',
    body: 'SanMu Audio plug-in suite is now open for direct download. macOS packages are signed with Apple Developer ID, officially notarized by Apple, and digitally signed by Avid PACE for AAX. Ready to install with zero Gatekeeper warnings; natively supports Apple Silicon (M1-M4) & Intel, compatible with Logic Pro, Cubase, Pro Tools, Reaper, FL Studio, and more.',
    suiteTitle: '📦 4-in-1 Suite Package (macOS · 56 MB)',
    suiteDesc: 'Contains notarized installers for EQ6, Comp, Delay, and Reverb in a single download.',
    downloadSuite: 'Download 4-in-1 Suite (.zip)',
    downloadPkg: 'Download .pkg',
    macReady: 'macOS Universal 2 · VST3 / AU / AAX (Notarized)',
    windowsPending: 'Windows x64 · VST3 / AAX (Build from source)',
    expires: 'AGPLv3 Open-Source Release · No Time Limit',
    checksums: 'SHA256 Checksums',
    releaseTag: 'GitHub Release Mirror ↗',
    buildSource: 'Source / Build ↗',
    docLink: 'View Release & Notarization Docs ↗',
    notarizedTag: 'Notarized · Zero Warnings',
  },
} as const;

export default function BetaDownloadCenter({ lang }: { lang: Lang; releaseId?: string }) {
  const t = copy[lang];

  return (
    <div class="download-center">
      <div class="download-auth">
        <div>
          <p class="eyebrow">SANMU AUDIO · OPEN SOURCE & OFFICIAL INSTALLERS</p>
          <h2>{t.title}</h2>
          <p>{t.body}</p>
        </div>
        <div style="display:flex; flex-direction:column; justify-content:center; gap:14px; background:#fff; padding:24px; border-radius:8px; border:1px solid #e2e2e4;">
          <strong style="font-size:15px; font-weight:600; color:#111;">{t.suiteTitle}</strong>
          <p style="font-size:12px; color:#666; margin:0; line-height:1.6;">{t.suiteDesc}</p>
          <div style="display:flex; gap:10px; flex-wrap:wrap; align-items:center; margin-top:4px;">
            <a
              class="button button-dark"
              href="https://github.com/zhaojin8973/SanMun-Audio/releases/download/v1.0.0-macos/SanMu_Audio_macOS_Signed_Installers_Suite.zip"
              style="font-size:12px; padding:0 18px; min-height:40px; text-decoration:none;"
            >
              {t.downloadSuite}
            </a>
            <a class="text-link" href="/downloads/macos/SHA256SUMS.txt" target="_blank" rel="noopener" style="font-size:11px;">
              {t.checksums}
            </a>
            <a class="text-link" href="https://github.com/zhaojin8973/SanMun-Audio/releases/tag/v1.0.0-macos" target="_blank" rel="noopener" style="font-size:11px;">
              {t.releaseTag}
            </a>
          </div>
        </div>
      </div>

      <div class="download-release-meta">
        <span>macOS Universal 2 · Apple Silicon (M1~M4) & Intel · macOS 10.15+</span>
        <span>{t.expires}</span>
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
                    v{product.version} · {product.size} · Universal 2 · {product.formats} · {t.notarizedTag}
                  </small>
                </div>
                <div style="display:flex; gap:8px; align-items:center;">
                  <a class="button button-dark" href={product.downloadUrl} download style="text-decoration:none;">
                    {t.downloadPkg}
                  </a>
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
                <a
                  class="button button-outline"
                  href="https://github.com/zhaojin8973/SanMun-Audio"
                  target="_blank"
                  rel="noopener"
                  style="text-decoration:none;"
                >
                  {t.buildSource}
                </a>
              </article>
            ))}
          </div>
        </section>
      </div>
    </div>
  );
}

