// Encodes ImageData into a 24-bit uncompressed BMP blob — the same
// PC-viewable format the original CyberCamera homebrew writes to SD.

export function encodeBMP(image: ImageData): Blob {
  const { width: w, height: h, data } = image;
  const rowSize = (w * 3 + 3) & ~3;
  const pixelBytes = rowSize * h;
  const fileSize = 54 + pixelBytes;
  const buf = new ArrayBuffer(fileSize);
  const v = new DataView(buf);

  // BITMAPFILEHEADER
  v.setUint8(0, 0x42); // B
  v.setUint8(1, 0x4d); // M
  v.setUint32(2, fileSize, true);
  v.setUint32(6, 0, true);
  v.setUint32(10, 54, true);

  // BITMAPINFOHEADER
  v.setUint32(14, 40, true);
  v.setInt32(18, w, true);
  v.setInt32(22, h, true); // positive = bottom-up
  v.setUint16(26, 1, true);
  v.setUint16(28, 24, true);
  v.setUint32(30, 0, true);
  v.setUint32(34, pixelBytes, true);
  v.setInt32(38, 2835, true);
  v.setInt32(42, 2835, true);
  v.setUint32(46, 0, true);
  v.setUint32(50, 0, true);

  const out = new Uint8Array(buf, 54);
  for (let y = 0; y < h; y++) {
    const srcRow = (h - 1 - y) * w * 4;
    const dstRow = y * rowSize;
    for (let x = 0; x < w; x++) {
      const si = srcRow + x * 4;
      const di = dstRow + x * 3;
      out[di] = data[si + 2]; // B
      out[di + 1] = data[si + 1]; // G
      out[di + 2] = data[si]; // R
    }
  }
  return new Blob([buf], { type: "image/bmp" });
}

export async function dataUrlToImageData(dataUrl: string): Promise<ImageData> {
  const img = new Image();
  await new Promise<void>((resolve, reject) => {
    img.onload = () => resolve();
    img.onerror = () => reject(new Error("decode failed"));
    img.src = dataUrl;
  });
  const c = document.createElement("canvas");
  c.width = img.naturalWidth;
  c.height = img.naturalHeight;
  const ctx = c.getContext("2d", { willReadFrequently: true })!;
  ctx.drawImage(img, 0, 0);
  return ctx.getImageData(0, 0, c.width, c.height);
}

export function downloadBlob(blob: Blob, filename: string) {
  const url = URL.createObjectURL(blob);
  const a = document.createElement("a");
  a.href = url;
  a.download = filename;
  document.body.appendChild(a);
  a.click();
  a.remove();
  setTimeout(() => URL.revokeObjectURL(url), 4000);
}
