import { useEffect, useState, useCallback } from 'preact/hooks';
import { supabase, type CommentRow, type CommentStatus } from '../../lib/supabase';

const STATUSES: { k: CommentStatus; zh: string; en: string }[] = [
  { k: 'pending', zh: '待审核', en: 'Pending' },
  { k: 'published', zh: '已发布', en: 'Published' },
  { k: 'resolved', zh: '已收到', en: 'Received' },
  { k: 'fixed', zh: '已修复', en: 'Fixed' },
  { k: 'accepted', zh: '已采纳', en: 'Accepted' },
];

export default function AdminApp() {
  const [me, setMe] = useState<{ id: string; display_name: string; is_admin: boolean } | null>(null);
  const [email, setEmail] = useState('');
  const [password, setPassword] = useState('');
  const [msg, setMsg] = useState('');
  const [tab, setTab] = useState<'pending' | 'all' | 'subs'>('pending');
  const [rows, setRows] = useState<CommentRow[]>([]);
  const [subs, setSubs] = useState<{ id: string; product_id: string; email: string; created_at: string }[]>([]);
  const [loading, setLoading] = useState(false);

  const load = useCallback(async () => {
    if (!supabase) return;
    setLoading(true);
    const { data: { user } } = await supabase.auth.getUser();
    if (!user) return setLoading(false);
    const { data: p } = await supabase.from('profiles').select('id, display_name, is_admin').eq('id', user.id).single();
    setMe(p as typeof me);
    if (!p?.is_admin) return setLoading(false);
    const q = supabase
      .from('comments')
      .select('*, profiles:author_id(display_name, is_admin), replies:comments!parent_id(*, profiles:author_id(display_name, is_admin))')
      .order('created_at', { ascending: false })
      .limit(200);
    const { data, error } = await (tab === 'all' ? q : q.eq('status', 'pending'));
    if (!error && data) setRows(data as unknown as CommentRow[]);
    const { data: s } = await supabase.from('feedback_subscriptions').select('*').order('created_at', { ascending: false }).limit(200);
    if (s) setSubs(s);
    setLoading(false);
  }, [tab]);

  useEffect(() => {
    if (!supabase) return;
    supabase.auth.getSession().then(({ data }) => {
      if (data.session) load();
    });
  }, [load]);

  async function signIn(e: Event) {
    e.preventDefault();
    if (!supabase) return;
    const { error } = await supabase.auth.signInWithPassword({ email, password });
    if (error) return setMsg(error.message || '登录失败');
    load();
  }

  async function setStatus(id: string, status: CommentStatus) {
    await supabase?.from('comments').update({ status }).eq('id', id);
    load();
  }
  async function togglePin(id: string, is_pinned: boolean) {
    await supabase?.from('comments').update({ is_pinned }).eq('id', id);
    load();
  }
  async function remove(id: string) {
    if (!confirm('删除这条留言？')) return;
    await supabase?.from('comments').delete().eq('id', id);
    load();
  }

  function exportCsv() {
    const head = 'id,product,type,status,pinned,author,body,created_at';
    const lines = rows.map((r) =>
      [r.id, r.product_id, r.type, r.status, r.is_pinned ? 1 : 0, r.profiles?.display_name || '', `"${(r.body || '').replace(/"/g, '""')}"`, r.created_at].join(',')
    );
    download('sanmu-comments.csv', [head, ...lines].join('\n'));
  }
  function exportSubs() {
    download('sanmu-subscriptions.csv', ['product,email,created_at', ...subs.map((s) => `${s.product_id},${s.email},${s.created_at}`)].join('\n'));
  }
  function download(name: string, content: string) {
    const a = document.createElement('a');
    a.href = URL.createObjectURL(new Blob([content], { type: 'text/csv;charset=utf-8' }));
    a.download = name;
    a.click();
    URL.revokeObjectURL(a.href);
  }

  if (!supabase) return <p class="t-meta">未配置 Supabase（见 supabase/SETUP.md）。</p>;

  if (!me || !me.is_admin) {
    return (
      <div class="comment-form" style="max-width:440px;">
        <h3 class="t-heading" style="margin:0 0 6px;">管理员登录</h3>
        <p class="t-caption" style="margin:0 0 16px;color:#94a3b8;">使用管理员账号登录后，可审核留言、标记状态、置顶与导出数据。</p>
        <form onSubmit={signIn} style="display:flex;gap:10px;flex-direction:column;">
          <input class="auth-in" type="email" required placeholder="管理员邮箱" value={email} onInput={(e) => setEmail((e.target as HTMLInputElement).value)} />
          <input class="auth-in" type="password" required placeholder="密码" value={password} onInput={(e) => setPassword((e.target as HTMLInputElement).value)} />
          <div style="display:flex;gap:12px;align-items:center;margin-top:6px;">
            <button class="btn btn--hero" type="submit" style="height:38px;padding:0 24px;font-size:14px;">登录后台</button>
            {loading && <span class="t-caption" style="color:#94a3b8;">正在验证…</span>}
          </div>
        </form>
        {msg && <p class="t-caption" style="margin:12px 0 0;color:#f43f5e;font-weight:500;">{msg}</p>}
        <style>{`.auth-in{height:38px;padding:0 14px;border:1px solid rgba(255,255,255,0.15);border-radius:10px;font:inherit;font-size:var(--fs-meta);width:100%;color:#fff;background:rgba(255,255,255,0.06);box-sizing:border-box;}.auth-in:focus{outline:2px solid var(--c-accent);outline-offset:-1px;}`}</style>
      </div>
    );
  }

  return (
    <div>
      <div class="chips" style="margin-bottom:18px;">
        <button type="button" class={tab === 'pending' ? 'chip is-on' : 'chip'} style="font-family:inherit;background:none;" onClick={() => setTab('pending')}>待审核</button>
        <button type="button" class={tab === 'all' ? 'chip is-on' : 'chip'} style="font-family:inherit;background:none;" onClick={() => setTab('all')}>全部留言</button>
        <button type="button" class={tab === 'subs' ? 'chip is-on' : 'chip'} style="font-family:inherit;background:none;" onClick={() => setTab('subs')}>订阅邮箱 ({subs.length})</button>
        <button type="button" class="chip" style="font-family:inherit;background:none;" onClick={exportCsv}>导出 CSV</button>
        <button type="button" class="chip" style="font-family:inherit;background:none;" onClick={exportSubs}>导出订阅</button>
      </div>

      {loading && <p class="t-caption">加载中…</p>}

      {tab !== 'subs' && rows.length === 0 && !loading && <p class="t-caption">暂无数据</p>}

      {tab === 'subs' && (
        <div>
          {subs.map((s) => (
            <div class="c-item" key={s.id} style="padding:12px 18px;">
              <div class="head"><span class="who">{s.email}</span><span class="badge type">{s.product_id}</span><span class="time">{s.created_at.slice(0, 10)}</span></div>
            </div>
          ))}
        </div>
      )}

      {tab !== 'subs' && rows.map((r) => (
        <div class="c-item" key={r.id}>
          <div class="head">
            <span class="who">{r.profiles?.display_name || '·'}</span>
            <span class="badge type">{r.product_id} · {r.type}</span>
            <span class="badge status">{STATUSES.find((s) => s.k === r.status)?.zh}</span>
            <span class="time">{r.created_at.slice(0, 16).replace('T', ' ')}</span>
          </div>
          <p class="body">{r.body}</p>
          <div class="foot" style="flex-wrap:wrap;">
            <button type="button" class="chip" style="font-family:inherit;background:none;" onClick={() => setStatus(r.id, 'published')}>通过</button>
            <button type="button" class="chip" style="font-family:inherit;background:none;" onClick={() => setStatus(r.id, 'resolved')}>已收到</button>
            <button type="button" class="chip" style="font-family:inherit;background:none;" onClick={() => setStatus(r.id, 'fixed')}>已修复</button>
            <button type="button" class="chip" style="font-family:inherit;background:none;" onClick={() => setStatus(r.id, 'accepted')}>已采纳</button>
            <button type="button" class="chip" style="font-family:inherit;background:none;" onClick={() => togglePin(r.id, !r.is_pinned)}>{r.is_pinned ? '取消置顶' : '置顶'}</button>
            <button type="button" class="chip" style="font-family:inherit;background:none;border-color:#ff7875;color:#e11d48;" onClick={() => remove(r.id)}>删除</button>
          </div>
        </div>
      ))}
    </div>
  );
}
