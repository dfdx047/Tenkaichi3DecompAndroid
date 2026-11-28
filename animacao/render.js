// Renders intro.html frame by frame and encodes to MP4 with ffmpeg.
// Usage: node render.js [fps] [only-frames-at-seconds,comma,separated]
const { chromium } = require('playwright');
const { spawn } = require('child_process');
const path = require('path');

(async () => {
  const fps = Number(process.argv[2] || 60);
  const stills = process.argv[3];
  const browser = await chromium.launch({ args: ['--allow-file-access-from-files'] });
  const page = await browser.newPage({ viewport: { width: 1920, height: 1080 } });
  await page.goto('file://' + path.join(__dirname, 'intro.html') + '?render');
  await page.evaluate(() => Promise.all([window.READY, document.fonts.load('italic 800 132px Inter')]).then(() => document.fonts.ready));

  if (stills) {
    for (const s of stills.split(',')) {
      const buf = await page.evaluate(t => { draw(t); return document.getElementById('c').toDataURL('image/png'); }, Number(s));
      require('fs').writeFileSync(path.join(process.argv[4] || __dirname, `still_${s}.png`), Buffer.from(buf.split(',')[1], 'base64'));
    }
    await browser.close();
    return;
  }

  const duration = await page.evaluate(() => DURATION);
  const out = path.join(__dirname, 'dragon_rage_intro.mp4');
  const ff = spawn('ffmpeg', ['-y', '-f', 'image2pipe', '-framerate', String(fps), '-c:v', 'png', '-i', '-',
    '-vf', 'scale=1920:1080:flags=lanczos', '-c:v', 'libx264', '-tune', 'animation', '-pix_fmt', 'yuv420p', '-crf', '12', '-preset', 'slow', '-movflags', '+faststart', out],
    { stdio: ['pipe', 'ignore', 'inherit'] });
  const frames = Math.round(duration * fps);
  for (let i = 0; i <= frames; i++) {
    const buf = await page.evaluate(t => { draw(t); return document.getElementById('c').toDataURL('image/png'); }, i / fps);
    ff.stdin.write(Buffer.from(buf.split(',')[1], 'base64'));
  }
  ff.stdin.end();
  await new Promise(r => ff.on('close', r));
  await browser.close();
  console.log('wrote', out, frames + 1, 'frames');
})();
