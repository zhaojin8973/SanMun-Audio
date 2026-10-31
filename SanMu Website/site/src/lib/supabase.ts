import { createClient, type SupabaseClient } from '@supabase/supabase-js';

const url = import.meta.env.PUBLIC_SUPABASE_URL as string | undefined;
const key = import.meta.env.PUBLIC_SUPABASE_ANON_KEY as string | undefined;

// 本地占位地址不能被当成线上留言已接通；生产构建必须注入正式 Supabase URL。
const isLocalPlaceholder = !!url && /^(https?:\/\/)?(127\.0\.0\.1|localhost)(:\d+)?/.test(url);

/** 未配置环境变量时为 null —— 留言区降级为示意态，站点照常构建 */
export const supabase: SupabaseClient | null =
  url && key && key.length > 20 && !isLocalPlaceholder ? createClient(url, key) : null;

export const supabaseConfigured = supabase !== null;

export type CommentStatus = 'pending' | 'published' | 'resolved' | 'fixed' | 'accepted';
export type CommentType = 'request' | 'bug' | 'feedback' | 'chat';

export interface CommentRow {
  id: string;
  product_id: string;
  parent_id: string | null;
  type: CommentType;
  title: string | null;
  body: string;
  status: CommentStatus;
  is_pinned: boolean;
  author_id: string;
  vote_count: number;
  created_at: string;
  profiles: { display_name: string; is_admin: boolean } | null;
  replies: CommentRow[];
}
