#!/usr/bin/env node
'use strict';

const { execFileSync } = require('child_process');
const fs = require('fs');
const path = require('path');

const root = path.join(__dirname, '..');

function useHomebrewLlvmIfAvailable(env) {
  if (process.platform !== 'darwin' || (env.CC && env.CXX)) return env;

  const prefixes = [
    '/opt/homebrew/opt/llvm/bin',
    '/usr/local/opt/llvm/bin',
  ];

  for (const prefix of prefixes) {
    const cc = path.join(prefix, 'clang');
    const cxx = path.join(prefix, 'clang++');
    if (fs.existsSync(cc) && fs.existsSync(cxx)) {
      return {
        ...env,
        CC: cc,
        CXX: cxx,
      };
    }
  }

  return env;
}

const args = ['node-gyp', 'rebuild', ...process.argv.slice(2)];
const env = useHomebrewLlvmIfAvailable(process.env);

if (process.platform === 'darwin') {
  if (env.CXX) {
    console.log(`icebug: building native addon with ${env.CXX}`);
  } else {
    console.warn(
      'icebug: Homebrew LLVM was not found; falling back to the system C++ compiler.'
    );
  }
}

execFileSync('npm', ['exec', '--', ...args], {
  cwd: root,
  env,
  stdio: 'inherit',
});
