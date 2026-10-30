// 帮助内容与音频处理分离；只说明当前可用的控件和行为。
const sections = [
  ['common', ['Shared controls', '共用控件与信号流'], [
    [['Makeup / Gain / Output and Trim', '补偿增益与输出微调'], ['For VCA, FET and Opto: compression → Makeup / Gain / Output → Mix with the original signal → Output trim. The main gain adjusts only the compressed branch; Trim adjusts the entire blend. First balance the compressed sound, then set Mix, then use Trim to match the overall level.', 'VCA、FET、Opto：压缩 → 补偿增益／Gain／Output → Mix 与原声混合 → Trim 输出微调。主增益只调整压缩声，Trim 调整整个混合结果。先整理压缩声的音量，再调整混合比例，最后用 Trim 对齐整体音量。']],
    [['Mix at 0%, in between and 100%', '混合比例与增益的关系'], ['At Mix 100%, the main gain and Trim both change the compressed output level. Below 100%, the main gain changes the balance between original and compressed sound, while Trim keeps that balance. At 0%, the main gain has no audible effect but Trim still works. Bypass restores the input and bypasses Trim too.', 'Mix 为 100% 时，主增益与 Trim 都调整压缩后的音量；小于 100% 时，主增益改变压缩声相对原声的分量，Trim 保持两者比例并整体升降。0% 时主增益不影响听到的声音，但 Trim 仍有效。旁通恢复输入信号，也跳过 Trim。']],
    [['Mix', '混合'], ['0% is original sound; 100% is processed sound. Lower Mix to retain more of the original attacks while blending in compression. LEV also blends original and processed sound, but has no separate Trim control.', '0% 为原声，100% 为处理声。降低混合比例，可以在加入压缩的同时保留更多原声起音。LEV 同样可混合原声与处理声，但没有独立 Trim 旋钮。']],
    [['Output trim', '输出微调'], ['Available in VCA, FET and Opto, from −18 to +18 dB. Adjusts final loudness after Mix, without changing the compressor’s detection or GR. It is not a limiter: positive gain can raise the output above 0 dBFS. Compare levels with bypass and watch OUT.', 'VCA、FET、Opto 提供 −18～+18 dB 的最终音量调节，位于 Mix 之后，不改变压缩检测或 GR。它不是限幅器，正增益可能使输出超过 0 dBFS。可对照旁通前后的音量，并观察 OUT 电平。']],
    [['Stereo link', '立体声联动'], ['Both channels share the same threshold, ratio and timing settings. At 0%, each channel responds to its own signal; at 100%, detection is linked for a steadier stereo image. Intermediate values blend these behaviours. There are no separate left/right parameter panels. Start fully linked when stereo position matters; lower it if each side should respond more independently.', '左右始终共用阈值、比例和时间设置。0% 时各自根据本声道信号压缩；100% 时联动检测，让声像更稳定；中间值折中。这里不提供左右两套参数面板。重视立体声位置时可从全联动开始，需要两边更独立响应时再降低。']],
    [['What independent response means', '左右各自响应的含义'], ['With the same threshold on both sides, a loud hit on the left can cause more reduction on the left than on the right. You still adjust both sides with one set of knobs. VCA links using combined channel energy; FET and Opto follow the stronger side. Link has no stereo effect on mono input. GR shows the larger channel reduction, not separate left/right values.', '例如左右阈值相同，左边出现大声鼓点时，左边可以压得更多，右边压得更少；旋钮仍同时设置两边。VCA 联动检测综合左右能量，FET 和 Opto 参考较强一侧。单声道输入没有左右联动效果。当前 GR 显示左右较大的压缩量，不分别显示两边。']],
    [['Sidechain HP', '侧链高通'], ['Filters low frequencies only in the signal used to trigger compression. Raise it if bass or kick drums make the whole signal dip too much. It does not directly remove bass from the audible signal. OFF disables the filter.', '只过滤用于触发压缩的检测信号中的低频。底鼓或低音让整体被压得太多时，可提高截止频率。它不直接切除听到的低频；OFF 关闭过滤。']],
  ]],
  ['lev', ['LEV · Adaptive leveller', 'LEV · 自适应平准'], [
    [['Compression', '压缩量'], ['Turn clockwise for stronger levelling. This also includes internal level compensation, so it is not a target-GR readout. Start low, increase until loud and quiet parts are more even, then set Gain by ear.', '顺时针增加平准强度，同时包含内部电平补偿；旋钮数值不是目标 GR。先从较小压缩量开始，调到强弱段更均匀，再用输出增益调整音量。']],
    [['Gain', '输出增益'], ['Adjusts the processed branch from −36 to 0 dB before Mix. Lower it if levelling makes the processed sound too loud. It does not boost above 0 dB.', '在 Mix 前调整处理声的音量，范围 −36～0 dB。平准后的声音过大时可降低；此旋钮不提供高于 0 dB 的增益。']],
    [['Gate', '门限'], ['Reduces quiet background sound below the threshold. Raise it gradually during a quiet passage; back it off if word endings or soft notes are lost. OFF disables this attenuation.', '压低低于门限的安静背景声。可在安静段逐渐提高；若尾字或弱音被吃掉，就降低门限。OFF 关闭这一衰减。']],
    [['Smooth', '平滑'], ['Off keeps zero latency. On adds about 12 ms of latency to smooth gain changes, including peak protection. Choose it when smoother gain movement matters more than immediate monitoring response.', '关闭保持零延迟；开启增加约 12 毫秒延迟，平滑主体与峰值保护的增益变化。更重视平滑过渡、且能接受监听延迟时可开启。']],
  ]],
  ['vca', ['VCA · Bus compressor', 'VCA · 总线压缩'], [
    [['Threshold', '阈值'], ['Lower it to make compression engage more readily. Watch GR while setting the desired amount; then compare the output level with the original.', '降低阈值，让压缩更容易介入。观察 GR 调整压缩程度，再与原声音量对照。']],
    [['Ratio', '比例'], ['Higher ratios restrain above-threshold sound more strongly. Start with a low ratio for gentle control; increase it for firmer restraint. The 2 / 4 / 10 buttons select common values.', '比例越高，超过阈值的部分压得越强。温和整理可从低比例开始，需要更强约束时提高。2／4／10 按钮用于快速选择常用值。']],
    [['Attack', '启动'], ['Controls how quickly reduction builds. Shorter values catch more of the attack; longer values let more through. Range: 0.01–300 ms, default 30 ms. The slow end is an extension for gradual control of sustained sound; it is not a delay before compression begins, nor a promise of complete response within that time.', '控制压缩量建立的速度。越短越能约束起音，越长越保留起音。范围 0.01～300 ms，默认 30 ms；慢端用于更缓慢地控制持续声。它不是等待这段时间后才开始压缩，也不表示整机必定在该时间内完成响应。']],
    [['Release / AUTO', '释放／自动'], ['Controls recovery after the signal becomes quieter. Short values recover quickly but can make level movement more obvious; long values recover gently but may hold down the next note. AUTO adapts recovery to the signal. The current control displays AUTO at 1200 ms and above.', '控制信号变轻后的恢复速度。短释放恢复快，但音量起伏可能更明显；长释放更缓慢，但可能压住下一拍。AUTO 随信号调整恢复；当前控制到 1200 ms 及以上显示 AUTO。']],
    [['Makeup', '补偿增益'], ['Raises or lowers only the compressed branch before Mix. Use it to balance the compressed sound against the original. In this implementation it does not drive extra compression or output colour; use Trim for the final blend’s volume.', '只调整 Mix 前的压缩声，用来补回压缩后的音量。在当前实现中，它不额外驱动压缩或染色；混合后的总音量交给 Trim 调整。']],
  ]],
  ['fet', ['FET · Fast compressor', 'FET · 快速压缩'], [
    [['Input / Output', '输入／输出'], ['Turn Input clockwise to drive compression harder; use Output to balance the processed level before Mix. Their displayed 48→0 scales indicate attenuation, so clockwise raises the signal even as the numbers fall. These are control scales, not measured input/output levels.', '顺时针提高输入，驱动更多压缩；用输出旋钮平衡 Mix 前的处理声音量。两者显示 48→0 的衰减刻度，所以顺时针时数字下降、声音反而提高。这是控制刻度，不是实测输入／输出电平。']],
    [['Attack / Release', '启动／释放'], ['Both use a 1–7 scale: 1 is slowest and 7 fastest. Attack shapes how much of the initial hit passes; Release shapes recovery between hits. Tune them while listening to the rhythm and watching GR.', '两者均使用 1～7 刻度：1 最慢，7 最快。启动决定放过多少起音，释放决定拍间恢复。可边听节奏边观察 GR 调整。']],
    [['Ratio / ALL', '比例／ALL'], ['4 / 8 / 12 / 20 select increasing compression ratios. ALL is a separate, more aggressive setting; it is not a sum of the four ratios. Use it when stronger dynamic shaping is intended.', '4／8／12／20 依次提高压缩比例。ALL 是更激烈的独立设置，不是四个比例相加。需要更强的动态塑形时可尝试。']],
    [['Headroom', '余量'], ['Higher values allow more input before compression; lower values make compression easier to trigger. It changes the compressor’s working point, not the final output volume.', '提高余量，让更高的输入电平才触发压缩；降低则更容易压缩。它改变压缩工作点，不是最终输出音量。']],
    [['Input / output link', '输入／输出联动'], ['When enabled, editing either knob adjusts the other in the opposite direction. This helps compare input drive settings but does not guarantee matched loudness. Check the level with Output or Trim after adjusting.', '开启后，手动调整其中一个旋钮会反向调整另一个，方便比较不同输入驱动量，但不保证严格等响。调完仍可用输出或 Trim 校对音量。']],
  ]],
  ['opto', ['Opto · Optical compressor', 'Opto · 光电压缩'], [
    [['Peak reduction', '压缩量'], ['Turn up to increase compression. Attack and recovery adapt internally; there are no manual timing knobs. Listen to sustained notes and phrase endings while setting the amount.', '调大增加压缩。启动与恢复由内部动态控制，没有手动时间旋钮。可结合持续音和乐句尾部调整压缩程度。']],
    [['Gain', '增益'], ['Adjusts the processed level before Mix, independently of Peak reduction. Set compression first, then balance the level with Gain. The 0–100 scale is a control scale, not decibels.', '独立于压缩量，调整 Mix 前的处理声音量。先设定压缩，再用增益平衡音量。0～100 是控制刻度，不是分贝。']],
    [['Compress / Limit', '压缩／限制'], ['Compress gives gentler restraint; Limit applies stronger restraint to loud passages. Limit is not a guaranteed output ceiling. Use Gain and Trim to set the output level.', '压缩模式更温和，限制模式对大声部分约束更强。限制模式并不保证输出绝不越过某个上限；输出音量仍需用增益与 Trim 调整。']],
  ]],
];

export function renderManual(root, language, mode) {
  const pick = pair => pair[language === 'zh' ? 1 : 0];
  root.replaceChildren();
  for (const [id, title, entries] of sections) {
    const section = document.createElement('details');
    section.className = 'manual-section';
    section.dataset.manual = id;
    section.open = id === 'common' || id === ['lev', 'vca', 'fet', 'opto'][mode];
    const summary = document.createElement('summary');
    summary.textContent = pick(title);
    section.append(summary);
    const list = document.createElement('dl');
    list.className = 'help-list';
    for (const [label, description] of entries) {
      const entry = document.createElement('div');
      const term = document.createElement('dt'), definition = document.createElement('dd');
      term.textContent = pick(label); definition.textContent = pick(description);
      entry.append(term, definition); list.append(entry);
    }
    section.append(list); root.append(section);
  }
}
