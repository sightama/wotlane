// ABOUTME: Captures before-screenshots of playwotm.com at phone (390x844) and desktop (1440x900) widths.
// ABOUTME: Browser audio is muted with --mute-audio; run: NODE_PATH=$(npm root -g) node capture.cjs <outDir>.
const { chromium } = require("playwright");
const path = require("path");

const out = process.argv[2];
if (!out) {
  console.error("usage: NODE_PATH=$(npm root -g) node capture.cjs <outDir>");
  process.exit(1);
}
const VIEWPORTS = { phone: { width: 390, height: 844 }, desktop: { width: 1440, height: 900 } };

(async () => {
  const browser = await chromium.launch({ args: ["--mute-audio"] });
  for (const [name, viewport] of Object.entries(VIEWPORTS)) {
    const ctx = await browser.newContext({ viewport, deviceScaleFactor: 2, isMobile: name === "phone", hasTouch: name === "phone" });
    const page = await ctx.newPage();
    await page.goto("https://playwotm.com/", { waitUntil: "load" });
    await page.waitForTimeout(3000);
    await page.screenshot({ path: path.join(out, `home-${name}.png`) });
    await page.click("#openSettings");
    await page.waitForTimeout(800);
    await page.screenshot({ path: path.join(out, `settings-saves-${name}.png`) });
    await page.click("#tab-controls");
    await page.waitForTimeout(500);
    await page.screenshot({ path: path.join(out, `settings-controls-${name}.png`), fullPage: true });
    await page.click("#closeSettings");
    await page.click("#single");
    await page.waitForTimeout(20000);
    await page.screenshot({ path: path.join(out, `game-start-${name}.png`) });
    await ctx.close();
  }
  await browser.close();
})();
