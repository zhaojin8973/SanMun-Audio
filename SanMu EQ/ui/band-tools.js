import {setBandColour} from './shared/band-colour.js';
import {icon} from './icons.js';
import {clamp} from './controls.js';
import {bindCardInput} from './card-input.js';
const glyphs={type:['filterBell','filterLowShelf','filterHighShelf','filterLowCut','filterHighCut'],placement:['stereo','left','right','mid','side'],slope:['slope','slope','slope','slope']};
const labels={type:'滤波类型',placement:'声道位置',slope:'斜率'};
const bandFields=['enabled','type','freq','gain','q','slope','placement','dynamic','dyn_range','dyn_threshold','dyn_auto','dyn_upward'];

// 按住监听属于临时状态，不写入预设、撤销记录或宿主自动化。
export function bindAudition(button,app,band=()=>app.selected){
  let pointer=null,startX=0,startFrequency=0,n=-1,moved=false;
  const finish=()=>{if(n<0)return;const old=pointer;pointer=null;n=-1;app.stopAudition();if(moved){moved=false;app.end();}if(old!=null&&button.hasPointerCapture(old))button.releasePointerCapture(old);};
  button.addEventListener('pointerdown',e=>{if(e.button||button.disabled)return;e.preventDefault();n=band();if(n<1)return;button.focus();pointer=e.pointerId;startX=e.clientX;startFrequency=app.values[`band${n}_freq`];app.startAudition(n,finish);button.setPointerCapture(pointer);});
  button.addEventListener('pointermove',e=>{if(pointer!==e.pointerId||n<1)return;if(!moved&&Math.abs(e.clientX-startX)<2)return;if(!moved){moved=true;app.begin();}app.set(`band${n}_freq`,clamp(startFrequency*Math.pow(1000,(e.clientX-startX)/(app.graph.area.w*app.scale)),20,20000));});
  for(const event of ['pointerup','pointercancel','lostpointercapture'])button.addEventListener(event,finish);
  button.addEventListener('keydown',e=>{if(![' ','Enter'].includes(e.key)||e.repeat||n>0)return;e.preventDefault();n=band();app.startAudition(n,finish);});
  button.addEventListener('keyup',e=>{if([' ','Enter'].includes(e.key)){e.preventDefault();finish();}});button.addEventListener('blur',finish);
  return finish;
}

export function createBandTools(app){
  const toolbar=document.createElement('div');toolbar.id='node-tools';toolbar.setAttribute('role','toolbar');toolbar.setAttribute('aria-label','频点操作');toolbar.hidden=true;
  toolbar.innerHTML='<div class="node-top"><input class="node-frequency" spellcheck="false"></div><div class="node-readings"><input class="node-gain" spellcheck="false"><input class="node-q" spellcheck="false"></div><div class="node-selectors"></div>';
  document.querySelector('.stage').append(toolbar);
  const top=toolbar.querySelector('.node-top'),selectors=toolbar.querySelector('.node-selectors');
  const actionButton=(glyph,label,action)=>{const b=document.createElement('button');b.className='icon-button';b.innerHTML=icon(glyph);b.dataset.action=glyph;b.dataset.tip=label;b.setAttribute('aria-label',label);if(action)b.onclick=()=>{if(app.selected>0)action(app.selected);};return b;};
  const power=actionButton('power','频段开关',n=>app.toggle(`band${n}_enabled`));top.prepend(power);
  const headphones=actionButton('headphones','按住独听，左右拖动扫频');top.append(headphones);bindAudition(headphones,app);
  top.append(actionButton('x','删除频段',n=>app.command('deleteBand',{band:n})));
  const popup=document.createElement('div');popup.id='band-choice-menu';popup.className='band-choice-menu';popup.hidden=true;popup.setAttribute('role','listbox');document.querySelector('#app').append(popup);
  const menu=document.createElement('div');menu.id='band-action-menu';menu.className='band-choice-menu';menu.hidden=true;menu.setAttribute('role','menu');document.querySelector('#app').append(menu);
  let active=null,menuBand=-1,near=false,hideTimer=0,clipboard=null;
  const inputs=['freq','gain','q'].map((field,i)=>bindCardInput(toolbar.querySelector(['.node-frequency','.node-gain','.node-q'][i]),field,app));
  const more=actionButton('chevron-down','更多频段操作',()=>expand());more.id='node-more';more.setAttribute('aria-haspopup','menu');more.setAttribute('aria-expanded','false');
  const keep=()=>{clearTimeout(hideTimer);near=true;app.tools?.refresh();};
  const leave=()=>{clearTimeout(hideTimer);hideTimer=setTimeout(()=>{near=false;app.tools?.refresh();},220);};
  const closeChoice=()=>{popup.hidden=true;if(active)active.button.setAttribute('aria-expanded','false');active=null;};
  const close=(focus=false)=>{closeChoice();menu.hidden=true;more.setAttribute('aria-expanded','false');if(focus&&!toolbar.hidden)more.focus();};
  const positionPopup=(element,button,side=false)=>{
    const r=button.getBoundingClientRect(),left=r.left/app.scale,top=r.top/app.scale,bottom=r.bottom/app.scale;
    element.style.left=`${clamp(side?r.right/app.scale+5:left,12,1088-element.offsetWidth)}px`;
    element.style.top=`${clamp(side?top:bottom+5,12,732-element.offsetHeight)}px`;
  };
  document.addEventListener('pointermove',e=>{
    if(e.target.closest('#node-tools,#band-choice-menu,#band-action-menu')||app.graph.drag||app.auditionBand>0||inputs.some(i=>i.isEditing())){keep();return;}
    if(app.graph.focus?.amount>.2){leave();return;}
    const node=e.target.closest('.eq-node');if(node){keep();const n=Number(node.dataset.band);if(n!==app.selected)app.select(n);return;}
    const selected=app.graph.buttons.get(app.selected),r=selected?.getBoundingClientRect();
    if(r&&Math.hypot(e.clientX-r.left-r.width/2,e.clientY-r.top-r.height/2)<32*app.scale)keep();else leave();
  });
  toolbar.addEventListener('focusin',keep);toolbar.addEventListener('focusout',()=>{if(!active&&menu.hidden)leave();});
  window.addEventListener('blur',()=>{near=false;close();app.tools?.refresh();});
  const pickers=[];
  function refreshMenu(){if(active)[...popup.children].forEach((b,i)=>b.setAttribute('aria-selected',i===app.values[`band${app.selected}_${active.field}`]));}
  function open(picker){
    if(active?.button===picker.button){closeChoice();return;}closeChoice();if(picker.place!=='menu'){menu.hidden=true;more.setAttribute('aria-expanded','false');}
    active=picker;menuBand=app.selected;popup.replaceChildren();popup.setAttribute('aria-label',app.t(labels[picker.field]));
    app.schema[`band1_${picker.field}`].choices.forEach((name,i)=>{const option=document.createElement('button');option.setAttribute('role','option');option.dataset.value=i;option.innerHTML=icon(glyphs[picker.field][i])+`<span>${name}</span><i class="selection-dot"></i>`;option.onclick=()=>{if(menuBand===app.selected){app.begin();app.set(`band${menuBand}_${picker.field}`,i);app.end();}const origin=picker.button;close();if(origin.isConnected&&!origin.closest('[hidden]'))origin.focus();keep();};popup.append(option);});
    popup.hidden=false;picker.button.setAttribute('aria-expanded','true');refreshMenu();positionPopup(popup,picker.button,picker.place==='menu');popup.querySelector('[aria-selected=true]')?.focus();
  }
  for(const field of ['type','placement','slope'])for(const place of ['inspector','node']){
    if(place==='inspector'&&field==='slope')continue;
    const button=document.createElement('button');button.id=`${place}-${field}-open`;button.className='choice-button';button.setAttribute('aria-haspopup','listbox');button.setAttribute('aria-expanded','false');
    (place==='node'?selectors:document.querySelector(`#inspector-${field}`)).append(button);
    const picker={field,place,button};pickers.push(picker);button.onclick=()=>open(picker);
  }
  selectors.append(more);
  const capture=()=>Object.fromEntries(bandFields.map(key=>[key,app.values[`band${app.selected}_${key}`]]));
  function expand(){
    if(!menu.hidden){close();return;}close();menuBand=app.selected;if(menuBand<1)return;
    const p=`band${menuBand}_`,cut=app.values[p+'type']>=3;menu.replaceChildren();
    const add=(id,label,glyph,fn,disabled=false,pressed=null)=>{
      const b=document.createElement('button');b.dataset.menuAction=id;b.setAttribute('role','menuitem');b.innerHTML=icon(glyph)+`<span>${app.t(label)}</span><i class="selection-dot"></i>`;b.disabled=disabled;if(pressed!=null)b.setAttribute('aria-pressed',pressed);
      b.onclick=()=>{if(app.selected!==menuBand){close();return;}if(fn)fn(b);};menu.append(b);return b;
    };
    const run=fn=>()=>{fn();close();keep();};
    add('enabled',app.values[p+'enabled']?'旁通此频段':'启用此频段','power',run(()=>app.toggle(p+'enabled')),false,!!app.values[p+'enabled']);
    add('invert','增益反向','polarity',run(()=>{app.begin();app.set(p+'gain',-app.values[p+'gain']);app.end();}),cut);
    add('dynamic',app.values[p+'dynamic']&&!app.linearLegacy(menuBand)?'关闭动态':'开启动态','power',run(()=>app.toggle(p+'dynamic')),cut,!!app.values[p+'dynamic']&&!app.linearLegacy(menuBand));
    for(const field of ['type','slope','placement']){const b=add(field,labels[field],glyphs[field][app.values[p+field]],button=>open({field,button,place:'menu'}),field==='slope'&&!cut);b.setAttribute('aria-haspopup','listbox');b.setAttribute('aria-expanded','false');}
    const separator=document.createElement('hr');menu.append(separator);
    add('copy','复制频段','copy',run(()=>{clipboard=capture();}));
    add('paste','粘贴到此频段','copy',run(()=>{app.begin();for(const [key,value]of Object.entries(clipboard))app.set(p+key,value);app.end();}),!clipboard);
    add('duplicate','复制为新频段','plus',run(()=>{const settings=capture(),n=app.beginBand(settings.freq,settings);if(n>0){app.end();app.tools?.show(n);}}),app.bands().length>=24);
    add('cut','剪切频段','trash',run(()=>{clipboard=capture();app.command('deleteBand',{band:app.selected});}));
    add('delete','删除频段','x',run(()=>app.command('deleteBand',{band:app.selected})));
    menu.hidden=false;more.setAttribute('aria-expanded','true');positionPopup(menu,more);menu.querySelector('button')?.focus();
  }
  const navigation=(element,e)=>{
    const items=[...element.querySelectorAll('button:not(:disabled)')],i=items.indexOf(document.activeElement);
    if(e.key==='Escape'){e.preventDefault();e.stopPropagation();close(true);}
    if(['ArrowDown','ArrowUp','Home','End'].includes(e.key)){e.preventDefault();items[e.key==='Home'?0:e.key==='End'?items.length-1:(i+(e.key==='ArrowDown'?1:items.length-1))%items.length]?.focus();}
  };
  popup.onkeydown=e=>navigation(popup,e);menu.onkeydown=e=>navigation(menu,e);
  document.addEventListener('pointerdown',e=>{if(!e.target.closest('.choice-button,#node-more,#band-choice-menu,#band-action-menu'))close();});window.addEventListener('resize',()=>close());
  return {close,expand,show(n){keep();if(n!==app.selected)app.select(n);else this.refresh();},hide(){near=false;close();this.refresh();},refresh(){
    const n=app.selected,p=`band${n}_`,valid=app.bands().includes(n),type=app.values[p+'type']||0;
    toolbar.hidden=!valid||app.graph.focus?.amount>.2||!(near||inputs.some(i=>i.isEditing())||app.graph.drag?.n===n||active?.place==='node'||!menu.hidden||app.auditionBand===n);
    if(!valid){close();return;}if((active||!menu.hidden)&&menuBand!==n)close();refreshMenu();
    for(const {field,place,button}of pickers){
      const value=app.values[p+field],name=app.schema[`band1_${field}`].choices[value],disabled=field==='slope'&&type<3,compact=place==='node';
      button.disabled=disabled;button.setAttribute('aria-label',`${app.t(labels[field])}: ${disabled?'—':name}`);button.dataset.tip=disabled?app.t('斜率适用于高切和低切'):`${app.t(labels[field])}: ${name}`;
      const display=compact?(field==='type'?'':field==='slope'?`${(value+1)*12} dB/oct`:['ST','L','R','M','S'][value]):name;
      const html=`${compact?'':`<span class="choice-caption">${app.t(labels[field])}</span>`}<span class="choice-value">${compact&&field==='slope'?'':icon(glyphs[field][value])}${display?`<b>${disabled?'—':display}</b>`:''}${compact?'':icon('chevron-down')}</span>`;
      if(button.innerHTML!==html)button.innerHTML=html;
    }
    for(const b of toolbar.querySelectorAll('[data-action]'))b.setAttribute('aria-label',app.t(b.dataset.tip));
    power.setAttribute('aria-pressed',!!app.values[p+'enabled']);headphones.setAttribute('aria-pressed',app.auditionBand===n||!!app.values[p+'solo']);
    const gain=app.values[p+'gain'];inputs.forEach(i=>i.update());
    more.classList.toggle('dynamic-active',!!app.values[p+'dynamic']&&!app.linearLegacy(n));
    const node=app.graph.buttons.get(n);if(!node)return;for(const element of [toolbar,popup,menu])setBandColour(element,node.style.getPropertyValue('--band'));
    // 指针捕获保证耳机扫频不中断，卡片和频点一起跟随频率移动。
    const x=parseFloat(node.style.left),y=parseFloat(node.style.top),height=toolbar.offsetHeight,left=clamp(x-toolbar.offsetWidth/2,app.graph.area.x,app.graph.area.x+app.graph.area.w-toolbar.offsetWidth);
    toolbar.style.left=`${left}px`;toolbar.style.setProperty('--tail-x',`${clamp(x-left,10,toolbar.offsetWidth-10)}px`);
    if(!app.graph.drag||!toolbar.dataset.side)toolbar.dataset.side=gain<0?'below':'above';
    let below=toolbar.dataset.side==='below';if(!below&&y-height-16<app.graph.area.y)below=true;else if(below&&y+height+16>app.graph.area.y+app.graph.area.h)below=false;
    toolbar.dataset.side=below?'below':'above';toolbar.style.top=`${below?y+17:y-height-17}px`;
  }};
}
