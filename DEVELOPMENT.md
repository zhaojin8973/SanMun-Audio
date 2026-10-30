# 开发与发布约束

## 单一源码入口

四个插件源码和共享网页兼容层只在本根仓库维护。不要在 Windows checkout 复制一份长期维护的工程；Windows 只使用同一个 Git 提交进行构建和宿主测试。

## 凭据边界

不要提交 Apple 钥匙串、PACE 密码、App 专用密码、私钥、AAX SDK、安装包或公证日志。PACE 发布脚本只从环境变量或本机钥匙串读取凭据。已有旧本地历史中曾出现过凭据，凭据必须在 Apple/PACE 侧轮换后再进行任何公开同步。

## Windows 产物边界

GitHub Actions 使用 Windows 2022 生成未签名 VST3 候选 ZIP。它不执行 PACE、Authenticode，也不替代 Cubase、REAPER、Studio One 或 Pro Tools 的 Windows 实机扫描。正式 Windows 发布必须在 Windows 签名环境中单独完成。
