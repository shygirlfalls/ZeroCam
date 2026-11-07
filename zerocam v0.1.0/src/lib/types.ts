export type PowerState = "off" | "boot" | "home" | "app";
export type CamStatus = "idle" | "requesting" | "live" | "denied";
export type Facing = "user" | "environment";
export type TimerOpt = 0 | 4 | 6 | 8 | 10;
export type ScaleOpt = 1 | 2;
export type CaptureMode = "photo" | "video";
export type MediaKind = "photo" | "video";

export type FilterId =
  | "normal"
  | "xpblue"
  | "sepia"
  | "mono"
  | "vivid"
  | "matrix"
  | "vhs"
  | "thermal"
  | "pixel"
  | "negative"
  | "dream"
  | "warp";

export interface MediaItem {
  id: string;
  kind: MediaKind;
  name: string;
  thumbUrl: string;
  dataUrl?: string;
  blobUrl?: string;
  mime: string;
  w: number;
  h: number;
  filter: FilterId;
  mirror: boolean;
  dateStamp: boolean;
  ts: number;
  duration?: number;
}

export interface Settings {
  filter: FilterId;
  timer: TimerOpt;
  scale: ScaleOpt;
  hud: boolean;
  shutterSound: boolean;
  dateStamp: boolean;
  facing: Facing;
  zoom: number; // 1.0 - 4.0x
  mode: CaptureMode;
  mirror: boolean;
  draw: boolean;
  drawColor: string;
  brightness: number;  // 0.5 - 2.0, default 1
  contrast: number;    // 0.5 - 2.0, default 1
  saturation: number;  // 0.0 - 2.0, default 1
}

export const DEFAULT_SETTINGS: Settings = {
  filter: "xpblue",
  timer: 0,
  scale: 1,
  hud: true,
  shutterSound: true,
  dateStamp: true,
  facing: "user",
  zoom: 1,
  mode: "photo",
  mirror: false,
  draw: false,
  drawColor: "#ff0000",
  brightness: 1,
  contrast: 1,
  saturation: 1,
};

export const MAX_MEDIA = 60;

export type ButtonId =
  | "A"
  | "B"
  | "X"
  | "Y"
  | "L"
  | "R"
  | "START"
  | "SELECT"
  | "DU"
  | "DD"
  | "DL"
  | "DR";

export const pad = (n: number, len = 2) => String(n).padStart(len, "0");

export function stampName(ts: number, kind: MediaKind) {
  const d = new Date(ts);
  const ymd = `${d.getFullYear()}${pad(d.getMonth() + 1)}${pad(d.getDate())}`;
  const hms = `${pad(d.getHours())}${pad(d.getMinutes())}${pad(d.getSeconds())}`;
  return `${kind === "video" ? "VID" : "PIC"}_${ymd}_${hms}`;
}
