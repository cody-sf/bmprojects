#!/usr/bin/env node
/**
 * Generates src/Catalog.h from the two places that already own this data:
 *
 *   - ../RNUmbrella/constants.ts                     -> display names, palette
 *     colors, effect-parameter sliders + their BLE command codes
 *   - ../libraries/BurningManLEDs/src/LightShow.cpp  -> the palette/effect id
 *     strings the firmware actually accepts
 *
 * Same contract as BMLightsWatch/scripts/generate-catalog.js: the firmware is
 * authoritative for the wire strings (an id it does not know silently falls
 * back to cool/palette_stream), the app contributes everything human-facing.
 * Read `src/LightShow.cpp`, not the drifted copy at the library root.
 *
 *   node BikeRemote/scripts/generate-catalog.js
 */
const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '../..');
const CONSTANTS = path.join(ROOT, 'RNUmbrella/constants.ts');
const LIGHTSHOW = path.join(ROOT, 'libraries/BurningManLEDs/src/LightShow.cpp');
const OUT = path.join(ROOT, 'BikeRemote/src/Catalog.h');

/** Pull `export const <name> ... = { ... };` out of the TS file and eval it. */
function extractObject(src, name) {
  const start = src.indexOf(`export const ${name}`);
  if (start < 0) throw new Error(`${name} not found in constants.ts`);
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
const commands = extractObject(ts, 'COMMON_DEVICE_COMMANDS');
const effectParams = extractObject(ts, 'EFFECT_PARAMETERS');

const fwPalettes = extractCppNames(cpp, 'paletteNameMap');
const fwEffects = extractCppNames(cpp, 'effectNameMap');

const title = s => s.replace(/_/g, ' ').replace(/\b\w/g, c => c.toUpperCase());
const cStr = s => '"' + s.replace(/["\\]/g, '\\$&') + '"';

// ---- palettes ----
// Firmware name -> app palette key: the app drops the underscores. The map's
// "*Palette" aliases and the custom slots are runtime concerns, not catalog
// rows; palettes the app has no colors for (emerald, r) are skipped because
// there is nothing to draw on a card.
const paletteRows = [];
const seenPal = new Set();
for (const fwName of fwPalettes) {
  if (/Palette$/.test(fwName) || /^custom\d$/.test(fwName)) continue;
  if (seenPal.has(fwName)) continue;
  seenPal.add(fwName);
  const entry = palettes[fwName.replace(/_/g, '')];
  if (!entry) continue;
  const colors = entry.palette.map(hex => [
    parseInt(hex.slice(1, 3), 16),
    parseInt(hex.slice(3, 5), 16),
    parseInt(hex.slice(5, 7), 16),
  ]);
  paletteRows.push({ id: fwName, name: entry.name.trim(), colors });
}

// ---- effects ----
// "off" is excluded: the Home power toggle is that control. GPS-driven
// effects carry a flag so the screens can hide them when the connected
// device reports no GPS. The matrix display ids are excluded outright: the
// display is an overlay now (BLE 0x89), driven from the Matrix screen's Show
// chips, not a mode - the firmware keeps the old ids only as aliases.
const NEEDS_GPS = new Set(['speedo', 'pstat']);
const DISPLAY_IDS = new Set(['panel_text', 'panel_bitmap', 'panel_words', 'panel_anim']);

const paramDefs = [];
const paramKey = p => `${p.command}|${p.name}|${p.min}|${p.max}|${p.default}`;
const paramIndex = new Map();
function internParam(p) {
  const key = paramKey(p);
  if (paramIndex.has(key)) return paramIndex.get(key);
  const code = commands[p.command];
  if (code === undefined) throw new Error(`no command code for ${p.command}`);
  const idx = paramDefs.length;
  paramDefs.push({ code, name: p.name, min: p.min, max: p.max, def: p.default });
  paramIndex.set(key, idx);
  return idx;
}

const effectRows = [];
for (const fwName of fwEffects) {
  if (fwName === 'off' || DISPLAY_IDS.has(fwName)) continue;
  const appKey = modeMapping[fwName] || fwName;
  const name = (backpackModes[appKey] || backpackModes[fwName] || title(fwName)).trim();
  const ep = effectParams[appKey] || effectParams[fwName];
  const custom = ep ? ep.customParams || [] : [];
  if (custom.length > 3) throw new Error(`${fwName} has ${custom.length} params; Catalog.h holds 3`);
  const slots = custom.map(internParam);
  while (slots.length < 3) slots.push(-1);
  let flags = [];
  if (NEEDS_GPS.has(fwName)) flags.push('FX_NEEDS_GPS');
  effectRows.push({ id: fwName, name, flags: flags.length ? flags.join(' | ') : '0', slots });
}

// ---- emit ----
const lines = [];
lines.push('// GENERATED by scripts/generate-catalog.js - do not edit.');
lines.push('// Sources: RNUmbrella/constants.ts (names, colors, slider ranges) and');
lines.push('// libraries/BurningManLEDs/src/LightShow.cpp (the wire id strings).');
lines.push('#pragma once');
lines.push('#include <stdint.h>');
lines.push('');
lines.push('struct BmPalette {');
lines.push('  const char* id;    // exact string LightShow::paletteNameToId matches');
lines.push('  const char* name;');
lines.push('  const uint8_t (*colors)[3];  // evenly spaced gradient stops');
lines.push('  uint8_t count;');
lines.push('};');
lines.push('');
lines.push('// One adjustable effect parameter: the BLE command that sets it (int32 LE');
lines.push('// payload) and the slider range the app uses. Values index PARAM_VALUES via');
lines.push('// code - PARAM_CODE_BASE.');
lines.push('struct BmParamDef {');
lines.push('  uint8_t code;');
lines.push('  const char* name;');
lines.push('  int16_t minV, maxV, defV;');
lines.push('};');
lines.push('');
lines.push('constexpr uint8_t FX_NEEDS_GPS = 0x01;     // hidden unless status gps=true');
lines.push('constexpr uint8_t FX_NEEDS_MATRIX = 0x02;  // hidden unless status mtxW>0');
lines.push('');
lines.push('struct BmEffect {');
lines.push('  const char* id;    // exact string LightShow::effectNameToId matches');
lines.push('  const char* name;');
lines.push('  uint8_t flags;');
lines.push('  int8_t params[3];  // indices into PARAM_DEFS, -1 empty');
lines.push('};');
lines.push('');
for (const p of paletteRows) {
  const rows = p.colors.map(c => `{${c[0]},${c[1]},${c[2]}}`).join(',');
  lines.push(`static const uint8_t PALC_${p.id}[][3] = {${rows}};`);
}
lines.push('');
lines.push('static const BmPalette PALETTES[] = {');
for (const p of paletteRows) {
  lines.push(`  {${cStr(p.id)}, ${cStr(p.name)}, PALC_${p.id}, ${p.colors.length}},`);
}
lines.push('};');
lines.push(`static const int PALETTE_COUNT = ${paletteRows.length};`);
lines.push('');
lines.push('static const BmParamDef PARAM_DEFS[] = {');
for (const d of paramDefs) {
  lines.push(`  {0x${d.code.toString(16).padStart(2, '0')}, ${cStr(d.name)}, ${d.min}, ${d.max}, ${d.def}},`);
}
lines.push('};');
lines.push(`static const int PARAM_DEF_COUNT = ${paramDefs.length};`);
lines.push('');
lines.push('static const BmEffect EFFECTS[] = {');
for (const e of effectRows) {
  lines.push(`  {${cStr(e.id)}, ${cStr(e.name)}, ${e.flags}, {${e.slots.join(', ')}}},`);
}
lines.push('};');
lines.push(`static const int EFFECT_COUNT = ${effectRows.length};`);
lines.push('');

fs.writeFileSync(OUT, lines.join('\n'));
console.log(`wrote ${path.relative(ROOT, OUT)}`);
console.log(`  ${paletteRows.length} palettes, ${effectRows.length} effects, ${paramDefs.length} param defs`);
