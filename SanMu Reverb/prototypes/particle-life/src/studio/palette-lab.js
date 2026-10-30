import './palette-lab.css';
import { Knob } from './knob.js';
import { parameterList } from '../parameters.js';

// 独立配色实验页：不写入主界面的主题、参数或音频状态。
const palettes = [
  {
    id:'A', name:'极光', description:'青蓝、橙金、电紫、玫红。通透、鲜明，偏冷光。',
    light:{ tones:['#00aeba','#ff9d37','#7770ed','#df53b2'], ends:['#3484ec','#ef624f','#467ade','#a95ae7'], inks:['#085b6c','#794017','#43417d','#85316c'] },
    dark:{ tones:['#24dfdd','#ffb540','#a494ff','#ff71c8'], ends:['#447bf5','#f16b58','#6584ff','#b265fa'], inks:['#a2fbf4','#ffe4ac','#e7d9ff','#ffc8ea'] },
  },
  {
    id:'B', name:'绯霞', description:'珊瑚、蜜金、碧水、兰紫。更有温度，也更有个性。',
    light:{ tones:['#f07083','#e7aa26','#00b3a8','#ac6cde'], ends:['#d355b0','#ed7840','#459de1','#7b74e7'], inks:['#843450','#775019','#0d655f','#6c398f'] },
    dark:{ tones:['#ff8c9d','#ffcf61','#35e0c4','#cb99ff'], ends:['#e95fc0','#f59a4a','#4cacf2','#9391ff'], inks:['#ffd0dc','#fff0b2','#adfced','#ebd4ff'] },
  },
  {
    id:'C', name:'棱彩', description:'祖母绿、琥珀、电蓝、莓紫。对比最强，色彩最饱满。',
    light:{ tones:['#14b993','#f19718','#398eef','#c753c7'], ends:['#129ccc','#ef6343','#7958e9','#eb637e'], inks:['#096052','#82420f','#244e89','#763380'] },
    dark:{ tones:['#35e9ab','#ffb52d','#65bbff','#eb83ff'], ends:['#21bcd9','#f47642','#8d75ff','#ff759d'], inks:['#b5ffe1','#ffe1a0','#cceaff','#f5ccff'] },
  },
];
const types=['Hall','Plate','Room','Chamber'];
const values={decay:3.99,early:100};
const cards=[], knobs=[];
const rgb = hex => [1,3,5].map(i=>parseInt(hex.slice(i,i+2),16)).join(' ');

function card(palette, mode) {
  const colours=palette[mode];
  return `<article class="palette-card ${mode}" data-palette="${palette.id}" data-theme="${mode}">
    <header><span class="badge">${palette.id}</span><div><h2>${palette.name}</h2><p>${palette.description}</p></div><span class="theme-name">${mode==='light'?'浅色':'深色'}</span></header>
    <div class="preview">
      <div class="type-row" role="group" aria-label="${palette.name} ${mode==='light'?'浅色':'深色'}混响类型">
        ${types.map((type,i)=>`<button type="button" data-type="${i}" aria-pressed="${i===0}" style="--tone:${rgb(colours.tones[i])};--end:${rgb(colours.ends[i])};--ink:${colours.inks[i]}">${type}</button>`).join('')}
      </div>
      <div class="control-row"><div class="knob large" data-knob="decay"></div><div class="knob small" data-knob="early"></div><div class="control-sample"><span class="sample-label">混合显示</span><button class="overlay" type="button" aria-pressed="true">叠加</button><div class="swatch-line">${colours.tones.map((c,i)=>`<i style="--swatch:${c};--swatch-end:${colours.ends[i]}"></i>`).join('')}</div></div></div>
      <div class="sample-footer"><span>原有旋钮 · 彩色玻璃</span><span class="sample-status">已选 Hall</span></div>
    </div>
  </article>`;
}

document.querySelector('#palette-lab').innerHTML=`<div class="shell"><div class="intro"><div><div class="eyebrow">SanMu Audio · Reverb</div><h1>让颜色透出来 <span>第二轮</span></h1><p>胶囊透出双色渐变，刻度与指针随类型变色。保留现有旋钮与中性玻璃底板。</p></div><div class="instructions">点击类型，六张卡片同步对照<br>拖动旋钮，查看不同位置的色彩</div></div><div class="grid">${palettes.flatMap(p=>['light','dark'].map(mode=>card(p,mode))).join('')}</div><footer>仅为独立配色预览，主界面 U12 与水、粒子保持现状。</footer></div>`;

document.querySelectorAll('.palette-card').forEach(element=>cards.push({element,palette:palettes.find(p=>p.id===element.dataset.palette),mode:element.dataset.theme}));
function selectType(index) {
  cards.forEach(({element,palette,mode})=>{
    const colours=palette[mode], light=mode==='light', tone=colours.tones[index], end=colours.ends[index], ink=colours.inks[index];
    const variables={
      '--accent':light?ink:tone,'--pointer':ink,'--tick':light?tone+'e0':tone,
      '--track':end+(light?'50':'65'),'--knob-top':tone+(light?'20':'42'),
      '--knob-bottom':end+(light?'0e':'1c'),'--knob-edge':tone+(light?'a0':'ba'),
      '--knob-shine':light?'#ffffff':ink+'bd','--knob-shadow':light?'#51626c18':'#090d182e',
      '--active-rgb':rgb(tone),'--active-end':rgb(end),'--active-ink':ink,
    };
    Object.entries(variables).forEach(([key,value])=>element.style.setProperty(key,value));
    element.querySelectorAll('[data-type]').forEach(button=>button.setAttribute('aria-pressed',String(Number(button.dataset.type)===index)));
    element.querySelector('.sample-status').textContent=`已选 ${types[index]}`;
  });
  knobs.forEach(knob=>knob.draw());
}
selectType(0);
cards.forEach(({element,palette,mode})=>{
  element.querySelectorAll('[data-knob]').forEach(container=>{
    const id=container.dataset.knob;
    const definition=[...parameterList.find(item=>item[0]===id)];
    definition[0]=`${palette.id}-${mode}-${id}`;
    const knob=new Knob(container,definition,()=>values[id],(_,value)=>{
      values[id]=value;
      knobs.forEach(item=>item.update());
    },()=>{},()=>{});
    knob.setLanguage('en');
    knobs.push(knob);
  });
  element.querySelectorAll('[data-type]').forEach(button=>button.addEventListener('click',()=>selectType(Number(button.dataset.type))));
  element.querySelector('.overlay').addEventListener('click',event=>{
    const pressed=event.currentTarget.getAttribute('aria-pressed')!=='true';
    document.querySelectorAll('.overlay').forEach(button=>button.setAttribute('aria-pressed',String(pressed)));
  });
});
