// 双轨时间编辑器：共用真实参数，不生成装饰动画或虚构音频波形。
export const beats = [4, 3, 2, 4/3, 1.5, 1, 2/3, .75, .5, 1/3, .375, .25, 1/6, .125];
export const divisions = ['1/1','1/2D','1/2','1/2T','1/4D','1/4','1/4T','1/8D','1/8','1/8T','1/16D','1/16','1/16T','1/32'];
const clamp = (v, lo, hi) => Math.max(lo, Math.min(hi, v));
const text = (el, value) => { if (el.textContent !== value) el.textContent = value; };

export class EchoTimeEditor {
    constructor(container, app) {
        this.app = app;
        this.container = container;
        container.innerHTML = `
            <div class="echo-heading">
                <div class="echo-heading-copy"><strong id="echo-title"></strong><span id="echo-context"></span></div>
                <div class="echo-precision">
                    <span class="precision-label">PRECISION</span>
                    <fieldset class="styles sync-group" aria-label="Time mode">
                        <label data-tip="tip:sync_0"><input type="radio" name="sync_mode" value="0"><span>SYNC</span></label>
                        <label data-tip="tip:sync_1"><input type="radio" name="sync_mode" value="1" checked><span>BPM</span></label>
                        <label data-tip="tip:sync_2"><input type="radio" name="sync_mode" value="2"><span>FREE</span></label>
                    </fieldset>
                    <button id="btn-time-link" class="action-toggle" aria-pressed="true" data-tip="tip:time_link"><span>LINK</span></button>
                </div>
            </div>
            <div class="echo-workspace">
                <div class="echo-timing">
                    <div class="echo-lane" data-channel="L"><span class="echo-channel">L</span><div class="echo-rail"><div class="echo-length"></div><button id="echo-time-l" class="echo-handle" role="slider" data-tip="tip:echo_time"><span></span></button></div><output></output></div>
                    <div class="echo-lane" data-channel="R"><span class="echo-channel">R</span><div class="echo-rail"><div class="echo-length"></div><button id="echo-time-r" class="echo-handle" role="slider" data-tip="tip:echo_time"><span></span></button></div><output></output></div>
                    <div class="echo-ruler"></div>
                </div>
                <div class="echo-feedback"><div class="echo-feedback-title"><span></span><output></output></div>
                    <div id="echo-feedback-control" role="slider" tabindex="0" data-tip="tip:echo_feedback"><svg viewBox="0 0 200 76" aria-hidden="true"><path class="decay-fill"/><path class="decay-line"/><circle class="decay-handle" r="5"/></svg><div class="repeat-labels"><span>1</span><span>2</span><span>3</span><span>4</span><span>5</span><span>6</span></div></div>
                    <span id="echo-feedback-note"></span>
                </div>
            </div>
            `;
        this.lanes = [...container.querySelectorAll('.echo-lane')];
        for (const lane of this.lanes) {
            const handle = lane.querySelector('.echo-handle');
            const input = document.createElement('input');
            input.className = 'echo-time-input';
            input.id = lane.dataset.channel === 'L' ? 'edit-time-l' : 'edit-time-r';
            input.hidden = true;
            input.autocomplete = 'off';
            lane.querySelector('.echo-rail').appendChild(input);
            this.bindDrag(handle, x => this.setTime(lane, x), lane.querySelector('.echo-rail'));
            handle.addEventListener('dblclick', () => this.editTime(lane));
            handle.addEventListener('keydown', e => {
                if (e.key === 'Enter') { e.preventDefault(); this.editTime(lane); }
                else this.timeKey(e, lane);
            });
            input.addEventListener('blur', () => { input.hidden = true; });
            input.addEventListener('keydown', e => {
                if (e.key === 'Escape') { input.hidden = true; handle.focus(); }
                if (e.key !== 'Enter') return;
                e.preventDefault();
                const m = this.model(), value = input.value.trim();
                const match = value.match(/^([+]?(?:\d+(?:\.\d*)?|\.\d+))\s*(ms|s)?$/i);
                const next = m.free ? (match ? Number(match[1]) * (match[2]?.toLowerCase() === 's' ? 1000 : 1) : NaN)
                    : divisions.findIndex(note => note.toLowerCase() === value.toLowerCase());
                if (!Number.isFinite(next) || next < (m.free ? 1 : 0) || next > (m.free ? 2000 : 13)) {
                    input.setAttribute('aria-invalid', 'true');
                    this.app.notify(this.app.t('Enter a value within the parameter range.')); return;
                }
                const right = lane.dataset.channel === 'R';
                if (handle.disabled) return;
                input.hidden = true;
                this.app.begin();
                this.app.set(m.free ? (right ? 'time_r' : 'time_l') : (right ? 'division_r' : 'division'), next);
                if (m.linked && !right) this.app.set(m.free ? 'time_r' : 'division_r', next);
                this.app.end();
                handle.focus();
            });
        }
        this.feedbackControl = container.querySelector('#echo-feedback-control');
        this.bindDrag(this.feedbackControl, (_, y) => app.set('feedback', (1-y)*100), this.feedbackControl);
        this.feedbackControl.addEventListener('keydown', e => {
            if (!['ArrowUp','ArrowDown','ArrowLeft','ArrowRight','Home','End'].includes(e.key) || app.values.freeze > .5) return;
            e.preventDefault(); app.begin();
            const next = e.key === 'Home' ? 0 : e.key === 'End' ? 100 : app.values.feedback + (['ArrowUp','ArrowRight'].includes(e.key) ? 1 : -1);
            app.set('feedback', clamp(next,0,100)); app.end();
        });
        this.render();
    }
    model() {
        const v = this.app.values, free = v.sync_mode === 2;
        const bpm = v.sync_mode === 0 ? (this.app.hostBpm || 120) : (v.manual_bpm || 120);
        const linked = v.time_link > .5 || v.topology === 1;
        const rawL = free ? v.time_l : beats[v.division ?? 5];
        const rawR = linked ? rawL : free ? v.time_r : beats[v.division_r ?? 5];
        return {v, free, bpm, linked, rawL, rawR, max: free ? 2000 : 4, min: free ? 1 : .125};
    }
    editTime(lane) {
        const handle = lane.querySelector('.echo-handle'), input = lane.querySelector('.echo-time-input');
        if (handle.disabled) return;
        input.value = handle.querySelector('span').textContent;
        input.removeAttribute('aria-invalid');
        input.hidden = false;
        input.focus(); input.select();
    }
    bindDrag(element, apply, area) {
        let pointer = null;
        element.addEventListener('pointerdown', e => {
            if (e.button !== 0) return;
            e.preventDefault();
            if (element.getAttribute('aria-disabled') === 'true') return;
            element.focus(); pointer = e.pointerId;
            element.setPointerCapture(pointer); this.app.begin();
            // 时间节点拖动采用相对位移，避免抓住标签边缘时突然跳值。
            const rect = area.getBoundingClientRect();
            const m = this.model();
            const raw = element.id === 'echo-time-r' ? m.rawR : m.rawL;
            this.dragStart = { x: e.clientX, y: e.clientY, feedback: this.app.values.feedback, position: (raw-m.min)/(m.max-m.min), rect };

        });
        element.addEventListener('pointermove', e => {
            if (pointer !== e.pointerId) return;
            const {rect,x,y,feedback,position} = this.dragStart;
            if (element === this.feedbackControl) {
                const next = clamp(feedback+(y-e.clientY)/rect.height*100,0,100);
                apply(0,1-next/100); return;
            }
            apply(clamp(position+(e.clientX-x)/rect.width,0,1),clamp((e.clientY-rect.top)/rect.height,0,1));
        });
        const finish = e => { if (pointer !== e.pointerId) return; pointer=null; this.app.end(); };
        ['pointerup','pointercancel','lostpointercapture'].forEach(name=>element.addEventListener(name,finish));
    }
    setTime(lane, position) {
        const m = this.model(), right = lane.dataset.channel === 'R';
        const value = m.min+position*(m.max-m.min);
        const id = m.free ? (right ? 'time_r':'time_l') : (right ? 'division_r':'division');
        const next = m.free ? Math.round(value*10)/10 : beats.reduce((best,b,i)=>Math.abs(b-value)<Math.abs(beats[best]-value)?i:best,0);
        this.app.set(id,next);
        if (m.linked && !right) this.app.set(m.free ? 'time_r':'division_r',next);
    }
    timeKey(e,lane) {
        if (!['ArrowUp','ArrowDown','ArrowLeft','ArrowRight','Home','End'].includes(e.key)) return;
        if (lane.querySelector('button').disabled) return;
        e.preventDefault(); const m=this.model(), right=lane.dataset.channel==='R';
        const id=m.free?(right?'time_r':'time_l'):(right?'division_r':'division');
        const dir=['ArrowUp','ArrowRight'].includes(e.key)?1:-1;
        let next;
        if (m.free) next=e.key==='Home'?1:e.key==='End'?2000:clamp(m.v[id]+dir*10,1,2000);
        else {
            const sorted=beats.map((b,i)=>({b,i})).sort((a,b)=>a.b-b.b);
            const idx=sorted.findIndex(p=>p.i===m.v[id]);
            next=sorted[e.key==='Home'?0:e.key==='End'?13:clamp(idx+dir,0,13)].i;
        }
        this.app.begin(); this.app.set(id,next);
        if(m.linked&&!right) this.app.set(m.free?'time_r':'division_r',next);
        this.app.end();
    }
    render() {
        const m=this.model(), zh=this.app.ui.language==='zh', v=m.v;
        const $=s=>this.container.querySelector(s);
        const key=JSON.stringify([v.time_l,v.time_r,v.division,v.division_r,v.sync_mode,v.time_link,v.topology,v.feedback,v.freeze,m.bpm,zh,this.app.ui.history]);
        if(this.key!==key) {
            this.key=key;
            text($('#echo-title'),zh?'回声时间编辑器':'Echo time editor');
            text($('#echo-context'),m.free?(zh?'自由时间 · ms':'Free time · ms'):`${m.bpm.toFixed(1)} BPM · ${zh?'音符吸附':'Note snap'}`);
            this.lanes.forEach((lane,i)=>{
                const raw=i?m.rawR:m.rawL, handle=lane.querySelector('button');
                const requested=m.free?raw:raw*60000/m.bpm, ms=Math.min(5000,requested);
                const note=m.free?`${ms.toFixed(1)} ms`:divisions[i&&!m.linked?v.division_r:v.division];
                const position=(raw-m.min)/(m.max-m.min)*100;
                handle.style.left=`${position}%`; lane.querySelector('.echo-length').style.width=`${position}%`;
                const input = lane.querySelector('.echo-time-input');
                input.style.left = `${position}%`;
                input.setAttribute('aria-label', zh ? `输入${i?'右':'左'}路时间` : `${i?'Right':'Left'} time entry`);
                if (!input.hidden && (i === 1 && m.linked || this.lastFree !== m.free)) input.hidden = true;
                text(handle.querySelector('span'),note);
                text(lane.querySelector('output'),`${ms.toFixed(1)} ms${requested>5000?' *':''}`);
                handle.disabled=i===1&&m.linked;
                handle.setAttribute('aria-disabled',String(handle.disabled));
                handle.setAttribute('aria-label',zh?`调整${i?'右':'左'}路延迟时间`:`${i?'Right':'Left'} delay time`);
                handle.setAttribute('aria-valuemin',String(m.min));handle.setAttribute('aria-valuemax',String(m.max));handle.setAttribute('aria-valuenow',String(raw));handle.setAttribute('aria-valuetext',`${note} · ${ms.toFixed(1)} ms`);
            });
            this.lastFree = m.free;
            const ticks=m.free?['1','500','1000','1500','2000 ms']:['1/32','1/4','1/2','1/2D','1/1'];
            const ruler=$('.echo-ruler'); ruler.replaceChildren(...ticks.map(t=>{const s=document.createElement('span');s.textContent=t; const values=m.free?[1,500,1000,1500,2000]:[.125,1,2,3,4]; s.style.left=`${(values[ticks.indexOf(t)]-m.min)/(m.max-m.min)*100}%`;return s;}));
            text($('.echo-feedback-title span'),zh?'反馈衰减':'Feedback decay');
            text($('.echo-feedback-title output'),v.freeze>.5?(zh?'冻结中':'Frozen'):`${v.feedback.toFixed(1)}%`);
            const gain=v.feedback/100*.985;
            const points=Array.from({length:6},(_,i)=>[8+i*36.8,8+60*(1-Math.pow(gain,i))]);
            const path=points.map(([x,y],i)=>`${i?'L':'M'} ${x} ${y}`).join(' ');
            $('.decay-line').setAttribute('d',path);$('.decay-fill').setAttribute('d',path+' L192 70 L8 70 Z');
            $('.decay-handle').setAttribute('cx',points[1][0]);$('.decay-handle').setAttribute('cy',points[1][1]);
            this.feedbackControl.setAttribute('aria-label',zh?'调节反馈衰减':'Adjust feedback decay');
            this.feedbackControl.setAttribute('aria-valuemin','0');this.feedbackControl.setAttribute('aria-valuemax','100');this.feedbackControl.setAttribute('aria-valuenow',String(v.feedback));
            this.feedbackControl.setAttribute('aria-disabled',String(v.freeze>.5));
            this.feedbackControl.hidden=v.freeze>.5||this.app.ui.history===false;
            text($('#echo-feedback-note'),v.freeze>.5?(zh?'保持已有回声 · 新输入已关闭':'Hold existing repeats · input closed'):(zh?'重复次数 · 参数示意':'Repeat number · parameter model'));
            const capped=!m.free&&Math.max(m.rawL,m.rawR)*60000/m.bpm>5000;
        }
    }
}
