// ABOUTME: Minimal zip archives for sharing creations: writes stored or deflated entries and reads the stored and deflated
// ABOUTME: entries common zip tools make, with the browser's own CompressionStream (no third-party code). Refuses damaged or huge archives.

const CRC_TABLE = (() => {
  const t = new Uint32Array(256);
  for (let n = 0; n < 256; n++) {
    let c = n;
    for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
    t[n] = c >>> 0;
  }
  return t;
})();

export function crc32(bytes) {
  let c = 0xffffffff;
  for (let i = 0; i < bytes.length; i++) c = CRC_TABLE[(c ^ bytes[i]) & 0xff] ^ (c >>> 8);
  return (c ^ 0xffffffff) >>> 0;
}

async function pipe(bytes, stream) {
  const out = new Response(new Blob([bytes]).stream().pipeThrough(stream));
  return new Uint8Array(await out.arrayBuffer());
}
const deflate = (bytes) => pipe(bytes, new CompressionStream('deflate-raw'));

// Inflates at most `limit` bytes: an entry that unpacks to more than its directory claims stops there (a zip bomb never fills memory)
async function inflate(bytes, limit) {
  const reader = new Blob([bytes]).stream().pipeThrough(new DecompressionStream('deflate-raw')).getReader();
  const chunks = [];
  let n = 0;
  for (;;) {
    const { done, value } = await reader.read();
    if (done) break;
    n += value.length;
    if (n > limit) {
      await reader.cancel();
      throw new Error('larger than it claims');
    }
    chunks.push(value);
  }
  return new Uint8Array(await new Blob(chunks).arrayBuffer());
}

const STORED = 0, DEFLATED = 8;
const UTF8_NAMES = 0x800;   // general purpose flag: names are UTF-8
const DOS_DATE = 0x21;       // 1980-01-01, the earliest DOS date (no clock time is kept)

// entries: [{name, data: Uint8Array}] -> the archive's bytes. Each entry is deflated unless that doesn't make it smaller.
export async function zip(entries) {
  const enc = new TextEncoder();
  const parts = [], central = [];
  let offset = 0;
  for (const { name, data } of entries) {
    const nameBytes = enc.encode(name);
    const packed = await deflate(data);
    const method = packed.length < data.length ? DEFLATED : STORED;
    const body = method === DEFLATED ? packed : data;
    const crc = crc32(data);
    const local = new DataView(new ArrayBuffer(30));
    local.setUint32(0, 0x04034b50, true);
    local.setUint16(4, 20, true);
    local.setUint16(6, UTF8_NAMES, true);
    local.setUint16(8, method, true);
    local.setUint16(12, DOS_DATE, true);
    local.setUint32(14, crc, true);
    local.setUint32(18, body.length, true);
    local.setUint32(22, data.length, true);
    local.setUint16(26, nameBytes.length, true);
    const dir = new DataView(new ArrayBuffer(46));
    dir.setUint32(0, 0x02014b50, true);
    dir.setUint16(4, 20, true);
    dir.setUint16(6, 20, true);
    dir.setUint16(8, UTF8_NAMES, true);
    dir.setUint16(10, method, true);
    dir.setUint16(14, DOS_DATE, true);
    dir.setUint32(16, crc, true);
    dir.setUint32(20, body.length, true);
    dir.setUint32(24, data.length, true);
    dir.setUint16(28, nameBytes.length, true);
    dir.setUint32(42, offset, true);
    parts.push(new Uint8Array(local.buffer), nameBytes, body);
    central.push(new Uint8Array(dir.buffer), nameBytes);
    offset += 30 + nameBytes.length + body.length;
  }
  const dirSize = central.reduce((n, p) => n + p.length, 0);
  const end = new DataView(new ArrayBuffer(22));
  end.setUint32(0, 0x06054b50, true);
  end.setUint16(8, entries.length, true);
  end.setUint16(10, entries.length, true);
  end.setUint32(12, dirSize, true);
  end.setUint32(16, offset, true);
  return new Uint8Array(await new Blob([...parts, ...central, new Uint8Array(end.buffer)]).arrayBuffer());
}

// The archive's files (folder entries left out) as [{name, data}], in the central directory's order. The sizes the directory
// claims are checked against the limits before anything is inflated, and each file's size and CRC after.
export async function unzip(bytes, { maxEntries = 256, maxBytes = 64 << 20 } = {}) {
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const notZip = () => new Error('not a zip file');
  let end = -1;
  for (let i = bytes.length - 22; i >= Math.max(0, bytes.length - 22 - 0xffff); i--)
    if (view.getUint32(i, true) === 0x06054b50) { end = i; break; }
  if (end < 0) throw notZip();
  const count = view.getUint16(end + 10, true);
  const dirAt = view.getUint32(end + 16, true);
  if (count > maxEntries) throw new Error(`too many files in the zip (${count}, at most ${maxEntries})`);
  const dec = new TextDecoder();
  const found = [];
  let total = 0;
  for (let i = 0, p = dirAt; i < count; i++) {
    if (p + 46 > bytes.length || view.getUint32(p, true) !== 0x02014b50) throw notZip();
    const method = view.getUint16(p + 10, true);
    const crc = view.getUint32(p + 16, true);
    const packedSize = view.getUint32(p + 20, true);
    const size = view.getUint32(p + 24, true);
    const nameLen = view.getUint16(p + 28, true), extraLen = view.getUint16(p + 30, true), commentLen = view.getUint16(p + 32, true);
    const localAt = view.getUint32(p + 42, true);
    const name = dec.decode(bytes.subarray(p + 46, p + 46 + nameLen));
    p += 46 + nameLen + extraLen + commentLen;
    if (name.endsWith('/')) continue;
    if (method !== STORED && method !== DEFLATED) throw new Error(`${name}: unsupported compression in the zip`);
    total += size;
    if (total > maxBytes) throw new Error(`the zip is too large (more than ${maxBytes >> 20} MB unpacked)`);
    found.push({ name, method, crc, packedSize, size, localAt });
  }
  const out = [];
  for (const f of found) {
    if (f.localAt + 30 > bytes.length || view.getUint32(f.localAt, true) !== 0x04034b50) throw notZip();
    const at = f.localAt + 30 + view.getUint16(f.localAt + 26, true) + view.getUint16(f.localAt + 28, true);
    if (at + f.packedSize > bytes.length) throw notZip();
    const body = bytes.subarray(at, at + f.packedSize);
    let data;
    try {
      data = f.method === STORED ? body.slice() : await inflate(body, f.size);
    } catch {
      throw new Error(`${f.name}: damaged in the zip`);
    }
    if (data.length !== f.size || crc32(data) !== f.crc) throw new Error(`${f.name}: damaged in the zip`);
    out.push({ name: f.name, data });
  }
  return out;
}
