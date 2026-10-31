import { useEffect, useMemo, useState } from 'preact/hooks';
import { supabase } from '../lib/supabase';

type Lang = 'zh' | 'en';
type Artifact = {
  id: string;
  product_slug: string;
  product_name: string;
  platform: 'macOS' | 'Windows';
  version: string;
  package_name: string;
  object_path: string;
  status: 'ready' | 'pending' | 'withdrawn';
  size_bytes: number | null;
  sha256: string | null;
  min_system: string;
  formats: string[];
};

const copy = {
  zh: {
    loginTitle: '官方签名安装包与开源源码',
    loginBody: 'SanMu Audio 采用双轨模式：GitHub 开源仓库免费提供完整 C++ / WebUI 源码（AGPLv3 协议，可自行编译）；官网下载中心提供已完成 Apple Developer ID 代码签名的一键安装包，省去环境配置。',
    email: '邮箱', password: '密码（至少 6 位）', login: '登录', register: '注册账号', reset: '发送重置邮件',
    forgot: '忘记密码？', switchRegister: '还没有账号？注册', switchLogin: '已有账号？登录', signOut: '退出登录',
    resetBody: '输入注册邮箱后，我们会发送密码重置链接。',
    notConfigured: '一键签名安装包直链即将上线。你可先阅读下方产品说明与用户手册，或前往 GitHub 获取开源源码。',
    loading: '正在读取下载权限…',
    empty: '你已登录，此版本安装包正在整理，开放后会出现在这里。',
    download: '下载签名安装包', pending: '准备中',
    macReady: 'macOS Universal 2 · VST3 / AU / AAX（已签名）', windowsPending: 'Windows x64 · VST3 / AAX',
    expires: 'AGPLv3 开源正式版 · 永久无期限限制', failed: '暂时无法获取下载链接，请稍后重试。',
    eula: '注册前请阅读并同意开源与授权条款', eulaLink: '查看授权条款与发布资料',
  },
  en: {
    loginTitle: 'Official Signed Installers & Open-Source Code',
    loginBody: 'SanMu Audio follows an open-core model: full C++ / WebUI source code is free on GitHub under AGPLv3; official ready-to-install signed packages are available here to save you from building from source.',
    email: 'Email', password: 'Password (6+ characters)', login: 'Sign in', register: 'Create account', reset: 'Send reset email',
    forgot: 'Forgot password?', switchRegister: 'Need an account? Create one', switchLogin: 'Already registered? Sign in', signOut: 'Sign out',
    resetBody: 'Enter your account email and we will send a password reset link.',
    notConfigured: 'Direct installer downloads will open shortly. Read the product overview and user manual below, or build from source on GitHub.',
    loading: 'Checking download access…',
    empty: 'You are signed in. Packages for this release are being prepared and will appear here when available.',
    download: 'Download signed installer', pending: 'In preparation',
    macReady: 'macOS Universal 2 · VST3 / AU / AAX (Signed)', windowsPending: 'Windows x64 · VST3 / AAX',
    expires: 'AGPLv3 Open-Source Release · No Time Limit', failed: 'The download link could not be created. Try again later.',
    eula: 'Read and accept the license terms before registering', eulaLink: 'View license and release documents',
  },
} as const;

export default function BetaDownloadCenter({ lang, releaseId }: { lang: Lang; releaseId: string }) {
  const t = copy[lang];
  const [session, setSession] = useState(false);
  const [mode, setMode] = useState<'login' | 'register' | 'reset'>('login');
  const [email, setEmail] = useState('');
  const [password, setPassword] = useState('');
  const [authMessage, setAuthMessage] = useState('');
  const [eulaAccepted, setEulaAccepted] = useState(false);
  const [artifacts, setArtifacts] = useState<Artifact[]>([]);
  const [loading, setLoading] = useState(Boolean(supabase));
  const [error, setError] = useState('');

  const catalog = useMemo(() => {
    const names = ['SanMu EQ6', 'SanMu Comp', 'SanMu Delay', 'SanMu Reverb'];
    return ['macOS', 'Windows'].flatMap((platform) => names.map((productName) => ({ platform, productName })));
  }, []);

  useEffect(() => {
    if (!supabase) {
      setLoading(false);
      return;
    }
    supabase.auth.getSession().then(({ data }) => {
      setSession(Boolean(data.session));
      if (data.session) void loadArtifacts();
      else setLoading(false);
    });
    const { data: listener } = supabase.auth.onAuthStateChange((_event, nextSession) => {
      const loggedIn = Boolean(nextSession);
      setSession(loggedIn);
      if (loggedIn) void loadArtifacts();
      else setArtifacts([]);
    });
    return () => listener.subscription.unsubscribe();
  }, [releaseId]);

  async function loadArtifacts() {
    if (!supabase) return;
    setLoading(true);
    const { data, error: queryError } = await supabase.from('beta_artifacts').select('*').eq('release_id', releaseId).eq('status', 'ready').order('platform').order('product_name');
    if (queryError) setError(queryError.message);
    else setArtifacts((data ?? []) as Artifact[]);
    setLoading(false);
  }

  async function submitAuth(event: Event) {
    event.preventDefault();
    setAuthMessage('');
    if (!supabase) return;
    if (mode === 'reset') {
      const result = await supabase.auth.resetPasswordForEmail(email, { redirectTo: window.location.href });
      setAuthMessage(result.error ? result.error.message : t.resetBody);
      return;
    }
    if (password.length < 6) {
      setAuthMessage(lang === 'zh' ? '密码至少需要 6 位。' : 'Password must contain at least 6 characters.');
      return;
    }
    if (mode === 'register' && !eulaAccepted) {
      setAuthMessage(lang === 'zh' ? '请先阅读并同意内测 EULA。' : 'Please read and accept the beta EULA first.');
      return;
    }
    const result = mode === 'login' ? await supabase.auth.signInWithPassword({ email, password }) : await supabase.auth.signUp({ email, password });
    if (result.error) setAuthMessage(result.error.message);
    else if (mode === 'register') setAuthMessage(lang === 'zh' ? '账号已创建，可以开始查看下载权限。' : 'Account created. You can now check download access.');
  }

  async function download(artifact: Artifact) {
    if (!supabase) return;
    setError('');
    const { data, error: signedError } = await supabase.storage.from('beta-downloads').createSignedUrl(artifact.object_path, 600);
    if (signedError || !data?.signedUrl) {
      setError(t.failed);
      return;
    }
    window.open(data.signedUrl, '_blank', 'noopener,noreferrer');
  }

  const available = (platform: string, productName: string) => artifacts.find((item) => item.platform === platform && item.product_name === productName);

  return (
    <div class="download-center">
      <div class="download-auth">
        <div><p class="eyebrow">SANMU AUDIO · OPEN SOURCE & INSTALLERS</p><h2>{t.loginTitle}</h2><p>{t.loginBody}</p></div>
        {!supabase ? <p class="download-state">{t.notConfigured}</p> : session ? (
          <div class="download-authenticated"><span>{email || (lang === 'zh' ? '已登录' : 'Signed in')}</span><button type="button" class="button button-outline" onClick={() => supabase.auth.signOut()}>{t.signOut}</button></div>
        ) : (
          <form class="download-login" onSubmit={submitAuth}>
            <label><span>{t.email}</span><input type="email" required value={email} onInput={(event) => setEmail((event.currentTarget as HTMLInputElement).value)} /></label>
            {mode !== 'reset' && <label><span>{t.password}</span><input type="password" required minLength={6} value={password} onInput={(event) => setPassword((event.currentTarget as HTMLInputElement).value)} /></label>}
            {mode === 'register' && <label class="download-eula"><input type="checkbox" checked={eulaAccepted} onChange={(event) => setEulaAccepted((event.currentTarget as HTMLInputElement).checked)} /><span>{t.eula} · <a href={`${lang === 'zh' ? '/zh' : '/en'}/release`}>{t.eulaLink}</a></span></label>}
            <div class="download-login-actions"><button type="submit" class="button button-dark">{mode === 'login' ? t.login : mode === 'register' ? t.register : t.reset}</button>{mode === 'login' && <button type="button" class="text-link" onClick={() => setMode('reset')}>{t.forgot}</button>}<button type="button" class="text-link" onClick={() => setMode(mode === 'register' ? 'login' : mode === 'reset' ? 'login' : 'register')}>{mode === 'login' ? t.switchRegister : t.switchLogin}</button></div>
            {authMessage && <p class="download-state">{authMessage}</p>}
          </form>
        )}
      </div>
      <div class="download-release-meta"><span>{lang === 'zh' ? 'SanMu Audio 开源正式版' : 'SanMu Audio Open-Source Release'}</span><span>{t.expires}</span></div>
      {session && loading && <p class="download-state">{t.loading}</p>}
      {session && !loading && error && <p class="download-state download-state--error">{error}</p>}
      {session && !loading && artifacts.length === 0 && <p class="download-state">{t.empty}</p>}
      <div class="download-platform-grid">
        {['macOS', 'Windows'].map((platform) => (
          <section class="download-platform" key={platform}>
            <div class="download-platform-heading"><span class={`platform-mark platform-mark--${platform === 'macOS' ? 'mac' : 'windows'}`}></span><h3>{platform}</h3><span class="download-platform-note">{platform === 'macOS' ? t.macReady : t.windowsPending}</span></div>
            <div class="download-artifact-list">
              {catalog.filter((entry) => entry.platform === platform).map((entry) => {
                const artifact = available(platform, entry.productName);
                return <article class="download-artifact" key={`${platform}-${entry.productName}`}><div><strong>{entry.productName}</strong><small>{artifact?.version ? `${lang === 'zh' ? '版本' : 'Version'} ${artifact.version}` : t.pending}</small></div>{session && artifact ? <button type="button" class="button button-dark" onClick={() => download(artifact)}>{t.download}</button> : <span class="download-artifact-status">{artifact ? (lang === 'zh' ? '可下载' : 'Available') : t.pending}</span>}</article>;
              })}
            </div>
          </section>
        ))}
      </div>
    </div>
  );
}
