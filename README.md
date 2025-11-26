# node-gamepad

A tiny experimental Node.js addon written in **C + N-API** to test talking to HID gamepads.  
Not meant for production — just me playing around.

## What it does

- Enumerates HID gamepads (via `hidapi`)
- Opens a device by path or VID/PID
- Reads raw HID reports in a background thread
- Sends each report to a JS callback
- Supports writing output reports

## Example

\`\`\`js
const { Gamepad } = require('./build/Release/gamepad');

console.log(Gamepad.enumerate());

const gp = new Gamepad({ path: Gamepad.enumerate()[0].path });

gp.start((report) => {
  console.log("Report:", report); // null = disconnect
});
\`\`\`

## Why this exists

Just a small project to see if I could build something with N-API and C.  
Nothing more, nothing less.
