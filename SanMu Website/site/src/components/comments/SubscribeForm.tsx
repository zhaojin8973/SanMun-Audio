import { useState } from 'preact/hooks';
import { supabase } from '../../lib/supabase';

export default function SubscribeForm({ lang, productId }: { lang: 'zh' | 'en'; productId: string }) {
  const t = {
    zh: { placeholder: '你的邮箱，上线时提醒你', cta: '上线提醒我', done: '已订阅！发布时会发邮件给你', err: '订阅失败，请稍后再试', invalid: '邮箱格式不对', note: '订阅功能已接入（Supabase）' },
    en: { placeholder: 'Your email for launch news', cta: 'Notify me', done: 'Subscribed! We\u2019ll email you at launch', err: 'Subscription failed, try again', invalid: 'Invalid email', note: 'Subscription powered by Supabase' },
  }[lang];
  const [email, setEmail] = useState('');
  const [msg, setMsg] = useState('');
  const [ok, setOk] = useState(false);

  async function submit(e: Event) {
    e.preventDefault();
    if (!supabase) return;
    if (!/^[^@\s]+@[^@\s]+\.[^@\s]+$/.test(email)) return setMsg(t.invalid);
    const { error } = await supabase.from('feedback_subscriptions').insert({ product_id: productId, email });
    if (error) {
      setMsg(error.code === '23505' ? t.done : t.err);
    } else {
      setOk(true);
      setMsg(t.done);
    }
  }

  return (
    <div>
      <form class="subscribe" onSubmit={submit}>
        <input type="email" required placeholder={t.placeholder} value={email} onInput={(e) => setEmail((e.target as HTMLInputElement).value)} />
        <button class="btn btn--dark btn--big" type="submit">{t.cta}</button>
      </form>
      {msg && <p class="sub-note t-caption" style={ok || msg === t.done ? 'color:var(--c-accent-deep);' : ''}>{msg}</p>}
      {!msg && <p class="sub-note t-caption">{t.note}</p>}
    </div>
  );
}
