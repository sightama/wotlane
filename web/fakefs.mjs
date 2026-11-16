// ABOUTME: An in-memory stand-in for the browser's private file system (the directory and file handle calls the page scripts use),
// ABOUTME: shared by the page tests. Not shipped with the page.

export class Dir {
  constructor() { this.kind = 'directory'; this.items = new Map(); }
  async getDirectoryHandle(name, { create = false } = {}) {
    let h = this.items.get(name);
    if (!h && create) this.items.set(name, h = new Dir());
    if (!h || h.kind !== 'directory') throw new DOMException(name, 'NotFoundError');
    return h;
  }
  async getFileHandle(name, { create = false } = {}) {
    let h = this.items.get(name);
    if (!h && create) this.items.set(name, h = new File());
    if (!h || h.kind !== 'file') throw new DOMException(name, 'NotFoundError');
    return h;
  }
  async removeEntry(name) {
    if (!this.items.delete(name)) throw new DOMException(name, 'NotFoundError');
  }
  async *entries() { yield* [...this.items.entries()]; }
}

export class File {
  constructor() { this.kind = 'file'; this.bytes = new Uint8Array(0); }
  async getFile() { const b = this.bytes; return { size: b.length, arrayBuffer: async () => b.slice().buffer, text: async () => new TextDecoder().decode(b) }; }
  async createWritable() {
    const parts = [];
    return {
      write: async (d) => { parts.push(typeof d === 'string' ? new TextEncoder().encode(d) : new Uint8Array(d)); },
      close: async () => { this.bytes = new Uint8Array(await new Blob(parts).arrayBuffer()); },
    };
  }
}
