/* Minimal Canvas line chart with threshold bands. */
export function StripChart(canvas) {
    const ctx = canvas.getContext("2d");
    const dpr = window.devicePixelRatio || 1;

    // Handle high-DPI + responsive width.
    function resize() {
      const cssW = canvas.clientWidth || 820;
      const cssH = 300;
      canvas.width = cssW * dpr;
      canvas.height = cssH * dpr;
      ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    }
    resize();
    window.addEventListener("resize", resize);

    const PAD = { l: 44, r: 12, t: 14, b: 22 };

    // pull theme colors from CSS so the chart matches the stylesheet palette
    const css = getComputedStyle(canvas);
    const color = (name, fallback) => css.getPropertyValue(name).trim() || fallback;
    const cGrid = color("--line", "#232f47");
    const cMuted = color("--muted", "#7e8aa4");
    const cTrace = color("--accent", "#4d8dff");
    const cAmber = color("--amber", "#f0b23d");
    const cRed = color("--red", "#ff6f6f");

    function draw(data, opts) {
      const w = canvas.clientWidth;
      const h = 300;
      ctx.clearRect(0, 0, w, h);
      if (!data.length) return;

      const values = data.map((d) => d.value);
      let lo = Math.min(...values, opts.loThresh);
      let hi = Math.max(...values, opts.hiThresh);
      const span = (hi - lo) || 1;
      lo -= span * 0.1; hi += span * 0.1;

      const plotW = w - PAD.l - PAD.r;
      const plotH = h - PAD.t - PAD.b;
      const x = (i) => PAD.l + (i / Math.max(1, data.length - 1)) * plotW;
      const y = (v) => PAD.t + (1 - (v - lo) / (hi - lo)) * plotH;

      // grid + y labels
      ctx.strokeStyle = cGrid;
      ctx.fillStyle = cMuted;
      ctx.font = "11px 'Spline Sans Mono', monospace";
      ctx.lineWidth = 1;
      for (let g = 0; g <= 4; g++) {
        const val = lo + (g / 4) * (hi - lo);
        const gy = y(val);
        ctx.beginPath(); ctx.moveTo(PAD.l, gy); ctx.lineTo(w - PAD.r, gy); ctx.stroke();
        ctx.fillText(val.toFixed(1), 6, gy + 3);
      }

      // threshold bands (hex + "1a" = ~10% alpha)
      drawBand(y(hi), y(opts.hiThresh), cRed + "1a");   // above high
      drawBand(y(opts.loThresh), y(lo), cRed + "1a");   // below low
      drawThreshLine(y(opts.hiThresh), cRed);
      drawThreshLine(y(opts.loThresh), cAmber);

      function drawBand(y1, y2, color) {
        ctx.fillStyle = color;
        ctx.fillRect(PAD.l, Math.min(y1, y2), plotW, Math.abs(y2 - y1));
      }
      function drawThreshLine(gy, color) {
        ctx.strokeStyle = color; ctx.setLineDash([4, 4]);
        ctx.beginPath(); ctx.moveTo(PAD.l, gy); ctx.lineTo(w - PAD.r, gy); ctx.stroke();
        ctx.setLineDash([]);
      }

      // line
      ctx.strokeStyle = cTrace;
      ctx.lineWidth = 2;
      ctx.beginPath();
      data.forEach((d, i) => {
        const px = x(i), py = y(d.value);
        i === 0 ? ctx.moveTo(px, py) : ctx.lineTo(px, py);
      });
      ctx.stroke();

      // area fill
      ctx.lineTo(x(data.length - 1), PAD.t + plotH);
      ctx.lineTo(x(0), PAD.t + plotH);
      ctx.closePath();
      ctx.fillStyle = cTrace + "14";   // ~8% alpha area fill
      ctx.fill();

      // last point marker
      const last = data[data.length - 1];
      ctx.fillStyle = cTrace;
      ctx.beginPath(); ctx.arc(x(data.length - 1), y(last.value), 3.5, 0, Math.PI * 2); ctx.fill();
    }

    return { draw };
}

if (typeof window !== "undefined") window.StripChart = StripChart;
