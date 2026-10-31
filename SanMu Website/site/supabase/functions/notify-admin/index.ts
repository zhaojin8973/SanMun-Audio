// Supabase Edge Function：comments 表 INSERT 时由 Database Webhook 触发，
// 新留言进入待审（或任何新留言）→ 经 Resend 发邮件通知管理员。
//
// 部署：
//   supabase secrets set RESEND_API_KEY=re_xxx ADMIN_EMAIL=contact@sanmuaudio.com
//   supabase functions deploy notify-admin --no-verify-jwt
// 然后在 Dashboard → Database → Webhooks 建 trigger：
//   Table: comments, Events: INSERT, Function: notify-admin

const ADMIN_EMAIL = Deno.env.get('ADMIN_EMAIL') ?? 'contact@sanmuaudio.com';
const RESEND_API_KEY = Deno.env.get('RESEND_API_KEY') ?? '';

const TYPE_LABEL: Record<string, string> = {
  request: '💡 需求',
  bug: '🐛 Bug',
  feedback: '👍 反馈',
  chat: '💬 闲聊',
};

Deno.serve(async (req) => {
  if (!RESEND_API_KEY) {
    return new Response(JSON.stringify({ error: 'RESEND_API_KEY not set' }), { status: 500 });
  }
  const payload = await req.json();
  const r = payload?.record;
  if (!r?.id) {
    return new Response(JSON.stringify({ skipped: true, reason: 'no record' }), { status: 200 });
  }

  const pendingNote = r.status === 'pending' ? '（待审核：请到 /admin 处理）' : '';
  const subject = `[SanMu] 新留言 · ${TYPE_LABEL[r.type] ?? r.type} · ${r.product_id}${pendingNote}`;
  const html = `
    <p><b>${TYPE_LABEL[r.type] ?? r.type}</b> · 产品：<b>${r.product_id}</b> · 状态：<b>${r.status}</b></p>
    <p>${r.body ?? ''}</p>
    <p><a href="https://sanmuaudio.com/admin">打开管理后台处理</a></p>
  `;

  const resp = await fetch('https://api.resend.com/emails', {
    method: 'POST',
    headers: { Authorization: `Bearer ${RESEND_API_KEY}`, 'Content-Type': 'application/json' },
    body: JSON.stringify({
      from: 'SanMu Audio <notify@sanmuaudio.com>',
      to: [ADMIN_EMAIL],
      subject,
      html,
    }),
  });

  return new Response(JSON.stringify({ sent: resp.status }), { status: resp.ok ? 200 : 502 });
});
