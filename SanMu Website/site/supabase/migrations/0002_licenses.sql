-- ============================================================
-- P5 授权码系统（预留迁移）—— 购买环节启动时执行
-- 现在不执行也不影响任何功能；执行后 /admin 可发码。
-- 设计依据：docs/01-网站方案_v1.md §2.1「轻量授权码」路线
-- ============================================================

-- 授权码
create table public.licenses (
  id              uuid primary key default gen_random_uuid(),
  code            text not null unique,              -- 形如 SMEQ-XXXX-XXXX-XXXX
  product_id      text not null check (product_id in ('eq6','comp','reverb','bundle')),
  user_id         uuid references public.profiles (id) on delete set null,
  email           text,                              -- 购买邮箱
  status          text not null default 'active' check (status in ('active','revoked')),
  max_activations integer not null default 3,        -- 最多几台机器
  created_at      timestamptz not null default now(),
  note            text
);

-- 激活记录（机器绑定）
create table public.activations (
  id         uuid primary key default gen_random_uuid(),
  license_id uuid not null references public.licenses (id) on delete cascade,
  machine_id text not null,                          -- 插件端机器指纹哈希
  created_at timestamptz not null default now(),
  unique (license_id, machine_id)
);
create index licenses_code_idx on public.licenses (code);

-- 生成批量授权码（仅管理员）：select public.generate_licenses('eq6', 5)
create or replace function public.generate_licenses(p_product text, n integer)
returns setof text language plpgsql security definer set search_path = public as $$
declare i integer;
begin
  if not public.is_admin() then
    raise exception 'admin only';
  end if;
  for i in 1..n loop
    return next
      upper(p_product) || '-' ||
      upper(substr(md5(random()::text || clock_timestamp()::text), 1, 4)) || '-' ||
      upper(substr(md5(random()::text || clock_timestamp()::text), 1, 4)) || '-' ||
      upper(substr(md5(random()::text || clock_timestamp()::text), 1, 4));
  end loop;
end $$;

-- 在线校验（插件激活时调用）：返回可激活剩余额度
-- 插件端逻辑：授权码 + 机器指纹 → 服务器；离线时用本地签名文件兜底（见 P5 方案）
create or replace function public.activate_license(p_code text, p_machine text)
returns json language plpgsql security definer set search_path = public as $$
declare lic record;
begin
  select * into lic from public.licenses where code = p_code and status = 'active';
  if lic is null then
    return json_build_object('ok', false, 'reason', 'invalid');
  end if;
  insert into public.activations (license_id, machine_id) values (lic.id, p_machine)
    on conflict do nothing;
  if (select count(*) from public.activations where license_id = lic.id) > lic.max_activations then
    delete from public.activations where license_id = lic.id and machine_id = p_machine;
    return json_build_object('ok', false, 'reason', 'limit');
  end if;
  return json_build_object('ok', true, 'product', lic.product_id);
end $$;

alter table public.licenses enable row level security;
alter table public.activations enable row level security;

create policy licenses_admin on public.licenses for all using (public.is_admin());
create policy licenses_self_read on public.licenses for select using (user_id = auth.uid());
create policy activations_admin on public.activations for all using (public.is_admin());
