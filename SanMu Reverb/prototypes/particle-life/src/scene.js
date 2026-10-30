import { graphicsBackend } from './graphics.js';
const THREE = graphicsBackend();
import { ParticleLife } from './life.js';
import { DryField } from './dry-field.js';
import { WaterSurface } from './water-surface.js';

export class LifeScene {
  constructor(container, parameters, getTime, onFrame, appearance = {}) {
    this.container = container;
    this.parameters = parameters;
    this.getTime = getTime;
    this.onFrame = onFrame;
    this.reduced = matchMedia('(prefers-reduced-motion: reduce)');
    this.life = new ParticleLife(matchMedia('(max-width: 650px)').matches ? 4500 : 12000);
    this.dry = new DryField(matchMedia('(max-width: 650px)').matches ? 480 : 1000);
    this.view = 'overlay';
    this.wetGain = Math.sin(parameters.mix * Math.PI / 200);
    this.dryGain = 1;
    this.layerTime = null;
    this.life.motion = !this.reduced.matches;
    const transparent = appearance.transparent === true;
    this.renderer = new THREE.WebGLRenderer({ antialias: true, alpha: transparent, powerPreference: 'low-power' });
    this.renderer.setClearColor(transparent ? '#000000' : '#0b1012', transparent ? 0 : 1);
    this.renderer.setPixelRatio(Math.min(devicePixelRatio, 1.75));
    this.renderer.domElement.setAttribute('aria-label', '音乐厅三维生命粒子声场');
    this.renderer.domElement.setAttribute('role', 'img');
    container.prepend(this.renderer.domElement);
    this.scene = new THREE.Scene();
    this.camera = new THREE.PerspectiveCamera(40, 1, .1, 60);
    this.geometry = new THREE.BufferGeometry();
    for (const [name, array, size] of [
      ['position', this.life.position, 3], ['particleColor', this.life.color, 3],
      ['opacity', this.life.opacity, 1], ['pointSize', this.life.pointSize, 1],
    ]) this.geometry.setAttribute(name, new THREE.BufferAttribute(array, size).setUsage(THREE.DynamicDrawUsage));
    this.material = new THREE.ShaderMaterial({
      uniforms: { pixelRatio: { value: this.renderer.getPixelRatio() }, layerGain: { value: 1 }, exposure: { value: 1 }, saturation: { value: 1 }, pearlAmount: { value: .1 } },
      vertexShader: `
        attribute vec3 particleColor;
        attribute float opacity;
        attribute float pointSize;
        uniform float pixelRatio;
        uniform float layerGain;
        varying vec3 vColor;
        varying float vOpacity;
        void main() {
          vec4 mv = modelViewMatrix * vec4(position, 1.0);
          gl_Position = projectionMatrix * mv;
          gl_PointSize = clamp(pointSize * pixelRatio * 13.5 / -mv.z, 1.0, 6.0 * pixelRatio);
          vColor = particleColor;
          vOpacity = opacity * layerGain;
        }
      `,
      fragmentShader: `
        uniform float exposure;
        uniform float saturation;
        uniform float pearlAmount;
        varying vec3 vColor;
        varying float vOpacity;
        void main() {
          float r = length(gl_PointCoord - vec2(0.5)) * 2.0;
          if (r > 1.0 || vOpacity < 0.002) discard;
          float core = (exp(-r * r * 3.4) + 0.10 * exp(-r * r * 1.1)) * (1.0 - smoothstep(0.72, 1.0, r));
          float luminance = dot(vColor, vec3(0.2126, 0.7152, 0.0722));
          vec3 color = clamp(mix(vec3(luminance), vColor, saturation), 0.0, 1.0);
          vec3 pearl = mix(color, vec3(1.0), pearlAmount * exp(-r * r * 10.0));
          float alpha = vOpacity * core * exposure;
          gl_FragColor = vec4(pearl, alpha);
        }
      `,
      transparent: true, depthWrite: false, blending: THREE.AdditiveBlending,
    });
    this.points = new THREE.Points(this.geometry, this.material);
    this.points.frustumCulled = false;
    this.scene.add(this.points);
    this.dryGeometry = new THREE.BufferGeometry();
    for (const [name, array, size] of [
      ['position', this.dry.position, 3], ['particleColor', this.dry.color, 3], ['opacity', this.dry.opacity, 1],
    ]) this.dryGeometry.setAttribute(name, new THREE.BufferAttribute(array, size).setUsage(THREE.DynamicDrawUsage));
    this.dryMaterial = new THREE.ShaderMaterial({
      uniforms: { layerGain: { value: 1 } },
      vertexShader: `
        attribute vec3 particleColor;
        attribute float opacity;
        uniform float layerGain;
        varying vec3 vColor;
        varying float vOpacity;
        void main() {
          gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
          vColor = particleColor;
          vOpacity = opacity * layerGain;
        }
      `,
      fragmentShader: `
        varying vec3 vColor;
        varying float vOpacity;
        void main() {
          if (vOpacity < 0.002) discard;
          gl_FragColor = vec4(vColor, vOpacity);
        }
      `,
      transparent: true, depthWrite: false, blending: THREE.AdditiveBlending,
    });
    this.dryLines = new THREE.LineSegments(this.dryGeometry, this.dryMaterial);
    this.dryLines.frustumCulled = false;
    this.scene.add(this.dryLines);
    this.source = new THREE.Mesh(new THREE.RingGeometry(.017, .025, 32), new THREE.MeshBasicMaterial({ color: '#9dada8', transparent: true, opacity: .3, side: THREE.DoubleSide }));
    this.scene.add(this.source);
    this.water = new WaterSurface();
    this.scene.add(this.water.group);
    this.setAppearance(appearance);
    this.resizeObserver = new ResizeObserver(() => this.resize());
    this.resizeObserver.observe(container);
    this.resize();
    this.lastFrame = this.frameCount = this.frameMs = 0;
    this.tick = this.tick.bind(this);
    this.animation = null;
    this.setEnabled(true);
    document.addEventListener('visibilitychange', () => this.setEnabled(this.enabled));
    this.renderer.domElement.addEventListener('webglcontextlost', event => { event.preventDefault(); this.onFrame({ error: '画面暂停，正在恢复图形连接。' }); });
    this.renderer.domElement.addEventListener('webglcontextrestored', () => { this.clear(); this.onFrame({ error: '' }); });
  }

  ingest(metrics) {
    if (!this.enabled) return;
    this.life.ingest(metrics, this.parameters); this.dry.ingest(metrics, !document.hidden);
  }
  setEnabled(value) {
    const enabled = value === true;
    // 重新开启时清除停用期间的过期画面，音频处理与电平不受影响。
    if (enabled && this.enabled === false) this.clear();
    this.enabled = enabled;
    clearTimeout(this.animation); this.animation = null;
    this.renderer.domElement.hidden = !enabled;
    if (enabled) {
      this.lastFrame = 0; this.layerTime = null;
      this.animation = setTimeout(() => this.tick(performance.now()), 16);
    }
  }
  clear() { this.life.clear(); this.dry.clear(); this.water.clear(); }
  setView(view) {
    if (!['dry', 'wet', 'overlay'].includes(view)) return;
    this.view = view;
  }
    // 两种材质共用信号时钟和观察规则，切换外观不重建声场或音频。
  setAppearance({ exposure = 1, saturation = 1, pearl = .1, surface = 'dust' } = {}) {
    const bounded = (value, min, max, fallback) => Number.isFinite(value) ? Math.max(min, Math.min(max, value)) : fallback;
    this.material.uniforms.exposure.value = bounded(exposure, .5, 2, 1);
    this.material.uniforms.saturation.value = bounded(saturation, .5, 2, 1);
    this.material.uniforms.pearlAmount.value = bounded(pearl, 0, .3, .1);
    this.surface = surface === 'water' ? 'water' : 'dust';
    this.water.group.visible = this.surface === 'water';
    this.points.visible = this.dryLines.visible = this.source.visible = this.surface === 'dust';
  }
  resize() {
    const { width, height } = this.container.getBoundingClientRect();
    if (width <= 0 || height <= 0) return;
    this.renderer.setSize(width, height, false);
    this.camera.aspect = width / height;
    const distance = Math.max(11.8, 12.5 / this.camera.aspect);
    this.camera.position.set(0, 1.2, distance);
    this.camera.lookAt(0, 0, 0);
    this.camera.updateProjectionMatrix();
  }

  tick(now) {
    if (!this.enabled) { this.animation = null; return; }
    const start = performance.now();
    this.animation = setTimeout(() => this.tick(performance.now()), 16);
    this.life.motion = !this.reduced.matches;
    this.dry.motion = !this.reduced.matches;
    if (this.reduced.matches && now - this.lastFrame < 65) return;
    this.lastFrame = now;
    const time = this.getTime();
    this.life.update(time, this.parameters);
    this.dry.update(time);
    const dt = this.layerTime === null ? 1 / 60 : Math.max(0, (now - this.layerTime) / 1000);
    this.layerTime = now;
    const follow = 1 - Math.exp(-dt / .06);
    // 混响与叠加统一按声音的湿声增益淡化尘埃，输入参考、粒子的空间与生命保持独立。
    const mix = Number.isFinite(this.parameters.mix) ? Math.min(100, Math.max(0, this.parameters.mix)) : 100;
    const wetTarget = this.view === 'dry' ? 0 : Math.sin(mix * Math.PI / 200);
    this.wetGain += (wetTarget - this.wetGain) * follow;
    this.dryGain += ((this.view === 'wet' ? 0 : 1) - this.dryGain) * follow;
    this.material.uniforms.layerGain.value = this.wetGain;
    this.dryMaterial.uniforms.layerGain.value = this.dryGain;
    this.water.update(time, this.life, this.dry, this.wetGain, this.dryGain, !this.reduced.matches);
    for (const attribute of Object.values(this.geometry.attributes)) attribute.needsUpdate = true;
    for (const name of ['position', 'opacity']) this.dryGeometry.attributes[name].needsUpdate = true;
    const energy = this.life.wet.reduce((sum, value) => sum + value, 0) * this.wetGain
      + this.dry.levels.reduce((sum, value) => sum + value, 0) * this.dryGain;
    this.source.material.opacity = .28 * Math.exp(-energy * 240);
    this.renderer.render(this.scene, this.camera);
    this.frameCount++;
    this.frameMs = performance.now() - start;
    if (this.frameCount % 12 === 0) this.onFrame({ count: this.life.count, wet: this.life.metrics.wetRms || 0, frameMs: this.frameMs });
  }
}
