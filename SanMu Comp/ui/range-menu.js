// GR 档位在网页内绘制，避免宿主原生菜单覆盖插件主题。
export class RangeMenu {
 constructor(app,key='grRange',id='gr-range',label=value=>`GR ${value} dB`){
  this.app=app;this.key=key;this.label=label;this.root=document.querySelector('#'+id).parentElement;
  this.button=this.root.querySelector('#'+id);this.menu=this.root.querySelector('#'+id+'-menu');
  this.items=[...this.menu.querySelectorAll('button')];
  this.button.onclick=()=>this.menu.hidden?this.open():this.close();
  this.button.onkeydown=e=>{if(['ArrowDown','ArrowUp'].includes(e.key)){e.preventDefault();this.open();}};
  this.items.forEach(item=>item.onclick=()=>{this.app.preference(this.key,Number(item.value));this.close(true);});
  this.menu.onkeydown=e=>{
   const index=this.items.indexOf(document.activeElement);let next;
   if(e.key==='ArrowDown')next=(index+1)%this.items.length;
   if(e.key==='ArrowUp')next=(index+this.items.length-1)%this.items.length;
   if(e.key==='Home')next=0;if(e.key==='End')next=this.items.length-1;
   if(next!==undefined){e.preventDefault();this.items[next].focus();}
   if(e.key==='Escape'){e.preventDefault();e.stopPropagation();this.close(true);}
   // 回到触发器后让浏览器执行正常的 Tab 顺序。
   if(e.key==='Tab')this.close(true);
  };
  document.addEventListener('pointerdown',e=>{if(!this.root.contains(e.target))this.close();});
  this.root.addEventListener('focusout',e=>{if(!this.root.contains(e.relatedTarget))this.close();});
  window.addEventListener('blur',()=>this.close());
 }
 update(value){
  const label=this.label(value),span=this.button.querySelector('span');
  if(span.textContent!==label)span.textContent=label;
  this.items.forEach(item=>item.setAttribute('aria-checked',Number(item.value)===value));
 }
 open(){this.app.hideTip?.();this.menu.hidden=false;this.button.setAttribute('aria-expanded','true');(this.items.find(item=>Number(item.value)===Number(this.app.ui[this.key]))||this.items[1]).focus();}
 close(focus=false){this.menu.hidden=true;this.button.setAttribute('aria-expanded','false');if(focus)this.button.focus();}
}
