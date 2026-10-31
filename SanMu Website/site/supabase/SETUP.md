# P3 留言区 · Supabase 部署指南

本地开发与上线前的云端配置步骤。**你只需要做第 1–4.1 步**（约 20 分钟），其余我已写好。

## 1. 创建 Supabase 项目

1. 打开 https://supabase.com → 用 GitHub 登录 → New project。
2. 名称 `sanmu-site`，数据库密码自己记好，区域选离用户近的（如 Tokyo / Singapore / US West）。
3. 免费层即可（500MB 数据库、50k 月活，对留言区绰绰有余）。

## 2. 执行数据库迁移

Dashboard → SQL Editor → New query → 把 `supabase/migrations/0001_init.sql` 整段粘贴 → Run。
（成功标志：左侧出现 `public.comments` / `profiles` / `votes` / `feedback_subscriptions` 四张表。）

## 3. 关闭邮箱验证（按你拍板的口径：邮箱+密码，无需验证码）

Dashboard → Authentication → Providers → Email：
- 关闭 **Confirm email**（默认是开的，必须关掉，否则注册后收不到验证码无法登录）。

## 4. 把密钥填进站点

1. Dashboard → Project Settings → API：
   - 复制 **Project URL** 与 **anon public key**。
2. 在 `site/` 下创建 `.env`（不要提交到 git）：

```
PUBLIC_SUPABASE_URL=https://xxxx.supabase.co
PUBLIC_SUPABASE_ANON_KEY=eyJhbGciOi...
```

3. 重启 `npm run dev`（或重新 `npm run build`）。

## 4.1 启用注册用户下载

下载中心使用独立的私有 Storage，不把安装包放进网站静态目录：

1. 在 SQL Editor 执行 `supabase/migrations/0003_beta_downloads.sql`。
2. 在 Storage → `beta-downloads` 上传已经签名并完成验证的安装包；建议按 `internal-beta-20261002/macos/` 或 `internal-beta-20261002/windows/` 分目录。
3. 在 `public.beta_artifacts` 中登记每个文件的 `release_id`、`product_slug`、`platform`、`version`、`package_name`、`object_path`、`sha256` 和 `size_bytes`，确认后将 `status` 设为 `ready`。
4. 先用一个普通账号登录 `/zh/downloads`，确认能看到对应版本并生成短时签名链接；未登记或非 `ready` 的文件会继续显示“待上传或待验证”。

管理员上传建议直接使用 Supabase Dashboard 或受保护的服务端脚本，绝不把 `service_role` 密钥放进 Astro 前端或 `.env` 的 `PUBLIC_` 变量。

## 5. 把自己设为管理员（第一次必做）

在站点留言区注册你的账号（或直接在 Dashboard → Authentication → Users 建用户），然后 SQL Editor 执行：

```sql
update public.profiles set is_admin = true where id = '<你的 user id>';
```

user id 在 Authentication → Users 里复制。之后访问 `/admin` 登录即可看到待审队列。

## 6. 邮件通知（可选，P4 前配好即可）

1. 注册 https://resend.com，验证域名 sanmuaudio.com，拿到 API key。
2. 安装 Supabase CLI 并登录：`npx supabase login`。
3. 部署函数：

```
npx supabase functions deploy notify-admin --no-verify-jwt --project-ref <你的 project ref>
npx supabase secrets set RESEND_API_KEY=re_xxx ADMIN_EMAIL=contact@sanmuaudio.com --project-ref <ref>
```

4. Dashboard → Database → Webhooks → Create trigger：Table `comments`、Events `INSERT`、Type `Supabase Edge Functions`、Function `notify-admin`。

## 本地测试要点

- 未配置 `.env` 时站点照常构建，留言区显示示意态（不报错）。
- 配置后：产品页底部可注册/登录/发留言/投票；首条留言显示"待审核"，审核通过后（/admin）公开并免审。
- 投票：同一账号重复点 = 取消。

## 数据口径速查

| 表 | 用途 |
|---|---|
| profiles | 昵称、is_admin（管理员开关） |
| comments | product_id ∈ eq6/comp/reverb/delay；status ∈ pending/published/resolved/fixed/accepted；parent_id 用于官方回复 |
| votes | (comment_id, user_id) 主键 = 一人一票；计数冗余在 comments.vote_count |
| feedback_subscriptions | Coming Soon 邮箱订阅 |
