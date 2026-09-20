/* Minimal Canvas line chart with threshold bands. No libraries. */
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
      ctx.strokeStyle = "#26332e";
      ctx.fillStyle = "#8fa39a";
      ctx.font = "11px 'JetBrains Mono', monospace";
      ctx.lineWidth = 1;
      for (let g = 0; g <= 4; g++) {
        const val = lo + (g / 4) * (hi - lo);
        const gy = y(val);
        ctx.beginPath(); ctx.moveTo(PAD.l, gy); ctx.lineTo(w - PAD.r, gy); ctx.stroke();
        ctx.fillText(val.toFixed(1), 6, gy + 3);
      }

      // threshold bands
      drawBand(y(hi), y(opts.hiThresh), "rgba(226,96,96,0.10)");   // above high
      drawBand(y(opts.loThresh), y(lo), "rgba(226,96,96,0.10)");   // below low
      drawThreshLine(y(opts.hiThresh), "#e26060");
      drawThreshLine(y(opts.loThresh), "#e0a53d");

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
      ctx.strokeStyle = "#4ecb8f";
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
      ctx.fillStyle = "rgba(78,203,143,0.08)";
      ctx.fill();

      // last point marker
      const last = data[data.length - 1];
      ctx.fillStyle = "#4ecb8f";
      ctx.beginPath(); ctx.arc(x(data.length - 1), y(last.value), 3.5, 0, Math.PI * 2); ctx.fill();
    }

    return { draw };
}

if (typeof window !== "undefined") window.StripChart = StripChart;
