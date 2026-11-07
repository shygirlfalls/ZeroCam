import type { FilterId } from "./types";

export interface FilterDef {
  id: FilterId;
  name: string;
  desc: string;
  swatch: string;
  css?: string;
  lut?: Uint8ClampedArray;
  pixelate?: number;
  overlay?: "vhs" | "scan";
}

const hex = (h: string): [number, number, number] => [
  parseInt(h.slice(1, 3), 16),
  parseInt(h.slice(3, 5), 16),
  parseInt(h.slice(5, 7), 16),
];

function buildLut(stops: Array<[number, string]>): Uint8ClampedArray {
  const lut = new Uint8ClampedArray(256 * 3);
  for (let s = 0; s < stops.length - 1; s++) {
    const [p0, c0] = stops[s];
    const [p1, c1] = stops[s + 1];
    const [r0, g0, b0] = hex(c0);
    const [r1, g1, b1] = hex(c1);
    for (let i = p0; i <= p1; i++) {
      const t = p1 === p0 ? 0 : (i - p0) / (p1 - p0);
      lut[i * 3] = r0 + (r1 - r0) * t;
      lut[i * 3 + 1] = g0 + (g1 - g0) * t;
      lut[i * 3 + 2] = b0 + (b1 - b0) * t;
    }
  }
  return lut;
}

function curveLut(fn: (x: number) => number): Uint8ClampedArray {
  const lut = new Uint8ClampedArray(256 * 3);
  for (let i = 0; i < 256; i++) {
    const v = Math.max(0, Math.min(255, Math.round(fn(i / 255) * 255)));
    lut[i * 3] = lut[i * 3 + 1] = lut[i * 3 + 2] = v;
  }
  return lut;
}

export const FILTERS: FilterDef[] = [
  {
    id: "normal",
    name: "Normal",
    desc: "Clean camera signal",
    swatch: "#ffffff",
  },
  {
    id: "xpblue",
    name: "XP Blue",
    desc: "Cool, crisp blue cast",
    swatch: "#5da4ff",
    css: "contrast(1.08) saturate(1.08)",
    lut: buildLut([
      [0, "#0c1838"],
      [70, "#2157a0"],
      [135, "#4d97ea"],
      [200, "#bfe0ff"],
      [255, "#ffffff"],
    ]),
  },
  {
    id: "sepia",
    name: "Sepia",
    desc: "Warm photo booth tone",
    swatch: "#b68a5f",
    css: "sepia(0.85) contrast(1.08)",
  },
  {
    id: "mono",
    name: "Mono",
    desc: "Black and white",
    swatch: "#909090",
    lut: curveLut((x) => Math.pow(x, 0.92)),
    overlay: "scan",
  },
  {
    id: "vivid",
    name: "Vivid",
    desc: "Bright toy-camera color",
    swatch: "#ff5b5b",
    css: "saturate(1.5) contrast(1.12) brightness(1.04)",
  },
  {
    id: "matrix",
    name: "Green Grid",
    desc: "Retro terminal green",
    swatch: "#1dd91d",
    lut: buildLut([
      [0, "#020f02"],
      [110, "#0f5c0f"],
      [200, "#35d335"],
      [255, "#dbffdb"],
    ]),
    overlay: "scan",
  },
  {
    id: "vhs",
    name: "VHS Tape",
    desc: "Tracking noise and soft chroma",
    swatch: "#6a72a7",
    css: "saturate(0.6) contrast(1.12) brightness(1.06)",
    overlay: "vhs",
  },
  {
    id: "thermal",
    name: "Thermal",
    desc: "False heat palette",
    swatch: "#ff6e3d",
    lut: buildLut([
      [0, "#0c0a24"],
      [45, "#3f1d81"],
      [90, "#8f2e85"],
      [145, "#d84652"],
      [200, "#ff8d27"],
      [230, "#ffd44b"],
      [255, "#fff8c7"],
    ]),
  },
  {
    id: "pixel",
    name: "Pixel",
    desc: "DSi mosaic vibe",
    swatch: "#8b58ff",
    pixelate: 5,
    css: "saturate(1.24) contrast(1.1)",
  },
  {
    id: "negative",
    name: "Negative",
    desc: "Inverted colors",
    swatch: "#1a1a1a",
    css: "invert(1)",
  },
  {
    id: "dream",
    name: "Dream",
    desc: "Soft bloom pastel wash",
    swatch: "#ff9bd2",
    css: "brightness(1.12) saturate(1.15) contrast(0.94)",
    lut: buildLut([
      [0, "#101420"],
      [90, "#635fa3"],
      [155, "#9fc7ff"],
      [210, "#ffd2ec"],
      [255, "#fffefb"],
    ]),
  },
  {
    id: "warp",
    name: "Distortion",
    desc: "Classic DSi warp lens",
    swatch: "#00ff88",
    css: "contrast(1.1) saturate(1.2)",
  },
];

export function applyWarp(img: ImageData, time: number) {
  const { width: w, height: h, data } = img;
  const src = new Uint8ClampedArray(data);
  const cx = w / 2;
  const cy = h / 2;
  
  for (let y = 0; y < h; y++) {
    for (let x = 0; x < w; x++) {
      const dx = x - cx;
      const dy = y - cy;
      const dist = Math.sqrt(dx * dx + dy * dy);
      
      // Dynamic bulge/warp effect
      const factor = 1.0 + Math.sin(dist * 0.05 - time * 2) * 0.05;
      const sx = Math.floor(cx + dx * factor);
      const sy = Math.floor(cy + dy * factor);
      
      if (sx >= 0 && sx < w && sy >= 0 && sy < h) {
        const i = (y * w + x) * 4;
        const si = (sy * w + sx) * 4;
        data[i] = src[si];
        data[i + 1] = src[si + 1];
        data[i + 2] = src[si + 2];
      }
    }
  }
}

export function getBestVideoMime() {
  const types = [
    'video/mp4;codecs=h264,aac',
    'video/mp4;codecs=h264',
    'video/mp4',
    'video/webm;codecs=h264',
    'video/webm;codecs=vp9',
    'video/webm'
  ];
  return types.find(t => MediaRecorder.isTypeSupported(t)) || 'video/webm';
}

export const QUICK_EFFECTS: FilterId[] = [
  "normal",
  "xpblue",
  "sepia",
  "mono",
  "vivid",
  "matrix",
  "vhs",
  "thermal",
  "pixel",
  "negative",
  "dream",
];

export const filterDef = (id: FilterId): FilterDef =>
  FILTERS.find((f) => f.id === id) ?? FILTERS[0];

export function applyLut(img: ImageData, lut: Uint8ClampedArray) {
  const d = img.data;
  for (let i = 0; i < d.length; i += 4) {
    const y = (d[i] * 77 + d[i + 1] * 150 + d[i + 2] * 29) >> 8;
    const j = y * 3;
    d[i] = lut[j];
    d[i + 1] = lut[j + 1];
    d[i + 2] = lut[j + 2];
  }
}

let scanPattern: HTMLCanvasElement | null = null;
function getScanPattern(): HTMLCanvasElement {
  if (scanPattern) return scanPattern;
  const c = document.createElement("canvas");
  c.width = 4;
  c.height = 3;
  const x = c.getContext("2d")!;
  x.fillStyle = "rgba(0,0,0,0.18)";
  x.fillRect(0, 2, 4, 1);
  scanPattern = c;
  return c;
}

export function paintScanlines(ctx: CanvasRenderingContext2D, w: number, h: number) {
  const pat = ctx.createPattern(getScanPattern(), "repeat");
  if (!pat) return;
  ctx.save();
  ctx.fillStyle = pat;
  ctx.fillRect(0, 0, w, h);
  ctx.restore();
}

export function paintVhs(ctx: CanvasRenderingContext2D, w: number, h: number, t: number) {
  ctx.save();
  for (let i = 0; i < 70; i++) {
    const a = Math.random() * 0.08;
    ctx.fillStyle = Math.random() > 0.5 ? `rgba(255,255,255,${a})` : `rgba(0,0,0,${a + 0.04})`;
    ctx.fillRect(Math.random() * w, Math.random() * h, 1 + Math.random() * 2, 1 + Math.random() * 1.5);
  }
  const bandY = ((t * 40) % (h + 80)) - 40;
  const grad = ctx.createLinearGradient(0, bandY, 0, bandY + 28);
  grad.addColorStop(0, "rgba(255,255,255,0)");
  grad.addColorStop(0.5, "rgba(255,255,255,0.05)");
  grad.addColorStop(1, "rgba(255,255,255,0)");
  ctx.fillStyle = grad;
  ctx.fillRect(0, bandY, w, 28);
  paintScanlines(ctx, w, h);
  ctx.font = '12px "Tahoma", "Verdana", sans-serif';
  ctx.fillStyle = "rgba(255,255,255,0.9)";
  const secs = Math.floor(t % 60);
  const mins = Math.floor(t / 60) % 60;
  ctx.fillText(`PLAY  00:${String(mins).padStart(2, "0")}:${String(secs).padStart(2, "0")}`, 8, h - 10);
  ctx.restore();
}

export function paintPlasma(ctx: CanvasRenderingContext2D, w: number, h: number, t: number) {
  const img = ctx.createImageData(w, h);
  const d = img.data;
  const cx = w / 2;
  const cy = h / 2;
  let i = 0;
  for (let y = 0; y < h; y++) {
    const sy = Math.sin(y * 0.03 - t * 0.9);
    for (let x = 0; x < w; x++) {
      const v =
        Math.sin(x * 0.02 + t) +
        sy +
        Math.sin((x + y) * 0.015 + t * 0.7) +
        Math.sin(Math.hypot(x - cx, y - cy) * 0.05 - t * 1.4);
      const n = (v + 4) / 8;
      d[i] = 75 + 75 * n;
      d[i + 1] = 110 + 90 * n;
      d[i + 2] = 170 + 70 * n;
      d[i + 3] = 255;
      i += 4;
    }
  }
  ctx.putImageData(img, 0, 0);
}

const pad = (n: number) => String(n).padStart(2, "0");

export function paintDateStamp(ctx: CanvasRenderingContext2D, w: number, h: number, ts: number) {
  const d = new Date(ts);
  const text = `${pad(d.getMonth() + 1)}/${pad(d.getDate())}/${String(d.getFullYear()).slice(2)} ${pad(d.getHours())}:${pad(d.getMinutes())}`;
  ctx.save();
  ctx.font = 'bold 14px "Tahoma", "Verdana", sans-serif';
  ctx.textAlign = "right";
  ctx.shadowColor = "rgba(0,0,0,0.8)";
  ctx.shadowBlur = 0;
  ctx.shadowOffsetX = 1;
  ctx.shadowOffsetY = 1;
  ctx.fillStyle = "#ff9b2f";
  ctx.fillText(text, w - 8, h - 10);
  ctx.restore();
}
