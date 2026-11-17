// ABOUTME: The results of a run: one row per (players, check) with pass, fail, info or skip, the detail and the evidence files. Prints
// ABOUTME: a live line per row and writes results.json and results.md (a table of players against checks).
import { writeFileSync } from 'node:fs';
import { join } from 'node:path';

export class Report {
  constructor(meta) {
    this.meta = meta;
    this.rows = [];
  }

  // status: 'pass' | 'fail' | 'info' | 'skip'; evidence: paths relative to the output folder
  add(n, check, status, detail = '', evidence = []) {
    this.rows.push({ n, check, status, detail, evidence });
    const mark = { pass: 'PASS', fail: 'FAIL', info: 'INFO', skip: 'SKIP' }[status];
    console.log(`  ${mark}  N=${n} ${check}${detail ? ': ' + detail : ''}`);
  }

  get failures() { return this.rows.filter((r) => r.status === 'fail'); }

  markdown() {
    const checks = [...new Set(this.rows.map((r) => r.check))];
    const ns = [...new Set(this.rows.map((r) => r.n))].sort((a, b) => a - b);
    const cell = (n, check) => {
      const rows = this.rows.filter((r) => r.n === n && r.check === check);
      if (!rows.length) return '';
      if (rows.some((r) => r.status === 'fail')) return 'FAIL';
      if (rows.some((r) => r.status === 'pass')) return 'pass';
      return rows[0].status;
    };
    const out = [`# Browser multiplayer results`, '', ...Object.entries(this.meta).map(([k, v]) => `- ${k}: ${v}`), ''];
    out.push(`| check | ${ns.map((n) => `N=${n}`).join(' | ')} |`, `|---|${ns.map(() => '---').join('|')}|`);
    for (const c of checks) out.push(`| ${c} | ${ns.map((n) => cell(n, c)).join(' | ')} |`);
    out.push('', '## Details', '');
    for (const r of this.rows) {
      out.push(`- **N=${r.n} ${r.check}** ${r.status.toUpperCase()}${r.detail ? ': ' + r.detail : ''}`);
      for (const e of r.evidence) out.push(`  - ${e}`);
    }
    return out.join('\n') + '\n';
  }

  write(dir) {
    writeFileSync(join(dir, 'results.json'), JSON.stringify({ meta: this.meta, rows: this.rows }, null, 2));
    writeFileSync(join(dir, 'results.md'), this.markdown());
  }
}
