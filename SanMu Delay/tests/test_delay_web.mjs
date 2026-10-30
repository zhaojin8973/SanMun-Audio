// 可见性、真实点击和宿主读数回显回归；依赖路径可由环境指定。
const { chromium } = await import(process.env.PLAYWRIGHT_MODULE || 'playwright');
import assert from 'node:assert/strict';
const base = process.env.DELAY_UI_URL || 'http://127.0.0.1:50586/index.html';
const browser = await chromium.launch({ executablePath: process.env.CHROME_PATH || '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome', headless: true });
const p = await browser.newPage({ viewport: { width: 1100, height: 750 } });
const errors = [];
p.on('pageerror', e => errors.push(e.message));
try {
    await p.goto(base);
    await p.waitForFunction(() => window.sanmuApp?.knobs?.bpm && window.sanmuApp?.duckMeter);
    assert(await p.locator('input[name="sync_mode"][value="1"]').isChecked(), '默认时间模式不是 BPM');
    const drain = () => p.evaluate(() => sanmuApp.commandQueue);
    const read = () => p.evaluate(() => ({ ...sanmuApp.values }));
    await p.locator('#knob-modern-mod-amount').press('End'); await drain();
    assert.equal((await read()).mod_depth, 100, 'Modern 调制量没有写入真实 mod_depth');
    await p.locator('#knob-modern-mod-amount').press('Home'); await drain();
    // 点击实际按钮表面；禁止 force 掩盖移出屏幕或被遮挡的问题。
    const choose = async (name, value) => {
        const input = p.locator(`input[name="${name}"][value="${value}"]`);
        await input.locator('..').locator('span').click();
        await drain();
        await p.waitForFunction(({name, value}) => Number(sanmuApp.values[name]) === Number(value), {name, value});
        assert(await input.isChecked(), `${name}/${value} 未选中`);
    };
    for (const value of [1, 2, 0]) await choose('topology', value);
    for (const value of [1, 2, 0]) await choose('sync_mode', value);
    assert(await p.locator('#edit-manual_bpm').isDisabled());
    await choose('sync_mode', 1);
    await p.locator('#edit-manual_bpm').click();
    await p.locator('#edit-manual_bpm').fill('135 BPM');
    await p.keyboard.press('Enter'); await drain();
    assert.equal((await read()).manual_bpm, 135);
    await choose('sync_mode', 0);
    const host = await p.evaluate(() => {
        sanmuApp.onFrame({ meter: { bpm: 156.5, duckGr: 0 } });
        return { shown: document.querySelector('#edit-manual_bpm').value, value: sanmuApp.values.manual_bpm };
    });
    assert.equal(host.shown, '156.5'); assert.equal(host.value, 135);
    const lockedDial = await p.locator('[data-parameter="manual_bpm"] .dial').boundingBox();
    await p.mouse.dblclick(lockedDial.x + lockedDial.width / 2, lockedDial.y + lockedDial.height / 2); await drain();
    assert.equal((await read()).manual_bpm, 135, '宿主模式双击不得修改手动速度');
    await choose('sync_mode', 2);
    assert(await p.locator('#edit-manual_bpm').isDisabled());
    await p.locator('#btn-time-link').click(); await drain();
    const before = await read();
    const rect = await p.locator('#echo-time-l').boundingBox();
    await p.mouse.move(rect.x + rect.width / 2, rect.y + rect.height / 2);
    await p.mouse.down(); await p.mouse.move(rect.x + rect.width / 2 + 80, rect.y + rect.height / 2, { steps: 10 }); await p.mouse.up(); await drain();
    assert((await read()).time_l > before.time_l + 100);
    assert.equal((await read()).time_r, before.time_r);
    await p.locator('#echo-time-r').focus(); await p.keyboard.press('End'); await drain();
    assert.equal((await read()).time_r, 2000);
    await p.locator('#undo').click(); await p.waitForFunction(v => sanmuApp.values.time_r === v, before.time_r);
    await p.locator('#echo-time-l').dblclick(); await p.locator('#edit-time-l').fill('0.25 s');
    await p.keyboard.press('Enter'); await drain(); assert.equal((await read()).time_l, 250);
    await p.locator('#btn-time-link').click(); await drain();
    assert(await p.locator('#echo-time-r').isDisabled());
    await p.locator('#echo-time-l').focus(); await p.keyboard.press('Home'); await drain();
    assert.equal((await read()).time_l, 1); assert.equal((await read()).time_r, 1);
    await choose('sync_mode', 1);
    await p.locator('#echo-time-l').focus(); await p.keyboard.press('Enter');
    await p.locator('#edit-time-l').fill('1/8D'); await p.keyboard.press('Enter'); await drain();
    assert.equal((await read()).division, 7); assert.equal((await read()).division_r, 7);
    await choose('topology', 1); assert(await p.locator('#btn-time-link').isDisabled());
    assert(await p.locator('#echo-time-r').isDisabled());
    await p.locator('#echo-feedback-control').focus(); await p.keyboard.press('Home'); await drain();
    assert.equal((await read()).feedback, 0);
    await p.locator('#btn-freeze').click(); await drain(); assert(await p.locator('#echo-feedback-control').isHidden());
    await p.locator('#btn-freeze').click(); await drain();
    // 注入已知读数只验证显示映射；真实音频到 GR 的链路由原生测试覆盖。
    const meterStates = await p.evaluate(() => [0, 6, 12, 24, 36, NaN, -1].map(duckGr => {
        sanmuApp.onFrame({ meter: { duckGr } });
        return [document.querySelector('#duck-meter-fill').style.width, document.querySelector('#duck-meter-value').textContent];
    }));
    assert.deepEqual(meterStates, [['0%','0.0 dB'],['25%','6.0 dB'],['50%','12.0 dB'],['100%','24.0 dB'],['100%','36.0 dB'],['0%','0.0 dB'],['0%','0.0 dB']]);
    await p.locator('#preset-open').click();
    await p.getByRole('button', { name: 'Analog BBD Slapback FACTORY' }).click();
    await p.waitForFunction(() => sanmuApp.values.engine === 2);
    assert.equal((await read()).time_l, 110);
    await p.evaluate(() => { sanmuApp.onFrame = () => {}; });
    let combos = 0;
    for (const width of [1100, 506]) {
        await p.setViewportSize({ width, height: Math.round(width * 750 / 1100) });
        await p.waitForFunction(() => Math.abs(sanmuApp.scale - Math.min(innerWidth/1100,innerHeight/750)) < 0.0001);
        for (const theme of ['light','dark']) for (const lang of ['zh','en']) for (const engine of [0,1,2,3]) for (const topology of [0,1,2]) for (const sync of [0,1,2]) {
            await p.evaluate(state => {
                Object.assign(sanmuApp.ui, { theme: state.theme, language: state.lang });
                Object.assign(sanmuApp.values, { engine: state.engine, topology: state.topology, sync_mode: state.sync, time_link: 0 });
                sanmuApp.updateAll();
            }, { theme, lang, engine, topology, sync });
            const issues = await p.evaluate(() => {
                const issues = [], instrument = document.querySelector('.instrument').getBoundingClientRect();
                const inside = (a,b) => a.left >= b.left - 1 && a.top >= b.top - 1 && a.right <= b.right + 1 && a.bottom <= b.bottom + 1;
                const viewport = { left: 0, top: 0, right: innerWidth, bottom: innerHeight };
                const selectors = ['.families','.mode-routing','.echo-precision','.duck-meter','#duck-meter-value','.duck-meter-track','[data-parameter="manual_bpm"]','#knob-feedback','.details'];
                for (const selector of selectors) {
                    const e = document.querySelector(selector), r = e?.getBoundingClientRect();
                    if (!e || !e.checkVisibility() || !r.width || !r.height || !inside(r,instrument) || !inside(r,viewport)) issues.push(selector);
                }
                const route = document.querySelector('.mode-routing').getBoundingClientRect();
                const families = document.querySelector('.families').getBoundingClientRect();
                if (route.left <= families.right || Math.abs(route.top + route.height/2 - families.top - families.height/2) > 2) issues.push('路由未在引擎右侧同排');
                if (!inside(document.querySelector('.duck-meter').getBoundingClientRect(),document.querySelector('.duck-group').getBoundingClientRect())) issues.push('GR 不在 Ducking 内');
                if (document.querySelector('#echo-live,#echo-routing,#echo-hint,.meters,.precision-note,#tempo-box-container,#time-box-l-container')) issues.push('遗留重复说明或控件');
                if (document.querySelectorAll('input[name="sync_mode"]').length !== 3) issues.push('缺少时钟切换');
                const chars = [...document.querySelectorAll('.engine-char-wrapper .knob')].filter(e=>e.checkVisibility());
                if (chars.length !== [1,2,1,1][sanmuApp.values.engine]) issues.push('音色旋钮数量不符');
                return issues;
            });
            assert.deepEqual(issues, [], `${width}/${theme}/${lang}/${engine}/${topology}/${sync}`);
            combos++;
        }
    }
    await p.setViewportSize({ width: 1100, height: 750 });
    for (const theme of ['light','dark']) for (const engine of [0,1,2,3]) {
        await p.evaluate(({theme,engine}) => {
            Object.assign(sanmuApp.ui,{theme,language:'zh'});
            Object.assign(sanmuApp.values,{engine,topology:0,sync_mode:1,feedback:35}); sanmuApp.updateAll(); sanmuApp.duckMeter.update(0);
        }, {theme,engine});
        await p.screenshot({ path: `/tmp/delay-visible-${theme}-${engine}.png`, animations: 'disabled' });
    }
    await p.evaluate(() => { sanmuApp.applyLanguage('zh'); sanmuApp.applyTheme('light'); });
    await p.locator('#settings').click(); await p.locator('#help').click();
    await p.getByText('阻尼、调制与避让', {exact: true}).click();
    assert((await p.locator('#manual-content').innerText()).includes('Ducking 标题内'));
    await p.locator('#help-dialog .close').click();
    for (const scale of [.85,1,1.15,1.25,1.5]) {
        await p.setViewportSize({width:Math.round(1100*scale),height:Math.round(750*scale)});
        await p.waitForFunction(() => Math.abs(sanmuApp.scale - Math.min(innerWidth/1100,innerHeight/750)) < 0.0001);
        await p.locator('#settings').click();
        const box=await p.locator('#settings-dialog').boundingBox();
        assert(box.x>=0&&box.y>=0&&box.x+box.width<=1100*scale+1&&box.y+box.height<=750*scale+1);
        await p.locator('#settings-dialog .close').click();
    }
    // 外层预览也要包含新内页，不能只检查直接打开的 index。
    await p.setViewportSize({width:506,height:854});
    await p.goto(base.replace(/index\.html.*/, 'preview.html?review=20260913-visible-fix'));
    const frame = p.frameLocator('iframe');
    await frame.locator('input[name="topology"][value="2"]').locator('..').locator('span').click();
    await frame.locator('input[name="sync_mode"][value="1"]').locator('..').locator('span').click();
    await frame.locator('input[name="engine"][value="1"]').locator('..').locator('span').click();
    assert(await frame.locator('input[name="topology"][value="2"]').isChecked());
    assert(await frame.locator('.duck-meter').isVisible());
    await p.screenshot({path:'/tmp/delay-visible-preview-506.png'});
    assert.deepEqual(errors, []);
    console.log(JSON.stringify({passed:true,layoutCombinations:combos,scales:5,errors}));
} finally { await browser.close(); }
