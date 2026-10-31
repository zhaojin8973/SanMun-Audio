import { useEffect, useState, useCallback } from 'preact/hooks';
import { supabase, type CommentRow, type CommentStatus, type CommentType } from '../../lib/supabase';

type Lang = 'zh' | 'en';

const T = {
  zh: {
    signin: '注册后可留言 · ',
    signinB: '邮箱 + 密码',
    signinTail: '，无需验证码 · 首条留言审核后公开，之后免审',
    chips: [
      { k: 'request', label: '💡 需求' },
      { k: 'bug', label: '🐛 Bug' },
      { k: 'feedback', label: '👍 反馈' },
      { k: 'chat', label: '💬 闲聊' },
    ],
    placeholder: '说说你在混音里想解决的问题…',
    publish: '发布',
    login: '登录',
    register: '注册',
    logout: '退出',
    email: '邮箱',
    password: '密码',
    needLogin: '发布与投票需要登录',
    pendingHint: '首条留言已提交，审核通过后公开',
    official: '官方回复',
    daysAgo: (n: number) => (n === 0 ? '今天' : n === 1 ? '昨天' : `${n} 天前`),
    loading: '加载中…',
    empty: '还没有留言，来发第一条吧。',
    error: '出错了，请稍后再试',
    status: { pending: '待审核', published: '已发布', resolved: '已收到', fixed: '已修复', accepted: '已采纳' },
    errEmail: '邮箱或密码不正确',
    errWeak: '密码至少 6 位',
    signIn: '登录后发布',
  },
  en: {
    signin: 'Sign in to comment · ',
    signinB: 'Email + password',
    signinTail: ', no verification code · first comment is reviewed, then trusted',
    chips: [
      { k: 'request', label: '💡 Request' },
      { k: 'bug', label: '🐛 Bug' },
      { k: 'feedback', label: '👍 Feedback' },
      { k: 'chat', label: '💬 Chat' },
    ],
    placeholder: 'Tell us what you want to solve in your mixes…',
    publish: 'Publish',
    login: 'Sign in',
    register: 'Sign up',
    logout: 'Sign out',
    email: 'Email',
    password: 'Password',
    needLogin: 'Sign in to post and vote',
    pendingHint: 'First comment submitted — it appears after review',
    official: 'Official reply',
    daysAgo: (n: number) => (n === 0 ? 'today' : n === 1 ? 'yesterday' : `${n}d ago`),
    loading: 'Loading…',
    empty: 'No comments yet — be the first.',
    error: 'Something went wrong, try again',
    status: { pending: 'Pending', published: 'Published', resolved: 'Received', fixed: 'Fixed', accepted: 'Accepted' },
    errEmail: 'Incorrect email or password',
    errWeak: 'Password needs 6+ characters',
    signIn: 'Sign in to comment',
  },
};

function daysAgo(iso: string): number {
  return Math.floor((Date.now() - new Date(iso).getTime()) / 86400000);
}

export default function CommentWidget({ lang, productId }: { lang: Lang; productId: string }) {
  const t = T[lang];
  const [session, setSession] = useState(false);
  const [me, setMe] = useState<{ id: string; display_name: string; is_admin: boolean } | null>(null);
  const [rows, setRows] = useState<CommentRow[]>([]);
  const [loading, setLoading] = useState(true);
  const [mode, setMode] = useState<'login' | 'register'>('login');
  const [email, setEmail] = useState('');
  const [password, setPassword] = useState('');
  const [authMsg, setAuthMsg] = useState('');
  const [type, setType] = useState<CommentType>('request');
  const [body, setBody] = useState('');
  const [postMsg, setPostMsg] = useState('');

  const fetchRows = useCallback(async () => {
    if (!supabase) return;
    const { data: { user } } = await supabase.auth.getUser();
    let q = supabase
      .from('comments')
      .select('*, profiles:author_id(display_name, is_admin), replies:comments!parent_id(*, profiles:author_id(display_name, is_admin))')
      .eq('product_id', productId)
      .order('is_pinned', { ascending: false })
      .order('created_at', { ascending: false })
      .limit(100);
    q = user ? q.or(`status.neq.pending,author_id.eq.${user.id}`) : q.neq('status', 'pending');
    const { data, error } = await q;
    if (!error && data) setRows(data as unknown as CommentRow[]);
    setLoading(false);
  }, [productId]);

  useEffect(() => {
    if (!supabase) return;
    supabase.auth.getSession().then(({ data }) => {
      setSession(!!data.session);
      if (data.session?.user) {
        supabase!.from('profiles').select('id, display_name, is_admin').eq('id', data.session.user.id).single().then(({ data: p }) => setMe(p as typeof me));
      }
    });
    const { data: sub } = supabase.auth.onAuthStateChange((_e, s) => {
      setSession(!!s);
      if (s?.user) {
        supabase!.from('profiles').select('id, display_name, is_admin').eq('id', s.user.id).single().then(({ data: p }) => setMe(p as typeof me));
      } else setMe(null);
    });
    fetchRows();
    return () => sub.subscription.unsubscribe();
  }, [fetchRows]);

  async function submitAuth(e: Event) {
    e.preventDefault();
    setAuthMsg('');
    if (password.length < 6) return setAuthMsg(t.errWeak);
    if (!supabase) return;
    const res = mode === 'register'
      ? await supabase.auth.signUp({ email, password })
      : await supabase.auth.signInWithPassword({ email, password });
    if (res.error) setAuthMsg(res.error.message || t.errEmail);
    if (mode === 'register' && !res.error) setAuthMsg(t.pendingHint);
  }

  async function signOut() {
    await supabase?.auth.signOut();
  }

  async function vote(id: string) {
    if (!supabase || !session) return;
    const { error } = await supabase.rpc('toggle_vote', { target: id });
    if (!error) fetchRows(); // 回读真实计数（toggle 语义在数据库侧）
  }

  async function submit(e: Event) {
    e.preventDefault();
    if (!supabase || !me || !body.trim()) return;
    const { data, error } = await supabase.from('comments').insert({
      product_id: productId, type, body: body.trim(), author_id: me.id,
    }).select('status').single();
    setBody('');
    if (!error) {
      setPostMsg(data?.status === 'pending' ? t.pendingHint : '');
      fetchRows();
    } else setPostMsg(t.error);
  }

  const statusLabel = (s: CommentStatus) => t.status[s] ?? s;

  return (
    <div>
      <div class="signin">
        🔐{' '}
        <span>
          {t.signin}
          <b>{t.signinB}</b>
          {t.signinTail}
        </span>
      </div>

      {session ? (
        <div class="comment-form">
          <div class="chips">
            {t.chips.map((c) => (
              <button key={c.k} type="button" class={type === c.k ? 'chip is-on' : 'chip'} onClick={() => setType(c.k as CommentType)} style="font-family:inherit;background:none;">
                {c.label}
              </button>
            ))}
          </div>
          <textarea value={body} onInput={(e) => setBody((e.target as HTMLTextAreaElement).value)} placeholder={t.placeholder}></textarea>
          <div class="row" style="display:flex;justify-content:flex-end;align-items:center;gap:12px;">
            {postMsg && <span class="t-caption" style="margin-right:auto;">{postMsg}</span>}
            <button class="btn btn--dark" type="button" onClick={submit}>{t.publish}</button>
          </div>
        </div>
      ) : (
        <div class="comment-form">
          <p class="t-meta" style="margin:0 0 12px;">{t.needLogin}</p>
          <form onSubmit={submitAuth} style="display:flex;gap:10px;flex-wrap:wrap;align-items:center;">
            <input class="auth-in" type="email" required placeholder={t.email} value={email} onInput={(e) => setEmail((e.target as HTMLInputElement).value)} />
            <input class="auth-in" type="password" required placeholder={t.password} value={password} onInput={(e) => setPassword((e.target as HTMLInputElement).value)} />
            <button class="btn btn--dark" type="submit">{mode === 'login' ? t.login : t.register}</button>
            <button class="btn btn--ghost" type="button" onClick={() => setMode(mode === 'login' ? 'register' : 'login')} style="border:none;text-decoration:underline;">
              {mode === 'login' ? t.register : t.login}
            </button>
          </form>
          {authMsg && <p class="t-caption" style="margin:10px 0 0;">{authMsg}</p>}
          <style>{`.auth-in{height:34px;padding:0 14px;border:1px solid var(--c-border-row);border-radius:10px;font:inherit;font-size:var(--fs-meta);width:190px;}`}</style>
        </div>
      )}

      {session && (
        <p class="t-caption" style="margin:12px 0 0;">
          {me?.display_name || email} · {me?.is_admin && <span class="badge type">Admin</span>}{' '}
          <button type="button" class="t-caption" onClick={signOut} style="background:none;border:none;cursor:pointer;text-decoration:underline;">{t.logout}</button>
        </p>
      )}

      <div style="margin-top:18px;">
        {loading && <p class="t-caption">{t.loading}</p>}
        {!loading && rows.length === 0 && <p class="t-caption">{t.empty}</p>}
        {rows.map((r) => (
          <div class="c-item" key={r.id}>
            <div class="head">
              <span class="who">{r.profiles?.display_name || '·'}</span>
              {r.profiles?.is_admin && <span class="badge type">SanMu</span>}
              <span class="badge type">{T[lang].chips.find((c) => c.k === r.type)?.label ?? r.type}</span>
              {r.is_pinned && <span class="badge status">📌</span>}
              {r.status !== 'published' && <span class="badge status">{statusLabel(r.status)}</span>}
              <span class="time">{t.daysAgo(daysAgo(r.created_at))}</span>
            </div>
            <p class="body">{r.body}</p>
            <div class="foot">
              <button type="button" class="vote" style="background:none;border:none;font-family:inherit;" onClick={() => vote(r.id)} disabled={!session}>
                ▲ {r.vote_count}
              </button>
              {r.replies?.length > 0 && (
                <div style="flex:1;">
                  {r.replies.map((rp) => (
                    <div class="reply" key={rp.id}>
                      <b>{t.official}：</b>
                      {rp.body}
                    </div>
                  ))}
                </div>
              )}
            </div>
          </div>
        ))}
      </div>
    </div>
  );
}
