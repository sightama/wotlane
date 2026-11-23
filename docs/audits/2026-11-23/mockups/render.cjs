// ABOUTME: Renders the HTML mockups in this folder to PNG (all are phone width, 390px).
// ABOUTME: Run with NODE_PATH=$(npm root -g) node render.cjs from this directory.
const { chromium } = require("playwright");
(async () => {
  const b = await chromium.launch();
  for (const f of ["m1-home", "m2-lobby-rules", "m3-status-bar"]) {
    const p = await b.newPage({ viewport: { width: 390, height: 100 }, deviceScaleFactor: 2 });
    await p.goto("file://" + process.cwd() + "/" + f + ".html");
    await p.screenshot({ path: f + ".png", fullPage: true });
  }
  await b.close();
})();
