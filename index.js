// index.js (ESM)
import { createRequire } from 'node:module';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

const require = createRequire(import.meta.url);
const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

// Load compiled addon via node-gyp-build (works for both dev & prebuilt)
const native = (() => {
  try {
    return require('node-gyp-build')(__dirname);
  } catch {
    return require('./build/Release/gamepad.node');
  }
})();

function hex(buf) {
  return [...buf].map(b => b.toString(16).padStart(2, '0')).join(' ');
}

async function main() {
  const { Gamepad } = native;

  const devs = Gamepad.enumerate();
  console.log('HID Gamepads found:\n', devs);

  const pick = devs.find(d => d.vendorId === 0x045e) || devs[0]; // Microsoft first
  if (!pick) {
    console.error('No gamepad-like HID devices found. Plug in or pair the controller.');
    process.exit(1);
  }

  console.log('Opening:', pick.product || '(unknown)', pick);
  const gp = new Gamepad({ path: pick.path });

  gp.start((report) => {
    if (report === null) {
      console.log('Device disconnected.');
      process.exit(0);
    }
    const b = Buffer.from(report);
    console.log(`[len=${b.length}] ${hex(b)}`);
  });

  process.on('SIGINT', () => {
    try { gp.stop(); gp.close(); } catch {}
    process.exit(0);
  });
}

main().catch(err => { console.error(err); process.exit(1); });
