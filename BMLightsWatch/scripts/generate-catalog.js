#!/usr/bin/env node
/**
 * Generates BMLightsWatch/BMLightsWatch/Model/Catalog.swift from the two places
 * that already own this data:
 *
 *   - ../RNUmbrella/constants.ts      -> display names + swatch colors
 *   - ../libraries/BurningManLEDs/src/LightShow.cpp -> the palette/effect id
 *     strings the firmware actually accepts
 *
 * Read `src/LightShow.cpp`, not the copy beside it at the library root: only
 * `src` is compiled (PlatformIO builds a library's `src` directory when it has
 * one), and the two have drifted. The root copy still spells the newer palettes
 * `electric_desert`; the firmware answers to `electricdesert`.
 *
 * The firmware is the source of truth for the wire strings; the app only
 * contributes pretty names and colors. A palette the firmware does not know
 * is skipped, because sending it would silently fall back to "cool".
 */
const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '../..');
const CONSTANTS = path.join(ROOT, 'RNUmbrella/constants.ts');
const LIGHTSHOW = path.join(ROOT, 'libraries/BurningManLEDs/src/LightShow.cpp');
const OUT = path.join(ROOT, 'BMLightsWatch/BMLightsWatch/Model/Catalog.swift');

/** Pull `export const <name> ... = { ... };` out of the TS file and eval it. */
function extractObject(src, name) {
  const start = src.indexOf(`export const ${name}`);
  if (start < 0) throw new Error(`${name} not found in constants.ts`);
  // Skip past any type annotation (`: { [key: string]: string; }`) to the `=`.
  const open = src.indexOf('{', src.indexOf('=', start));
  let depth = 0;
  let end = -1;
  for (let i = open; i < src.length; i++) {
    if (src[i] === '{') depth++;
    else if (src[i] === '}') {
      depth--;
      if (depth === 0) { end = i; break; }
    }
  }
  if (end < 0) throw new Error(`unbalanced braces reading ${name}`);
  // eslint-disable-next-line no-eval
  return eval('(' + src.slice(open, end + 1) + ')');
}

/** Pull a `const <Type> <name>[] = { {"a", X::a}, ... };` table's string keys. */
function extractCppNames(src, name) {
  const start = src.indexOf(`${name}[] = {`);
  if (start < 0) throw new Error(`${name} not found in LightShow.cpp`);
  const end = src.indexOf('};', start);
  return [...src.slice(start, end).matchAll(/\{"([^"]+)"/g)].map(m => m[1]);
}

const ts = fs.readFileSync(CONSTANTS, 'utf8');
const cpp = fs.readFileSync(LIGHTSHOW, 'utf8');

const palettes = extractObject(ts, 'palettes');
const backpackModes = extractObject(ts, 'BACKPACK_MODES');
const modeMapping = extractObject(ts, 'modeMapping');

const fwPalettes = extractCppNames(cpp, 'paletteNameMap');
const fwEffects = extractCppNames(cpp, 'effectNameMap');

const title = s => s.replace(/_/g, ' ').replace(/\b\w/g, c => c.toUpperCase());
const swiftStr = s => '"' + s.replace(/["\\]/g, '\\$&') + '"';

// Firmware name -> app palette key: the app drops the underscores. Palettes the
// app has no colors for are skipped, which is also what keeps the four custom
// slots ("custom1".."custom4") out of the catalog - those are filled at runtime
// from what a device reports, not baked in here.
const paletteRows = [];
const skippedPalettes = [];
for (const fwName of fwPalettes) {
  const appKey = fwName.replace(/_/g, '');
  const entry = palettes[appKey];
  if (!entry) { skippedPalettes.push(fwName); continue; }
  paletteRows.push({ id: fwName, name: entry.name.trim(), colors: entry.palette });
}
const orphanAppPalettes = Object.keys(palettes)
  .filter(k => !fwPalettes.some(f => f.replace(/_/g, '') === k));

// Firmware effect name -> display name. The firmware's table mixes spellings:
// most entries are the long name the app also uses ("fire_plasma"), but a few
// are the app's own short key ("pstream", "speedo"). So try the long name
// through modeMapping first, then the short key directly, before falling back
// to title-casing the id - which is what produced names like "Pstream".
const effectRows = fwEffects.map(fwName => ({
  id: fwName,
  name: (
    backpackModes[modeMapping[fwName]] ||
    backpackModes[fwName] ||
    title(fwName)
  ).trim(),
}));

// The phone app writes its own short keys ("pstream", "electricdesert") rather
// than the firmware's names, and older firmware echoes those back in status.
// Keep a lookup so an alias arriving in `fx`/`pal` still selects the right row.
const effectAliases = Object.entries(modeMapping)
  .filter(([fw, appKey]) => fw !== appKey && fwEffects.includes(fw))
  .map(([fw, appKey]) => [appKey, fw]);
const paletteAliases = paletteRows
  .map(p => [p.id.replace(/_/g, ''), p.id])
  .filter(([alias, id]) => alias !== id);

const hexToSwift = hex => {
  const h = hex.replace('#', '');
  const r = parseInt(h.slice(0, 2), 16) / 255;
  const g = parseInt(h.slice(2, 4), 16) / 255;
  const b = parseInt(h.slice(4, 6), 16) / 255;
  const f = n => n.toFixed(3);
  return `Color(red: ${f(r)}, green: ${f(g)}, blue: ${f(b)})`;
};

// A watch swatch is a few millimetres wide - the full 10-stop palettes just
// turn to mud, so keep at most 5 evenly spaced stops.
const sampleColors = colors => {
  const unique = colors.filter((c, i) => i === 0 || c !== colors[i - 1]);
  if (unique.length <= 5) return unique;
  const step = (unique.length - 1) / 4;
  return Array.from({ length: 5 }, (_, i) => unique[Math.round(i * step)]);
};

const lines = [];
lines.push('// Generated by BMLightsWatch/scripts/generate-catalog.js - do not edit by hand.');
lines.push('// Regenerate after changing palettes/effects in RNUmbrella/constants.ts or');
lines.push('// libraries/BurningManLEDs/LightShow.cpp:');
lines.push('//');
lines.push('//   node BMLightsWatch/scripts/generate-catalog.js');
lines.push('');
lines.push('import SwiftUI');
lines.push('');
lines.push('/// One selectable palette. `id` is the exact string the firmware matches in');
lines.push('/// `LightShow::paletteNameToId`.');
lines.push('struct BMPalette: Identifiable, Hashable {');
lines.push('    let id: String');
lines.push('    let name: String');
lines.push('    let colors: [Color]');
lines.push('}');
lines.push('');
lines.push('/// One selectable effect. `id` is the exact string the firmware matches in');
lines.push('/// `LightShow::effectNameToId`.');
lines.push('struct BMEffect: Identifiable, Hashable {');
lines.push('    let id: String');
lines.push('    let name: String');
lines.push('}');
lines.push('');
lines.push('enum BMCatalog {');
lines.push('    static let palettes: [BMPalette] = [');
for (const p of paletteRows) {
  const colors = sampleColors(p.colors).map(hexToSwift).join(', ');
  lines.push(`        BMPalette(id: ${swiftStr(p.id)}, name: ${swiftStr(p.name)}, colors: [${colors}]),`);
}
lines.push('    ]');
lines.push('');
lines.push('    static let effects: [BMEffect] = [');
for (const e of effectRows) {
  lines.push(`        BMEffect(id: ${swiftStr(e.id)}, name: ${swiftStr(e.name)}),`);
}
lines.push('    ]');
lines.push('');
const pushAliases = (doc, name, entries) => {
  lines.push(`    /// ${doc}`);
  if (entries.length === 0) {
    lines.push(`    private static let ${name}: [String: String] = [:]`);
    return;
  }
  lines.push(`    private static let ${name}: [String: String] = [`);
  for (const [alias, id] of entries) {
    lines.push(`        ${swiftStr(alias)}: ${swiftStr(id)},`);
  }
  lines.push('    ]');
};

pushAliases(
  'Phone-app spellings that mean the same palette to us.',
  'paletteAliases',
  paletteAliases,
);
lines.push('');
pushAliases(
  'Phone-app spellings that mean the same effect to us.',
  'effectAliases',
  effectAliases,
);
lines.push('');
lines.push('    static func palette(id: String) -> BMPalette? {');
lines.push('        let key = paletteAliases[id] ?? id');
lines.push('        return palettes.first { $0.id == key }');
lines.push('    }');
lines.push('');
lines.push('    static func effect(id: String) -> BMEffect? {');
lines.push('        let key = effectAliases[id] ?? id');
lines.push('        return effects.first { $0.id == key }');
lines.push('    }');
lines.push('}');
lines.push('');

fs.mkdirSync(path.dirname(OUT), { recursive: true });
fs.writeFileSync(OUT, lines.join('\n'));

console.log(`wrote ${path.relative(ROOT, OUT)}`);
console.log(`  ${paletteRows.length} palettes, ${effectRows.length} effects`);
if (skippedPalettes.length) {
  console.log(`  firmware palettes with no colors in the app: ${skippedPalettes.join(', ')}`);
}
if (orphanAppPalettes.length) {
  console.log(`  app palettes the firmware cannot resolve: ${orphanAppPalettes.join(', ')}`);
}
