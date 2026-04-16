#!/usr/bin/env node
/**
 * install.js — postinstall entry point
 *
 * If a prebuilt binary for this platform/arch is already bundled in the
 * package (prebuilds/{platform}-{arch}/icebug.node) we use it as-is and
 * skip the compile step entirely.  That covers every consumer who does a
 * plain `npm install`.
 *
 * Only when no prebuilt is found do we fall back to compiling from source
 * (requires `node-gyp` and the vendor libs fetched by download-icebug.sh).
 */

'use strict';

const { execSync } = require('child_process');
const fs   = require('fs');
const path = require('path');

const root        = path.join(__dirname, '..');
const platformKey = `${process.platform}-${process.arch}`;
const prebuilt    = path.join(root, 'prebuilds', platformKey, 'icebug.node');

if (fs.existsSync(prebuilt)) {
  console.log(`icebug: prebuilt binary found for ${platformKey}, skipping compilation.`);
  process.exit(0);
}

console.log(`icebug: no prebuilt binary for ${platformKey}, compiling from source…`);

try {
  execSync('npm run download', { cwd: root, stdio: 'inherit' });
  execSync('node-gyp rebuild',  { cwd: root, stdio: 'inherit' });
} catch (err) {
  console.error('icebug: compilation failed:', err.message);
  process.exit(1);
}
