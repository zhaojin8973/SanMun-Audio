// 三个插件的设置边界、数字完整性和放大画布回归；兼容弹窗路径不等于 Catalina 宿主验收。
import assert from 'node:assert/strict';
import {createServer} from 'node:http';
import {readFileSync,existsSync,mkdirSync,writeFileSync} from 'node:fs';
import {fileURLToPath} from 'node:url';
import {resolve,extname,dirname} from 'node:path';
const workspace=resolve(dirname(fileURLToPath(import.meta.url)),'../..');
const {chromium,webkit}=await import(process.env.PLAYWRIGHT_MODULE||'/Users/zhaojin/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright/index.mjs');
const outputs=process.env.SANMU_UI_AUDIT_OUTPUT||'/tmp/sanmu-ui-catalina-20261005';
const compiledRoot=process.env.SANMU_UI_COMPILED_ROOT||'';
mkdirSync(outputs,{recursive:true});
const products={eq:['SanMu EQ',3],comp:['SanMu Comp',4],delay:['SanMu Delay',4]};
const types={'.html':'text/html','.css':'text/css','.js':'text/javascript','.json':'application/json'};
const server=createServer((request,response)=>{
    const route=decodeURIComponent(new URL(request.url,'http://localhost').pathname).split('/').filter(Boolean);
    const sourceReference=route[0]==='source';
    if(sourceReference)route.shift();
    const slug=route.shift(),product=products[slug];
    if(!product){response.writeHead(404).end();return;}
    const base=compiledRoot&&!sourceReference?resolve(compiledRoot,slug+'-ui'):resolve(workspace,product[0],'ui');
    const file=resolve(base,...route);
    if(!file.startsWith(base+'/')||!existsSync(file)){response.writeHead(404).end();return;}
    response.setHeader('Content-Type',types[extname(file)]||'application/octet-stream');
    response.end(readFileSync(file));
});
await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
const address=`http://127.0.0.1:${server.address().port}`;
const browserName=process.env.SANMU_UI_BROWSER||'chromium';
const browser=await (browserName==='webkit'?webkit.launch({headless:true}):chromium.launch({executablePath:process.env.CHROME_PATH||'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',headless:true}));
const polyfillSource=readFileSync(resolve(workspace,'packaging/web/node_modules/dialog-polyfill/dist/dialog-polyfill.js'),'utf8');
const polyfillCss=readFileSync(resolve(workspace,'packaging/web/node_modules/dialog-polyfill/dist/dialog-polyfill.css'),'utf8');
const report={passed:false,compiledRoot,browser:browserName,combinations:0,dialogPaths:['原生弹窗','旧浏览器兼容弹窗'],errors:[]};
try{
    for(const legacy of [false,true])for(const [product,[,modeCount]] of Object.entries(products)){
        const page=await browser.newPage({viewport:{width:1100,height:750},deviceScaleFactor:2});
        page.on('pageerror',error=>report.errors.push(`${product}/${legacy}:${error.message}`));
        if(legacy)await page.addInitScript({content:compiledRoot?`delete HTMLDialogElement.prototype.showModal; delete HTMLDialogElement.prototype.show; delete HTMLDialogElement.prototype.close;
            delete Object.hasOwn; delete Array.prototype.at; delete Element.prototype.replaceChildren; delete DocumentFragment.prototype.replaceChildren;
            delete window.structuredClone; delete window.queueMicrotask; delete window.ResizeObserver; delete Crypto.prototype.randomUUID;
            window.__SANMU_FORCE_LEGACY_FLEX_GAP=true;`:`delete HTMLDialogElement.prototype.showModal; delete HTMLDialogElement.prototype.show; delete HTMLDialogElement.prototype.close;\n${polyfillSource}\ndocument.addEventListener('DOMContentLoaded',()=>{const style=document.createElement('style');style.textContent=${JSON.stringify(polyfillCss)};document.head.prepend(style);document.querySelectorAll('dialog').forEach(dialog=>dialogPolyfill.registerDialog(dialog));});`});
        await page.goto(`${address}/${product}/index.html?demo=1`);
        await page.waitForFunction(product=>!!(product==='eq'?window.eqApp:product==='comp'?window.__sanmuComp?.ready:window.sanmuApp?.knobs?.bpm),product);
        if(compiledRoot)assert(await page.evaluate(()=>window.__sanmuCompatibility?.installed),'没有运行发布资源的兼容入口');
        if(legacy&&compiledRoot)assert(await page.evaluate(()=>document.documentElement.classList.contains('sanmu-legacy-flex-gap')),'没有强制启用旧 Flex 间距路径');
        for(const theme of ['light','dark'])for(const language of ['en','zh'])for(let mode=0;mode<modeCount;mode++)for(const scale of [.85,1,1.15,1.25,1.5]){
            await page.setViewportSize({width:Math.round(1100*scale),height:Math.round(750*scale)});
            await page.evaluate(({product,theme,language,mode})=>{
                if(product==='eq'){
                    const app=window.eqApp;app.ui.theme=theme;app.ui.language=language;app.values.character_mode=mode;app.select(3);app.refresh();app.resize();
                }else if(product==='comp'){
                    const app=window.__sanmuComp;app.ui.theme=theme;app.ui.language=language;app.values.mode=mode;app.refresh();window.dispatchEvent(new Event('resize'));
                }else{
                    const app=window.sanmuApp;app.ui.theme=theme;app.ui.language=language;app.values.engine=mode;app.applyTheme(theme);app.applyLanguage(language);app.updateAll();window.dispatchEvent(new Event('resize'));
                }
            },{product,theme,language,mode});
            await page.evaluate(()=>new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(resolve))));
            const checks=await page.evaluate(()=>{
                const visible=element=>{const rect=element.getBoundingClientRect();return rect.width>0&&rect.height>0&&getComputedStyle(element).visibility!=='hidden';};
                const issues=[];
                for(const input of document.querySelectorAll('.knob-readout input'))if(visible(input)){
                    const css=getComputedStyle(input),font=parseFloat(css.fontSize),height=input.clientHeight;
                    if(height<font*1.45)issues.push(`${input.id||'数字'}:${height}/${font}`);
                }
                for(const canvas of document.querySelectorAll('.dial canvas'))if(visible(canvas)){
                    const rect=canvas.getBoundingClientRect(),ratio=Math.min(3,devicePixelRatio||1);
                    if(canvas.width+1<rect.width*ratio||canvas.height+1<rect.height*ratio)issues.push(`画布分辨率:${canvas.width}/${rect.width}`);
                }
                return issues;
            });
            assert.deepEqual(checks,[],`${product}/${legacy}/${theme}/${language}/${mode}/${scale}`);
            await page.locator('#settings').click();
            const layout=await page.locator('#settings-dialog').evaluate(dialog=>{
                const rect=dialog.getBoundingClientRect();return {body:dialog.parentElement===document.body,left:rect.left,top:rect.top,right:rect.right,bottom:rect.bottom,width:innerWidth,height:innerHeight};
            });
            assert(layout.body&&layout.left>=-1&&layout.top>=-1&&layout.right<=layout.width+1&&layout.bottom<=layout.height+1,`${product}/${legacy}/${scale}:${JSON.stringify(layout)}`);
            if(theme==='light'&&language==='zh'&&mode===0&&scale===1.5)await page.screenshot({path:`${outputs}/${product}-${legacy?'legacy':'native'}-150.png`});
            await page.locator('#settings-dialog .close').click();
            const dial=page.locator('.knob .dial').first();await dial.focus();
            assert.equal(await dial.evaluate(element=>getComputedStyle(element).outlineStyle),'none',`${product}旋钮仍显示外框`);
            report.combinations++;
        }
        await page.close();
    }
    assert.deepEqual(report.errors,[]);
    report.passed=true;
}finally{
    writeFileSync(`${outputs}/report.json`,JSON.stringify(report,null,2));
    await browser.close();
    await new Promise(resolve=>server.close(resolve));
}
console.log(JSON.stringify(report,null,2));
