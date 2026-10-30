// ==============================================================================
// tooltips.js - SanMu Delay 双语提示系统
// ==============================================================================

export const tooltips = {
"Increase": {"en": "Increase", "zh": "延长时间或提高速度"},
"Decrease": {"en": "Decrease", "zh": "缩短时间或降低速度"},
"Close": {"en": "Close", "zh": "关闭窗口"},
"Clear peaks": {"en": "Clear peaks", "zh": "清除峰值保持"},
"Save preset": {"en": "Save preset", "zh": "保存当前声音设置"},
"Browse presets": {"en": "Browse presets", "zh": "浏览预设"},
"Next preset": {"en": "Next preset", "zh": "下一个预设"},
"Previous preset": {"en": "Previous preset", "zh": "上一个预设"},
    'tip:duck_external': {"en": "Use the host sidechain; falls back to dry input if unavailable.", "zh": "用宿主侧链控制避让；无侧链时使用干声。"},
    'tip:duck_internal': {"en": "Use the dry input to lower the repeats.", "zh": "用当前干声控制回声避让。"},
    'tip:echo_feedback': {"en": "Set repeat decay; this is a parameter model, not an audio waveform.", "zh": "调整回声衰减；图形为参数示意。"},
    'tip:echo_time': {"en": "Set the delay interval; linked channels share the left control.", "zh": "设置回声间隔；联动时由左路统一控制。"},
    'tip:manual_bpm': {"en": "SYNC follows the host. BPM lets you set the tempo. FREE ignores tempo.", "zh": "SYNC 跟随宿主；BPM 手动调速；FREE 不使用速度。"},
    'tip:modern_clarity': {"en": "Digital clarity preference for the Modern layout; it does not change the audio engine.", "zh": "Modern 的数字清晰度界面偏好；不改变音频引擎。"},
    'tip:modern_mod_mix': {"en": "Sets how much delay time is modulated in Modern mode.", "zh": "调整 Modern 模式中延迟时间的调制比例。"},
    'tip:duck_meter': {"en": "Current ducking reduction; the bar spans 0–24 dB.", "zh": "显示当前回声避让量；条形量程为 0～24 dB。"},
    'tip:engine_0': {"en": "Clean, precise digital repeats.", "zh": "清晰、准确的数字回声。"},
    'tip:engine_1': {"en": "Tape-style saturation and pitch movement.", "zh": "带饱和与音高漂移的磁带回声。"},
    'tip:engine_2': {"en": "Warm, darker analogue-style repeats.", "zh": "温暖、偏暗的模拟回声。"},
    'tip:engine_3': {"en": "Softens separate repeats into a diffuse tail.", "zh": "将独立回声柔化为扩散尾音。"},
    'tip:topology_0': {
        en: 'Independent: Separate Left and Right delay lines; Link can join their timing.',
        zh: 'Independent：左右声道分别反馈；开启 LINK 后可同步时间。'
    },
    'tip:topology_1': {
        en: 'Ping-Pong: The first repeat starts left, then echoes alternate Left and Right.',
        zh: 'Ping-Pong：首个回声从左侧开始，之后左右交替反弹。'
    },
    'tip:topology_2': {
        en: 'Cross: Stereo crossfeed delay lines where left feeds right and right feeds left.',
        zh: 'Cross：立体声交叉互馈拓扑，左右声道回声相互交织融合。'
    },
    'tip:sync_0': {
        en: 'Sync: Locks delay times to the host DAW tempo with musical subdivisions.',
        zh: 'Sync：锁定宿主工程速度，以标准音乐节拍时值切分。'
    },
    'tip:sync_1': {
        en: 'BPM: Set custom BPM tempo manually, independent of host tempo.',
        zh: 'BPM：手动指定速度（BPM），独立于宿主走带速度。'
    },
    'tip:sync_2': {
        en: 'Free: Continuous millisecond delay times (1.0 ms – 2000.0 ms).',
        zh: 'Free：自由毫秒模式（1.0 ms – 2000.0 ms），不受节拍约束。'
    },
    'tip:time_l': {
        en: 'Delay Time (Left): Sets the delay duration or subdivision for the left channel.',
        zh: '延迟时间 (L)：设置左声道的延迟时长或音符切分。'
    },
    'tip:time_r': {
        en: 'Delay Time (Right): Sets the delay duration or subdivision for the right channel.',
        zh: '延迟时间 (R)：设置右声道的延迟时长或音符切分。'
    },
    'tip:time_link': {
        en: 'Link L/R: Links Left and Right delay controls together.',
        zh: '左右联动：锁定并同步调节左右两声道的延迟参数。'
    },
    'tip:feedback': {"en": "Higher values keep the repeats going longer.", "zh": "数值越大，回声持续越久。"},
    'tip:freeze': {"en": "Recirculate existing echoes and stop adding new input.", "zh": "循环已有回声，停止加入新输入。"},
    'tip:damp_lowcut': {"en": "Remove low frequencies from repeating echoes.", "zh": "削减循环回声中的低频。"},
    'tip:damp_highcut': {"en": "Make successive repeats darker.", "zh": "削减循环回声中的高频。"},
    'tip:mod_rate': {"en": "Set how quickly the delay time moves.", "zh": "调整延迟时间起伏的速度。"},
    'tip:mod_depth': {"en": "Set the amount of pitch and time movement.", "zh": "调整时间与音高起伏的幅度。"},
    'tip:tape_drive': {
        en: 'Tape Drive: Magnetic tape input saturation and gentle soft-clipping distortion.',
        zh: '磁带驱动：磁带软饱和与染色程度。'
    },
    'tip:tape_wow': {
        en: 'Tape Wow: Motor instability producing pitch wow and flutter fluctuations.',
        zh: '磁带抖晃：磁带机机械马达微抖动引起的音高漂移。'
    },
    'tip:bbd_warmth': {
        en: 'BBD Warmth: Analog compander and low-voltage warmth characteristic of bucket-brigade chips.',
        zh: 'BBD 暖色：桶链芯片专属的模拟压扩与温润暖色。'
    },
    'tip:diffusion_amount': {
        en: 'Diffusion: Smears distinct echo taps into a dense continuous reverberant cloud.',
        zh: '扩散度：将离散回声脉冲揉碎为连续扩散的混响云雾。'
    },
    'tip:duck_enable': {
        en: 'Ducking: Automatically attenuates wet delay when dry signal or sidechain is present.',
        zh: 'Ducking 避让：当有干声信号或外部侧链输入时，自动衰减延迟湿声。'
    },
    'tip:duck_threshold': {
        en: 'Duck Threshold: Sensitivity threshold for ducking attenuation (-60 dB to 0 dB).',
        zh: '避让阈值：触发湿声衰减的电平门限（-60 dB 至 0 dB）。'
    },
    'tip:duck_amount': {
        en: 'Duck Amount: Maximum gain reduction applied to the delay signal (0% to 100%).',
        zh: '避让深度：延迟信号被压低的幅度（0% 至 100%）。'
    },
    'tip:duck_release': {
        en: 'Duck Release: Recovery time for wet delay to return after input drops (50 ms – 2000 ms).',
        zh: '避让释放：信号停止后湿声回弹复原的时间（50 ms – 2000 ms）。'
    },
    'tip:mix': {
        en: 'Mix: Balance between Dry (original) and Wet (delayed) audio (0% – 100%).',
        zh: '干湿比：原始干声与延迟湿声的比例（0% – 100%）。'
    },
    'tip:output_gain': {
        en: 'Output Gain: Final output level adjustment (-24 dB to +24 dB).',
        zh: '输出增益：插件总输出电平调整（-24 dB 至 +24 dB）。'
    },
    'tip:undo': { en: 'Undo last parameter edit', zh: '撤销上一次参数调节' },
    'tip:redo': { en: 'Redo parameter edit', zh: '重做参数调节' },
    'tip:ab': { en: 'Switch between A and B comparison slots', zh: '在 A / B 独立声音槽位之间切换' },
    'tip:copy_ab': { en: 'Copy current settings to the other slot', zh: '将当前槽位的设置复制到另一个槽位' },
    'tip:bypass': { en: 'Master plugin bypass', zh: '插件总旁通（保留原声直接输出）' },
    'tip:settings': { en: 'Settings and preferences', zh: '设置与首选项' }
};
