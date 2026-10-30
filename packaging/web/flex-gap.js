// 旧 WebKit 能解析 Grid 的 gap，却不会把 gap 应用于 Flex。这个回退只在
// 探针确认缺失时安装；现代引擎完全不改写布局，也不会给音频帧增加工作。
export function installFlexGapFallback() {
  const probe = document.createElement('div');
  probe.style.cssText = 'position:absolute;visibility:hidden;display:flex;flex-direction:column;row-gap:1px;';
  for (let i = 0; i < 2; i++) {
    const child = document.createElement('div');
    child.style.height = '1px';
    probe.appendChild(child);
  }
  document.body.appendChild(probe);
  const required = probe.scrollHeight !== 3 || window.__SANMU_FORCE_LEGACY_FLEX_GAP === true;
  probe.parentNode.removeChild(probe);
  if (!required) return;

  document.documentElement.classList.add('sanmu-legacy-flex-gap');
  const states = new WeakMap();
  const parents = new Set();
  let queued = false;
  let ignoreManagedStyleMutations = false;
  let clearManagedStyleTimer = 0;

  const restore = state => {
    let changed = false;
    for (const [child, properties] of state.children) {
      for (const [property, value] of Object.entries(properties)) {
        if (child.style[property] !== value) { child.style[property] = value; changed = true; }
      }
    }
    state.children.clear();
    state.baselines.clear();
    if (changed) {
      ignoreManagedStyleMutations = true;
      clearTimeout(clearManagedStyleTimer);
      clearManagedStyleTimer = setTimeout(() => { ignoreManagedStyleMutations = false; }, 0);
    }
  };

  const visibleFlexChildren = parent => [...parent.children].filter(child => {
    const childStyle = getComputedStyle(child);
    return childStyle.display !== 'none' && childStyle.position !== 'absolute' && childStyle.position !== 'fixed';
  });

  const groupLines = (children, horizontal) => {
    const lines = [];
    for (const child of children) {
      const rect = child.getBoundingClientRect();
      const coordinate = horizontal ? rect.top : rect.left;
      let line = lines.find(item => Math.abs(item.coordinate - coordinate) < 0.5);
      if (!line) { line = { coordinate, children: [] }; lines.push(line); }
      line.children.push({ child, rect });
    }
    lines.sort((a, b) => a.coordinate - b.coordinate);
    return lines;
  };

  const selectorRules = function* (rules) {
    for (const rule of rules) {
      if (rule.type === 1 && rule.selectorText) yield rule;
      else if (rule.cssRules) yield* selectorRules(rule.cssRules);
    }
  };
  const hasAutoMarginRule = (child, property) => {
    if (child.style[property] === 'auto') return true;
    const computed = getComputedStyle(child)[property];
    if (computed === '0px' || computed === 'auto') return false;
    for (const sheet of document.styleSheets) {
      try {
        for (const rule of selectorRules(sheet.cssRules)) {
          if (!child.matches(rule.selectorText)) continue;
          if (rule.style[property] === 'auto') return true;
          // 旧 WebKit 的 CSSOM 可能只暴露 shorthand。
          if (rule.style.margin === 'auto' || rule.style.marginInlineStart === 'auto' ||
            rule.style.marginInlineEnd === 'auto') return true;
        }
      } catch (_) {
        // 跨源样式表无法读取 cssRules；其计算值仍会被保留，不影响其他节点。
      }
    }
    return false;
  };

  const applyParent = parent => {
    let state = states.get(parent);
    if (!state) { state = { children: new Map(), baselines: new Map() }; states.set(parent, state); parents.add(parent); }
    // 先撤销上一轮的 inline margin，才能读到样式表实际给出的 margin。
    restore(state);
    const style = getComputedStyle(parent);
    if (style.display !== 'flex' && style.display !== 'inline-flex') return;
    const direction = style.flexDirection || 'row';
    const horizontal = direction === 'row' || direction === 'row-reverse';
    const reverse = direction.endsWith('reverse');
    const wrap = style.flexWrap && style.flexWrap !== 'nowrap';
    const wrapReverse = style.flexWrap === 'wrap-reverse';
    const mainGap = (horizontal ? style.getPropertyValue('--sanmu-flex-gap-x') : style.getPropertyValue('--sanmu-flex-gap-y')).trim();
    const crossGap = (horizontal ? style.getPropertyValue('--sanmu-flex-gap-y') : style.getPropertyValue('--sanmu-flex-gap-x')).trim();
    if ((!mainGap || mainGap === '0px' || mainGap === 'normal') && (!wrap || !crossGap || crossGap === '0px' || crossGap === 'normal')) return;

    const children = visibleFlexChildren(parent);
    if (children.length < 2) return;
    const autoMargins = new WeakMap();
    children.forEach(child => autoMargins.set(child, {
      left: hasAutoMarginRule(child, 'marginLeft'),
      right: hasAutoMarginRule(child, 'marginRight'),
      top: hasAutoMarginRule(child, 'marginTop'),
      bottom: hasAutoMarginRule(child, 'marginBottom'),
    }));
    const lines = groupLines(children, horizontal);
    const directionRtl = style.direction === 'rtl';
    const startIsMin = horizontal ? !(reverse !== directionRtl) : !reverse;
    const mainProperty = horizontal
      ? (startIsMin ? 'marginLeft' : 'marginRight')
      : (startIsMin ? 'marginTop' : 'marginBottom');
    const crossProperty = horizontal
      ? (wrapReverse ? 'marginBottom' : 'marginTop')
      : (wrapReverse === directionRtl ? 'marginLeft' : 'marginRight');
    const mainCoordinate = horizontal ? 'left' : 'top';
    const setMargin = (child, property, gap) => {
      if (!gap || gap === '0px' || gap === 'normal') return;
      const auto = autoMargins.get(child);
      if (auto && ((property === 'marginLeft' && auto.left) || (property === 'marginRight' && auto.right) ||
        (property === 'marginTop' && auto.top) || (property === 'marginBottom' && auto.bottom))) return;
      let properties = state.children.get(child);
      if (!properties) { properties = {}; state.children.set(child, properties); }
      if (!Object.prototype.hasOwnProperty.call(properties, property)) {
        properties[property] = child.style[property];
        let baseline = state.baselines.get(child);
        if (!baseline) { baseline = {}; state.baselines.set(child, baseline); }
        baseline[property] = getComputedStyle(child)[property] || '0px';
      }
      const computed = state.baselines.get(child)?.[property] || getComputedStyle(child)[property];
      // auto margin 是布局指令，不能把它当成要相加的数值。
      if (computed === 'auto') return;
      const base = computed || '0px';
      child.style[property] = `calc(${base} + ${gap})`;
      ignoreManagedStyleMutations = true;
      clearTimeout(clearManagedStyleTimer);
      clearManagedStyleTimer = setTimeout(() => { ignoreManagedStyleMutations = false; }, 0);
    };

    const applyMainGaps = linesToApply => linesToApply.forEach(line => {
      const ordered = [...line.children].sort((a, b) => startIsMin
        ? a.rect[mainCoordinate] - b.rect[mainCoordinate]
        : b.rect[mainCoordinate] - a.rect[mainCoordinate]);
      if (mainGap && mainGap !== '0px' && mainGap !== 'normal') {
        ordered.slice(1).forEach(item => setMargin(item.child, mainProperty, mainGap));
      }
    });
    // 主轴间距可能令最后一个项目换到下一行（尤其是 column wrap）。
    // 重新测量一次后再加交叉轴间距，避免项目被错误地推到旧行的位置。
    applyMainGaps(lines);
    const linesAfterMain = groupLines(children, horizontal);
    // 主轴 gap 可能让某个项目换行；此时它在新行是第一个项目，不能继续
    // 带着上一行的主轴 margin，否则会比原生 Flex 多出一个间距。
    const firstOnLine = new Set(linesAfterMain.map(line => {
      const ordered = [...line.children].sort((a, b) => startIsMin
        ? a.rect[mainCoordinate] - b.rect[mainCoordinate]
        : b.rect[mainCoordinate] - a.rect[mainCoordinate]);
      if (mainGap && mainGap !== '0px' && mainGap !== 'normal') {
        ordered.slice(1).forEach(item => setMargin(item.child, mainProperty, mainGap));
      }
      return ordered[0].child;
    }));
    for (const child of children) {
      const properties = state.children.get(child);
      if (!properties || !Object.prototype.hasOwnProperty.call(properties, mainProperty)) continue;
      if (firstOnLine.has(child)) child.style[mainProperty] = properties[mainProperty];
    }
    // 修正主轴 margin 后项目可能回到原来的行；交叉轴间距必须以最终
    // 行位置为准，先撤掉上一轮交叉轴写入再重新测量。
    for (const child of children) {
      const properties = state.children.get(child);
      if (properties && Object.prototype.hasOwnProperty.call(properties, crossProperty)) {
        child.style[crossProperty] = properties[crossProperty];
      }
    }
    const finalLines = groupLines(children, horizontal);
    if (wrap && crossGap && crossGap !== '0px' && crossGap !== 'normal') {
      finalLines.forEach((line, lineIndex) => {
        if (lineIndex > 0) line.children.forEach(item => setMargin(item.child, crossProperty, crossGap));
      });
    }
  };

  const refresh = () => {
    queued = false;
    const current = new Set(document.body.querySelectorAll('*'));
    for (const parent of parents) {
      if (!parent.isConnected || !current.has(parent)) {
        const state = states.get(parent);
        if (state) restore(state);
        parents.delete(parent);
      }
    }
    for (const parent of current) {
      const style = getComputedStyle(parent);
      if (style.display === 'flex' || style.display === 'inline-flex') applyParent(parent);
      else {
        const state = states.get(parent);
        if (state) restore(state);
      }
    }
  };
  const schedule = () => {
    if (!queued) { queued = true; requestAnimationFrame(refresh); }
  };

  new MutationObserver(records => {
    // meter/canvas 的逐帧数值更新不会触发这里；结构、隐藏、布局样式变化才重排。
    if (records.some(record => record.type === 'childList' ||
      ['class', 'hidden', 'open', 'data-mode'].includes(record.attributeName) ||
      (record.attributeName === 'style' && !ignoreManagedStyleMutations))) schedule();
  }).observe(document.body, { subtree: true, childList: true, attributes: true,
    attributeFilter: ['class', 'style', 'hidden', 'open', 'data-mode'] });
  window.addEventListener('resize', schedule);
  refresh();
}
