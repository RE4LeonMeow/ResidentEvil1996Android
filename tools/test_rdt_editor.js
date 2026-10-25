// test_rdt_editor.js - headless tests for tools/rdt_event_editor.html.
//
//   node tools/test_rdt_editor.js            # assertions + full sweep
//   node tools/test_rdt_editor.js --quick    # assertions only
//
// Run from the repo root; it reads the RDTs under assets/USA/Stage*/.
//
// The editor is a single self-contained HTML page, so there is nothing to
// import. This extracts its one <script> block, stubs just enough DOM for the
// module to evaluate, and drives parse/rebuild directly.
//
// The property that matters most is that an UNEDITED round-trip is
// byte-identical. If that regresses, every save silently corrupts the file, and
// nothing in the UI would show it.
'use strict';
const fs = require('fs');
const path = require('path');

const HTML = path.join(__dirname, 'rdt_event_editor.html');
const js = /<script[^>]*>([\s\S]*?)<\/script>/.exec(fs.readFileSync(HTML, 'utf8'))[1];

const noop = () => {};
const el = new Proxy({}, {
  get: (t, k) => (k === 'appendChild' || k === 'addEventListener' || k === 'remove' ||
                  k === 'scrollIntoView') ? noop
               : (k === 'classList') ? { add: noop, remove: noop }
               : (k === 'value' || k === 'innerHTML' || k === 'textContent') ? ''
               : el,
  set: () => true,
});
global.document = { getElementById: () => el, createElement: () => el,
                    querySelectorAll: () => [], addEventListener: noop, body: el };
global.window = { addEventListener: noop };
global.alert = noop;
global.confirm = () => true;

const M = new Function(js + `
  return { parseRdt, rebuildRegion, buildFile, segBytes, scdFieldsFor,
           readField, writeField, scdTemplates, SCD_CMDS, SCD_VLEN_TOTAL,
           setD: v => { D = v; } };
`)();

let fails = 0;
const ok = (c, m) => { console.log((c ? '  PASS  ' : '  FAIL  ') + m); if (!c) fails++; };
const same = (a, b) => {
  if (a.length !== b.length) return false;
  for (let i = 0; i < a.length; i++) if (a[i] !== b[i]) return false;
  return true;
};
const roomFiles = () => {
  const root = path.join('assets', 'USA');
  let out = [];
  for (const d of fs.readdirSync(root).filter(x => /^Stage\d$/.test(x)))
    out = out.concat(fs.readdirSync(path.join(root, d))
      .filter(f => /^ROOM.*\.RDT$/i.test(f)).map(f => path.join(root, d, f)));
  return out;
};

// ---------------------------------------------------------------------------
// Room 1110's init script is the reference case: the mirror pair that needs
// both the SCD section support and the MSB-counted bit index to read correctly.
// ---------------------------------------------------------------------------
console.log('\n== room 1110 init: mirror decode and operand editing ==');
{
  const f = path.join('assets', 'USA', 'Stage1', 'ROOM1110.RDT');
  const bytes = new Uint8Array(fs.readFileSync(f));
  const D = M.parseRdt('ROOM1110.RDT', bytes, 0x60);
  M.setD(D);

  let mirror = null, biton = null;
  for (const seg of D.segs) for (const it of seg.items) {
    if (it.op === 0x0F) mirror = it;
    if (it.op === 0x05 && it.bytes[1] === 5) biton = it;
  }
  ok(!!mirror && !!biton, 'mirror_set and the bank-5 bit_op both found');
  ok(mirror && /axis=X/.test(mirror.note) && /plane=X=12500/.test(mirror.note),
     'mirror note: ' + (mirror ? mirror.note : '-'));
  ok(biton && /bit=0\(msbidx 31\)/.test(biton.note) && / set\b/.test(biton.note) &&
     /mirror reflection pass enabled/.test(biton.note),
     'bit_op note: ' + (biton ? biton.note : '-'));

  const F = M.scdFieldsFor(0x0F);
  // Fields are read through the FULL item bytes with base=1 (opcode excluded),
  // exactly as buildEditor does - passing arg slices here once hid a bug where
  // every SCD field editor was shifted one byte onto the opcode.
  ok(M.readField(mirror.bytes, F[1], 1) === 4000,  'extent min = 4000');
  ok(M.readField(mirror.bytes, F[2], 1) === 6300,  'extent max = 6300');
  ok(M.readField(mirror.bytes, F[3], 1) === 12500, 'plane = 12500');

  const B = M.scdFieldsFor(0x05);
  // Field order for bit_test/bit_op: [bank, flagconst, bit-in-dword, dword, cond].
  ok(M.readField(biton.bytes, B[0], 1) === 5,  'flag bank = 5 (main_state_flags)');
  ok(M.readField(biton.bytes, B[1], 1) === 0x1F, 'flag constant id = selector byte 0x1F');
  ok(M.readField(biton.bytes, B[2], 1) === 31, 'bit in dword (from MSB) = 31, i.e. bit 0');
  ok(M.readField(biton.bytes, B[3], 1) === 0,  'dword (byte offset / 4) = 0');
  ok(M.readField(biton.bytes, B[4], 1) === 0,  'operation = 0 (set)');

  const before = biton.bytes[2];
  M.writeField(biton.bytes, B[2], 5, 1);
  ok(biton.bytes[2] === ((before & 0xE0) | 5), 'sub-byte write preserves the offset bits');
  M.writeField(biton.bytes, B[2], 31, 1);
  ok(biton.bytes[2] === before, 'sub-byte write round-trips');
  // Picking a named constant rewrites the whole selector byte.
  M.writeField(biton.bytes, B[1], 0x0B, 1);
  ok(biton.bytes[2] === 0x0B, 'flag constant write sets the selector byte');
  M.writeField(biton.bytes, B[1], 0x1F, 1);
  ok(biton.bytes[2] === before, 'flag constant write round-trips');
}

// ---------------------------------------------------------------------------
// Regression: every SCD bit_test / bit_op in every room, the field editors
// (read through it.bytes with base 1) must agree with the raw operands.
// Caught on ROOM4050: the fields were reading the opcode byte as the bank.
// ---------------------------------------------------------------------------
console.log('\n== bit_test / bit_op field editors agree with the raw operands, all rooms ==');
{
  let checks = 0, bad = 0;
  for (const f of roomFiles()) {
    const bytes = new Uint8Array(fs.readFileSync(f));
    for (const sec of [0x60, 0x64, 0x68]) {
      let D;
      try { D = M.parseRdt(path.basename(f), bytes, sec); } catch (e) { continue; }
      for (const seg of D.segs) for (const it of seg.items) {
        if (it.kind !== 'scd' || (it.op !== 0x04 && it.op !== 0x05)) continue;
        const F = M.scdFieldsFor(it.op);
        const a = it.bytes.slice(1);
        checks++;
        if (M.readField(it.bytes, F[0], 1) !== a[0] ||
            M.readField(it.bytes, F[1], 1) !== (a[1] & 0xFF) ||
            M.readField(it.bytes, F[2], 1) !== (a[1] & 0x1F) ||
            M.readField(it.bytes, F[3], 1) !== (a[1] & 0xE0) >> 3 ||
            M.readField(it.bytes, F[4], 1) !== a[2]) {
          bad++;
          if (bad <= 5) console.log('        ' + path.basename(f) + ' 0x' + sec.toString(16) +
                                    ' ' + hexs0(it.bytes));
        }
      }
    }
  }
  ok(checks > 0 && bad === 0, checks + ' bit_test/bit_op items checked, ' + bad + ' mismatches');
}
function hexs0(b) { return Array.from(b, x => x.toString(16).padStart(2, '0')).join(' '); }

console.log('\n== an operand edit rewrites exactly the bytes it should ==');
{
  const f = path.join('assets', 'USA', 'Stage1', 'ROOM1110.RDT');
  const bytes = new Uint8Array(fs.readFileSync(f));
  const D = M.parseRdt('ROOM1110.RDT', bytes, 0x60);
  M.setD(D);
  let mirror = null;
  for (const seg of D.segs) for (const it of seg.items) if (it.op === 0x0F) mirror = it;
  mirror.bytes[1] = 0x03;                       // 0x02 -> 0x03
  const r = M.buildFile();
  const diffs = [];
  for (let i = 0; i < bytes.length; i++) if (r.out[i] !== bytes[i]) diffs.push(i);
  ok(r.fits, 'rebuilt section still fits in place');
  ok(diffs.length === 1 && diffs[0] === 0x20ff7 && r.out[0x20ff7] === 0x03,
     'one byte changed, at 0x20ff7, to 0x03');
}

console.log('\n== block size words are recomputed on rebuild ==');
{
  const bytes = new Uint8Array(fs.readFileSync(path.join('assets','USA','Stage1','ROOM1110.RDT')));
  const D = M.parseRdt('ROOM1110.RDT', bytes, 0x60);
  M.setD(D);
  const seg = D.segs[0];
  const n = M.segBytes(seg).length;
  seg.items[0].bytes.push(0x00);
  const r = M.rebuildRegion();
  ok((r.body[0] | (r.body[1] << 8)) === n + 1, 'size word tracks the block length');
}

console.log('\n== SCD insert templates match the opcode widths ==');
{
  // A template whose length disagrees with SCD_CMDS / SCD_VLEN_TOTAL would
  // desync the stream the moment it is inserted.
  const widthOf = (b) => {
    const spec = M.SCD_CMDS[b[0]];
    if (!spec) return -1;
    // cmd_if / cmd_else are 2 bytes themselves; their second byte is a JUMP
    // length over the body, so a compound 'if <body> end_if' template is
    // 2 + skipLen long. Check that arithmetic instead of the base width.
    if (b[0] === 0x01 || b[0] === 0x02) return 2 + b[1];
    const vlen = M.SCD_VLEN_TOTAL[b[0]];
    return vlen ? vlen(b.slice(1)) : 1 + spec[1];
  };
  let n = 0, bad = 0;
  for (const t of M.scdTemplates()) {
    if (!t.bytes) continue;
    n++;
    if (t.bytes.length !== widthOf(t.bytes)) {
      bad++;
      console.log('        ' + t.label + ': ' + t.bytes.length + ' bytes, expected ' + widthOf(t.bytes));
    }
  }
  ok(bad === 0, n + ' SCD templates checked, ' + bad + ' width mismatches');
}

// ---------------------------------------------------------------------------
// The sweep. Every room, every section: parse then rebuild with no edits and
// require the output to equal the input byte for byte. Sections that fail to
// parse are counted, not failed - plenty of rooms ship an empty main or event
// section, and a section whose opcode widths are not all known yet keeps its
// undecoded tail as one opaque item, which still round-trips.
// ---------------------------------------------------------------------------
if (process.argv.indexOf('--quick') < 0) {
  console.log('\n== unedited round-trip is byte-identical, all rooms ==');
  const per = { 0x60:{ok:0,absent:0,undec:0}, 0x64:{ok:0,absent:0,undec:0}, 0x68:{ok:0,absent:0,undec:0} };
  const bad = [];
  const files = roomFiles();
  for (const f of files) {
    const bytes = new Uint8Array(fs.readFileSync(f));
    for (const sec of [0x60, 0x64, 0x68]) {
      let D;
      try { D = M.parseRdt(path.basename(f), bytes, sec); }
      catch (e) { per[sec].absent++; continue; }
      try {
        M.setD(D);
        if (!same(M.buildFile().out, bytes)) { bad.push(path.basename(f) + ' 0x' + sec.toString(16)); continue; }
        per[sec].ok++;
        if (D.segs.some(g => g.items.some(i => /unknown width|undecoded/.test((i.name||'') + (i.note||'')))))
          per[sec].undec++;
      } catch (e) {
        bad.push(path.basename(f) + ' 0x' + sec.toString(16) + ' THREW ' + e.message);
      }
    }
  }
  for (const k of [0x60, 0x64, 0x68])
    console.log('    0x' + k.toString(16) + '  identical=' + per[k].ok +
                '  absent=' + per[k].absent + '  (with an undecoded tail: ' + per[k].undec + ')');
  ok(bad.length === 0, files.length + ' rooms swept, ' + bad.length + ' mismatches');
  bad.slice(0, 20).forEach(b => console.log('        ' + b));
}

console.log('\n' + (fails ? fails + ' FAILURE(S)' : 'all checks passed'));
process.exit(fails ? 1 : 0);
