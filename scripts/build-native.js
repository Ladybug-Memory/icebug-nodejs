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

function nodeGypCommand() {
  try {
    return {
      command: process.execPath,
      args: [require.resolve('node-gyp/bin/node-gyp.js'), 'rebuild', ...process.argv.slice(2)],
    };
  } catch {
    if (process.env.npm_execpath) {
      return {
        command: process.execPath,
        args: [process.env.npm_execpath, 'exec', '--', 'node-gyp', 'rebuild', ...process.argv.slice(2)],
      };
    }

    return {
      command: process.platform === 'win32' ? 'npx.cmd' : 'npx',
      args: ['node-gyp', 'rebuild', ...process.argv.slice(2)],
    };
  }
}

const { command, args } = nodeGypCommand();
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

execFileSync(command, args, {
  cwd: root,
  env,
  stdio: 'inherit',
});
