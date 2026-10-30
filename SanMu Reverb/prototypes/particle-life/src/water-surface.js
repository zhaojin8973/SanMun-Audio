import { graphicsBackend } from './graphics.js';
const THREE = graphicsBackend();

// 水膜与水下光纹共用高度场，声音只驱动表面；不参与音频计算。
const field = `
  uniform vec4 wetBands;
  uniform vec4 dryBands;
  uniform vec4 phase;
  uniform float dryPhase;
  uniform float flow;
  uniform float wetGain;
  uniform float dryGain;
  uniform float space;
  uniform float attack;
  uniform float early;
  uniform float late;
  uniform float depth;

  float heightAt(vec2 p) {
    vec2 q = p / space;
    float r = length(q);
    float edge = 1.0 - smoothstep(0.68, 1.0, r);
    float diffusion = mix(early, late, smoothstep(0.12, 0.55, r));
    vec2 drift = depth * 0.035 * vec2(sin(q.y * 3.1 + flow), sin(q.x * 2.7 - flow * 0.79));
    vec2 w = q + drift;
    float warp = diffusion * (0.09 * sin(w.y * 4.3 + w.x * 1.7) + 0.07 * sin(w.x * 5.1 - w.y * 2.6));
    float wetHeight = 0.0;
    for (int b = 0; b < 4; b++) {
      float band = float(b);
      float k = 10.0 + band * 4.5;
      vec2 origin = diffusion * 0.12 * vec2(cos(band * 2.1), sin(band * 2.1));
      float radius = length(w - origin);
      float first = sin(k * (radius + warp) - phase[b]);
      float second = sin(k * 0.83 * length(w + origin * 1.6 + vec2(0.04, -0.07)) - phase[b] * 0.81 + 1.3);
      wetHeight += wetBands[b] * (first + diffusion * 0.50 * second) / (1.0 + diffusion * 0.50) / (1.0 + band * 0.25);
    }
    // 建立较大时，中心较轻，后部更舒展；尺寸与衰减分工保持独立。
    float rear = smoothstep(-0.45, 0.65, q.y);
    float envelope = edge * (0.52 + 0.48 * smoothstep(0.03, 0.23, r));
    envelope *= 1.0 + attack * (0.30 * rear - 0.35 * exp(-r * r * 14.0));
    float dryLevel = dot(dryBands, vec4(0.42, 0.32, 0.17, 0.09));
    // 输入局限在中心，独立于混响尺寸、预延迟及湿声增益。
    float dryRadius = length(p * vec2(1.0, 1.12));
    float dryHeight = 0.17 * dryLevel * exp(-dryRadius * dryRadius * 38.0)
      * (sin(dryRadius * 39.0 - dryPhase) + 0.24 * sin(p.x * 32.0 + p.y * 17.0 - dryPhase * 0.8));
    float activityFactor = clamp(wetGain * dot(wetBands, vec4(0.42, 0.32, 0.17, 0.09))
      + dryGain * dryLevel * exp(-dryRadius * dryRadius * 32.0), 0.0, 1.0);
    return 0.065 * edge * edge * activityFactor + 0.26 * wetGain * wetHeight * envelope + dryGain * dryHeight;
  }
`;

const vertexShader = `
  ${field}
  uniform float floorLayer;
  varying vec2 vPoint;
  varying vec3 vNormal;
  varying vec3 vWorld;
  varying float vCurvature;
  varying float vSlope;
  void main() {
    vec2 p = position.xy / vec2(7.9, 5.0);
    float d = 0.009;
    float h = heightAt(p);
    float xp = heightAt(p + vec2(d, 0.0)), xm = heightAt(p - vec2(d, 0.0));
    float yp = heightAt(p + vec2(0.0, d)), ym = heightAt(p - vec2(0.0, d));
    vec2 slope = vec2((xp - xm) / (2.0 * d * 7.9), (yp - ym) / (2.0 * d * 5.0));
    vNormal = normalize(mat3(modelMatrix) * vec3(-slope, 1.0));
    vSlope = length(slope);
    vCurvature = (xp + xm + yp + ym - 4.0 * h) / (d * d) * 0.007;
    vPoint = p;
    vec3 displaced = vec3(position.xy, mix(h, -0.17, floorLayer));
    vec4 world = modelMatrix * vec4(displaced, 1.0);
    vWorld = world.xyz;
    gl_Position = projectionMatrix * viewMatrix * world;
  }
`;

const fragmentShader = `
  uniform vec4 wetBands;
  uniform vec4 dryBands;
  uniform float wetGain;
  uniform float dryGain;
  uniform float space;
  uniform float floorLayer;
  varying vec2 vPoint;
  varying vec3 vNormal;
  varying vec3 vWorld;
  varying float vCurvature;
  varying float vSlope;
  void main() {
    float radius = length(vPoint / space);
    float mask = 1.0 - smoothstep(0.66, 1.0, radius);
    if (mask < 0.002) discard;
    float activity = clamp(dot(wetBands, vec4(0.42, 0.32, 0.17, 0.09)) * wetGain
      + dot(dryBands, vec4(0.42, 0.32, 0.17, 0.09)) * dryGain * exp(-dot(vPoint, vPoint) * 32.0), 0.0, 1.0);
    vec3 normal = normalize(vNormal);
    vec3 view = normalize(cameraPosition - vWorld);
    vec3 light = normalize(vec3(-0.45, 0.9, 0.75));
    float reflection = pow(max(0.0, dot(normal, normalize(light + view))), 88.0);
    float fresnel = pow(1.0 - max(0.0, dot(normal, view)), 3.0);
    float slope = clamp(vSlope * 1.8, 0.0, 1.0);
    if (floorLayer > 0.5) {
      // 同一高度场的曲率近似光线汇聚，作为薄而淡的底部折光。
      float focus = pow(clamp(abs(vCurvature) * 0.65, 0.0, 1.0), 2.0);
      vec3 lightColor = mix(vec3(0.38, 0.62, 0.70), vec3(0.99, 1.0, 1.0), smoothstep(-0.12, 0.20, vCurvature));
      gl_FragColor = vec4(lightColor, mask * focus * activity * 0.17);
      return;
    }
    vec4 weights = wetBands * wetGain + dryBands * dryGain * exp(-dot(vPoint, vPoint) * 32.0);
    float total = max(0.001, dot(weights, vec4(1.0)));
    vec3 tint = (weights.x * vec3(0.85, 0.71, 0.81) + weights.y * vec3(0.72, 0.74, 0.89)
      + weights.z * vec3(0.57, 0.82, 0.86) + weights.w * vec3(0.74, 0.84, 0.95)) / total;
    vec3 color = mix(vec3(0.29, 0.54, 0.63), tint, 0.20 * activity);
    color = mix(color, vec3(0.985, 0.995, 1.0), clamp(reflection * 1.5 + fresnel * 0.6, 0.0, 1.0));
    float alpha = mask * activity * (0.016 + 0.30 * slope + 0.27 * reflection + 0.055 * fresnel);
    gl_FragColor = vec4(color, min(alpha, 0.42));
  }
`;

const bounded = (value, max = 1) => Number.isFinite(value) ? Math.min(max, Math.max(0, value)) : 0;

export class WaterSurface {
  constructor() {
    this.group = new THREE.Group();
    this.group.rotation.x = -0.83;
    this.group.position.y = -0.1;
    this.group.visible = false;
    this.time = null;
    this.uniforms = {
      wetBands: { value: new THREE.Vector4() }, dryBands: { value: new THREE.Vector4() },
      phase: { value: new THREE.Vector4(0.1, 1.7, 3.1, 4.6) }, dryPhase: { value: 0 }, flow: { value: 0 },
      wetGain: { value: 0 }, dryGain: { value: 0 }, space: { value: 1 }, attack: { value: .5 },
      early: { value: 1 }, late: { value: 1 }, depth: { value: .38 },
    };
    this.geometry = new THREE.PlaneGeometry(18, 12, 192, 128);
    this.materials = [1, 0].map(layer => new THREE.ShaderMaterial({
      uniforms: { ...this.uniforms, floorLayer: { value: layer } }, vertexShader, fragmentShader,
      transparent: true, depthWrite: false, side: THREE.DoubleSide,
    }));
    for (let i = 0; i < 2; i++) {
      const mesh = new THREE.Mesh(this.geometry, this.materials[i]);
      mesh.frustumCulled = false; mesh.renderOrder = i;
      this.group.add(mesh);
    }
  }

  clear() {
    this.time = null;
    this.uniforms.wetBands.value.set(0, 0, 0, 0);
    this.uniforms.dryBands.value.set(0, 0, 0, 0);
    this.uniforms.phase.value.set(0.1, 1.7, 3.1, 4.6);
    this.uniforms.dryPhase.value = this.uniforms.flow.value = 0;
  }

  update(time, life, dry, wetGain, dryGain, motion) {
    if (!Number.isFinite(time)) return;
    if (this.time !== null && time < this.time) this.clear();
    const dt = this.time === null ? 0 : Math.min(0.06, Math.max(0, time - this.time));
    this.time = time;
    const u = this.uniforms, p = life.parameters;
    let wetActive = false, dryActive = false;
    for (let b = 0; b < 4; b++) {
      const wet = bounded(life.wet[b], 8), input = bounded(dry.levels[b], 8);
      const wetLevel = wet < 0.000025 ? 0 : Math.min(1.6, Math.sqrt(wet / 0.028));
      const dryLevel = input < 0.00003 ? 0 : Math.min(1.6, Math.sqrt(input / 0.055));
      u.wetBands.value.setComponent(b, wetLevel); u.dryBands.value.setComponent(b, dryLevel);
      wetActive ||= wetLevel > 0; dryActive ||= dryLevel > 0;
      if (motion && wetLevel > 0) u.phase.value.setComponent(b, u.phase.value.getComponent(b) + dt * (0.95 + b * 0.17));
    }
    if (motion && dryActive) u.dryPhase.value += dt * 2.7;
    if (motion && wetActive) u.flow.value += dt * bounded(p.rate, 10) * .23;
    u.wetGain.value = bounded(wetGain); u.dryGain.value = bounded(dryGain);
    u.space.value = .52 + .48 * bounded(p.size / 100);
    u.attack.value = bounded(p.attack / 100); u.early.value = bounded(p.early / 100); u.late.value = bounded(p.late / 100);
    u.depth.value = motion ? bounded(p.depth / 100) : 0;
  }
}
