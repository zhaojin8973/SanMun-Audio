-- ============================================================
-- SanMu 官网留言区 — 初始迁移
-- 用法：Supabase 项目 → SQL Editor 里整段执行；或 supabase db push
-- 配套设置见 supabase/SETUP.md（邮箱验证关闭、Webhook、通知函数）
-- ============================================================

create extension if not exists "pgcrypto";

-- ── 枚举 ──
create type public.comment_type  as enum ('request', 'bug', 'feedback', 'chat');
create type public.comment_status as enum ('pending', 'published', 'resolved', 'fixed', 'accepted');

-- ── 用户资料（auth.users 触发自动创建） ──
create table public.profiles (
  id           uuid primary key references auth.users (id) on delete cascade,
  display_name text not null default '',
  is_admin     boolean not null default false,
  created_at   timestamptz not null default now()
);

-- ── 留言 ──
create table public.comments (
  id         uuid primary key default gen_random_uuid(),
  product_id text not null check (product_id in ('eq6', 'comp', 'reverb', 'delay', 'suite')),
  parent_id  uuid references public.comments (id) on delete cascade,  -- 官方回复挂在 parent
  type       public.comment_type not null default 'feedback',
  title      text,
  body       text not null,
  status     public.comment_status not null default 'pending',
  is_pinned  boolean not null default false,
  author_id  uuid not null references public.profiles (id) on delete cascade,
  vote_count integer not null default 0,
  created_at timestamptz not null default now(),
  updated_at timestamptz not null default now()
);
create index comments_product_idx   on public.comments (product_id, status, created_at desc);
create index comments_author_idx    on public.comments (author_id);
create index comments_parent_idx    on public.comments (parent_id);

-- ── 投票 ──
create table public.votes (
  comment_id uuid not null references public.comments (id) on delete cascade,
  user_id    uuid not null references public.profiles (id) on delete cascade,
  created_at timestamptz not null default now(),
  primary key (comment_id, user_id)
);

-- ── Coming Soon 订阅 ──
create table public.feedback_subscriptions (
  id         uuid primary key default gen_random_uuid(),
  product_id text not null,
  email      text not null,
  created_at timestamptz not null default now(),
  unique (product_id, email)
);

-- ── 新用户自动建 profile ──
create or replace function public.handle_new_user()
returns trigger language plpgsql security definer set search_path = public as $$
begin
  insert into public.profiles (id) values (new.id) on conflict do nothing;
  return new;
end $$;

create trigger on_auth_user_created
  after insert on auth.users
  for each row execute function public.handle_new_user();

-- ── 审核规则：首发言待审，有已公开留言的作者免审 ──
create or replace function public.set_comment_status()
returns trigger language plpgsql security definer set search_path = public as $$
begin
  if exists (
    select 1 from public.comments c
    where c.author_id = new.author_id and c.status <> 'pending'
  ) then
    new.status := 'published';
  else
    new.status := 'pending';
  end if;
  return new;
end $$;

create trigger trg_comment_status
  before insert on public.comments
  for each row execute function public.set_comment_status();

-- ── 管理员判定 ──
create or replace function public.is_admin()
returns boolean language sql stable security definer set search_path = public as $$
  select coalesce((select is_admin from public.profiles where id = auth.uid()), false);
$$;

-- ── 投票 toggle（原子增减） ──
create or replace function public.toggle_vote(target uuid)
returns void language plpgsql security definer set search_path = public as $$
declare uid uuid := auth.uid();
begin
  if uid is null then
    raise exception 'login required';
  end if;
  if exists (select 1 from public.votes where comment_id = target and user_id = uid) then
    delete from public.votes where comment_id = target and user_id = uid;
    update public.comments set vote_count = vote_count - 1 where id = target;
  else
    insert into public.votes (comment_id, user_id) values (target, uid);
    update public.comments set vote_count = vote_count + 1 where id = target;
  end if;
end $$;

-- ── 行级安全 ──
alter table public.profiles enable row level security;
alter table public.comments enable row level security;
alter table public.votes enable row level security;
alter table public.feedback_subscriptions enable row level security;

create policy profiles_self
  on public.profiles for all
  using (id = auth.uid())
  with check (id = auth.uid());

-- 公开可见：非 pending；作者可见自己的待审；管理员全见
create policy comments_read
  on public.comments for select
  using (status <> 'pending' or author_id = auth.uid() or public.is_admin());

create policy comments_insert
  on public.comments for insert
  with check (author_id = auth.uid());

create policy comments_update_admin
  on public.comments for update
  using (public.is_admin());

-- 作者可删自己的留言；管理员可删任何
create policy comments_delete
  on public.comments for delete
  using (public.is_admin() or author_id = auth.uid());

create policy votes_write
  on public.votes for insert
  with check (user_id = auth.uid());

create policy votes_delete
  on public.votes for delete
  using (user_id = auth.uid());

create policy votes_read
  on public.votes for select
  using (user_id = auth.uid());

create policy subs_insert
  on public.feedback_subscriptions for insert
  with check (true);

create policy subs_read
  on public.feedback_subscriptions for select
  using (public.is_admin());
