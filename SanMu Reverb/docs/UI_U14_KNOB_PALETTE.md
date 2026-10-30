# U14 旋钮配色微调

本次只调整旋钮材质色，模块胶囊的 Hall 绿色与其他类型色值保持不变。

- 浅色旋钮：使用青灰激活弧线、指针与刻度，避免 Hall 旋钮呈草绿色。
- 深色旋钮：旋钮主体改为接近顶部控制条的深灰玻璃，激活弧线使用低亮度紫灰，指针和刻度使用冷灰紫。
- 新增 `--knob-accent`、`--knob-pointer`、`--knob-tick`、`--knob-track`，与模块状态色解耦。

验证：`npm run test:glass`、`npm run test:studio`、`npm run build` 均通过。
