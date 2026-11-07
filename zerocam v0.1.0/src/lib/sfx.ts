// Tiny synthesized SFX engine — no audio assets, pure WebAudio oscillators.

class Sfx {
  private ctx: AudioContext | null = null;
  private master: GainNode | null = null;
  private noiseBuf: AudioBuffer | null = null;
  muted = false;

  unlock() {
    if (this.ctx) {
      if (this.ctx.state === "suspended") void this.ctx.resume();
      return;
    }
    const AC: typeof AudioContext | undefined =
      window.AudioContext ??
      (window as unknown as { webkitAudioContext?: typeof AudioContext }).webkitAudioContext;
    if (!AC) return;
    this.ctx = new AC();
    this.master = this.ctx.createGain();
    this.master.gain.value = 0.22;
    this.master.connect(this.ctx.destination);

    const len = this.ctx.sampleRate;
    this.noiseBuf = this.ctx.createBuffer(1, len, this.ctx.sampleRate);
    const d = this.noiseBuf.getChannelData(0);
    for (let i = 0; i < len; i++) d[i] = Math.random() * 2 - 1;
  }

  setMuted(m: boolean) {
    this.muted = m;
  }

  private tone(
    freq: number,
    dur: number,
    opts: { type?: OscillatorType; gain?: number; slideTo?: number; delay?: number } = {},
  ) {
    if (!this.ctx || !this.master || this.muted) return;
    const t0 = this.ctx.currentTime + (opts.delay ?? 0);
    const osc = this.ctx.createOscillator();
    const g = this.ctx.createGain();
    osc.type = opts.type ?? "square";
    osc.frequency.setValueAtTime(freq, t0);
    if (opts.slideTo) osc.frequency.exponentialRampToValueAtTime(opts.slideTo, t0 + dur);
    g.gain.setValueAtTime(0, t0);
    g.gain.linearRampToValueAtTime(opts.gain ?? 0.5, t0 + 0.006);
    g.gain.exponentialRampToValueAtTime(0.001, t0 + dur);
    osc.connect(g).connect(this.master);
    osc.start(t0);
    osc.stop(t0 + dur + 0.05);
  }

  private noise(
    dur: number,
    opts: { gain?: number; filterFreq?: number; type?: BiquadFilterType; delay?: number } = {},
  ) {
    if (!this.ctx || !this.master || !this.noiseBuf || this.muted) return;
    const t0 = this.ctx.currentTime + (opts.delay ?? 0);
    const src = this.ctx.createBufferSource();
    src.buffer = this.noiseBuf;
    src.loop = true;
    const f = this.ctx.createBiquadFilter();
    f.type = opts.type ?? "highpass";
    f.frequency.value = opts.filterFreq ?? 1800;
    const g = this.ctx.createGain();
    g.gain.setValueAtTime(0, t0);
    g.gain.linearRampToValueAtTime(opts.gain ?? 0.3, t0 + 0.008);
    g.gain.exponentialRampToValueAtTime(0.001, t0 + dur);
    src.connect(f).connect(g).connect(this.master);
    src.start(t0);
    src.stop(t0 + dur + 0.05);
  }

  /** UI blip */
  ui(f = 1180) {
    this.tone(f, 0.055, { type: "square", gain: 0.24 });
  }
  move() {
    this.tone(760, 0.04, { type: "triangle", gain: 0.3 });
  }
  back() {
    this.tone(520, 0.07, { type: "square", gain: 0.22 });
    this.tone(390, 0.07, { type: "square", gain: 0.18, delay: 0.055 });
  }
  error() {
    this.tone(220, 0.16, { type: "sawtooth", gain: 0.22, slideTo: 140 });
  }

  shutter() {
    this.noise(0.06, { gain: 0.5, filterFreq: 2600 });
    this.tone(1900, 0.035, { type: "square", gain: 0.28 });
    this.noise(0.09, { gain: 0.4, filterFreq: 1200, delay: 0.07, type: "bandpass" });
    this.tone(340, 0.06, { type: "triangle", gain: 0.2, delay: 0.075 });
  }

  countTick(final: boolean) {
    this.tone(final ? 1560 : 880, final ? 0.22 : 0.09, {
      type: "sine",
      gain: 0.4,
    });
  }

  whoosh(opening: boolean) {
    this.noise(0.22, {
      gain: 0.2,
      filterFreq: opening ? 900 : 500,
      type: "bandpass",
    });
    this.tone(opening ? 420 : 720, 0.18, {
      type: "sine",
      gain: 0.16,
      slideTo: opening ? 880 : 330,
    });
  }

  power() {
    this.tone(140, 0.5, { type: "sawtooth", gain: 0.16, slideTo: 660 });
    this.noise(0.4, { gain: 0.08, filterFreq: 400, type: "lowpass" });
  }

  jingle() {
    const seq = [523, 659, 784, 1047];
    seq.forEach((f, i) =>
      this.tone(f, 0.14, { type: "square", gain: 0.2, delay: 0.32 + i * 0.09 }),
    );
    this.tone(1568, 0.3, { type: "sine", gain: 0.14, delay: 0.32 + 4 * 0.09 });
  }

  powerDown() {
    this.tone(520, 0.4, { type: "sawtooth", gain: 0.14, slideTo: 90 });
  }

  launch() {
    this.tone(392, 0.08, { type: "square", gain: 0.22 });
    this.tone(587, 0.08, { type: "square", gain: 0.22, delay: 0.07 });
    this.tone(880, 0.16, { type: "square", gain: 0.24, delay: 0.14 });
  }

  toggle(on: boolean) {
    this.tone(on ? 990 : 620, 0.06, { type: "triangle", gain: 0.3 });
  }
}

export const sfx = new Sfx();
