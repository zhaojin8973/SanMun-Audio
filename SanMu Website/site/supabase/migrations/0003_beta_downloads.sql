-- SanMu Audio 内测下载：私有对象存储 + 登录用户可读的发布清单
-- 上传包前请在 Storage 创建 private bucket：beta-downloads。

create table if not exists public.beta_artifacts (
  id uuid primary key default gen_random_uuid(),
  release_id text not null,
  product_slug text not null check (product_slug in ('eq6', 'comp', 'delay', 'reverb')),
  product_name text not null,
  platform text not null check (platform in ('macOS', 'Windows')),
  version text not null,
  package_name text not null,
  object_path text not null unique,
  status text not null default 'pending' check (status in ('ready', 'pending', 'withdrawn')),
  size_bytes bigint,
  sha256 text,
  min_system text not null,
  formats text[] not null default '{}',
  created_at timestamptz not null default now(),
  updated_at timestamptz not null default now()
);

create index if not exists beta_artifacts_release_idx on public.beta_artifacts (release_id, status, platform);
alter table public.beta_artifacts enable row level security;

drop policy if exists beta_artifacts_read_authenticated on public.beta_artifacts;
create policy beta_artifacts_read_authenticated
  on public.beta_artifacts for select
  to authenticated
  using (status = 'ready');

insert into storage.buckets (id, name, public)
values ('beta-downloads', 'beta-downloads', false)
on conflict (id) do nothing;

drop policy if exists beta_downloads_read_authenticated on storage.objects;
create policy beta_downloads_read_authenticated
  on storage.objects for select
  to authenticated
  using (bucket_id = 'beta-downloads');

-- 示例：管理员上传后把对应对象登记为 ready。
-- insert into public.beta_artifacts (release_id, product_slug, product_name, platform, version, package_name, object_path, status, min_system, formats)
-- values ('internal-beta-20261002', 'eq6', 'SanMu EQ6', 'macOS', '0.1.5', 'SanMu_EQ6_v0.1.5_Beta_Until20261031.pkg', 'internal-beta-20261002/macos/SanMu_EQ6_v0.1.5_Beta_Until20261031.pkg', 'ready', 'macOS 10.15+ / Apple silicon 11.0+', '{VST3,AU,AAX}');
