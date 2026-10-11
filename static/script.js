// AI MonitorX -- dashboard logic.
//
// Every 2 seconds: fetch /api/stats (latest snapshot + predictions) and
// /api/history (recent readings), then update the cards, the two
// charts and the process table. The page is served by Flask itself,
// so the API is on the same address -- no hard-coded IP needed.

const REFRESH_MS = 2000;
const HISTORY_POINTS = 30;

const TREND_TEXT = {
  increasing: "↑ Increasing",
  decreasing: "↓ Decreasing",
  stable: "→ Stable",
};

// ------------------------------------------------------------------
// Cards
// ------------------------------------------------------------------
function pct(value) {
  return value.toFixed(1) + "%";
}

function updateCard(cardId, resource, label) {
  const card = document.getElementById(cardId);
  const field = (name) => card.querySelector(`[data-field="${name}"]`);

  field("current").textContent = pct(resource.current_percent);
  field("predicted").textContent = pct(resource.predicted_percent);
  field("threshold").textContent = pct(resource.threshold_percent);
  field("trend").textContent = TREND_TEXT[resource.trend] || resource.trend;

  // Status always has an icon + words, never color alone.
  const status = field("status");
  if (resource.status === "warning") {
    status.className = "status critical";
    status.innerHTML = `<span class="icon">⚠</span>High ${label} expected`;
  } else {
    status.className = "status good";
    status.innerHTML = `<span class="icon">✓</span>Normal`;
  }
}

// ------------------------------------------------------------------
// Line chart (plain SVG, no libraries)
// ------------------------------------------------------------------
const SVG_NS = "http://www.w3.org/2000/svg";

function el(tag, attrs, parent) {
  const node = document.createElementNS(SVG_NS, tag);
  for (const key in attrs) node.setAttribute(key, attrs[key]);
  if (parent) parent.appendChild(node);
  return node;
}

function drawChart(containerId, points, resource, colorVar, label) {
  const box = document.getElementById(containerId);
  const width = box.clientWidth;
  const height = box.clientHeight;
  const pad = { top: 10, right: 44, bottom: 20, left: 34 };
  const plotW = width - pad.left - pad.right;
  const plotH = height - pad.top - pad.bottom;

  box.innerHTML = "";
  const svg = el("svg", { viewBox: `0 0 ${width} ${height}` }, box);

  if (points.length < 2) {
    el("text", { x: width / 2, y: height / 2, "text-anchor": "middle", class: "empty" }, svg)
      .textContent = "Collecting readings…";
    return;
  }

  // x: one slot per reading, plus one extra slot for the prediction.
  const slots = points.length; // last index = points.length - 1, prediction at points.length
  const x = (i) => pad.left + (i / slots) * plotW;
  const y = (v) => pad.top + (1 - v / 100) * plotH;

  // Grid + y-axis labels (0-100%, fixed scale so charts are comparable).
  const grid = el("g", { class: "grid" }, svg);
  const axis = el("g", { class: "axis" }, svg);
  for (const v of [0, 25, 50, 75, 100]) {
    el("line", { x1: pad.left, x2: pad.left + plotW, y1: y(v), y2: y(v) }, grid);
    el("text", { x: pad.left - 6, y: y(v) + 4, "text-anchor": "end" }, axis).textContent = v + "%";
  }
  el("text", { x: pad.left, y: height - 4 }, axis).textContent = "older";
  el("text", { x: pad.left + plotW, y: height - 4, "text-anchor": "end" }, axis).textContent = "next";

  // Threshold line, labeled directly.
  const ty = y(resource.threshold_percent);
  el("line", { x1: pad.left, x2: pad.left + plotW, y1: ty, y2: ty, class: "threshold" }, svg);
  el("text", { x: pad.left + 4, y: ty - 4, class: "threshold-label" }, svg)
    .textContent = `Threshold ${resource.threshold_percent.toFixed(0)}%`;

  const color = `var(${colorVar})`;

  // Actual readings.
  const d = points.map((p, i) => `${i ? "L" : "M"}${x(i).toFixed(1)},${y(p.usage).toFixed(1)}`).join(" ");
  el("path", { d, class: "line", style: `stroke:${color}` }, svg);

  // Prediction: dashed segment from the latest reading to the predicted next value.
  const last = points.length - 1;
  el("path", {
    d: `M${x(last)},${y(points[last].usage)} L${x(slots)},${y(resource.predicted_percent)}`,
    class: "predict-line", style: `stroke:${color}`,
  }, svg);
  el("circle", { cx: x(last), cy: y(points[last].usage), r: 4, class: "last-dot", style: `fill:${color}` }, svg);
  el("circle", { cx: x(slots), cy: y(resource.predicted_percent), r: 4.5, class: "predict-dot", style: `stroke:${color}` }, svg);
  el("text", { x: x(slots) + 8, y: y(resource.predicted_percent) + 4, class: "end-label" }, svg)
    .textContent = resource.predicted_percent.toFixed(0) + "%";

  // Hover: crosshair + tooltip showing the nearest reading.
  const crosshair = el("line", { y1: pad.top, y2: pad.top + plotH, class: "crosshair", visibility: "hidden" }, svg);
  const hoverDot = el("circle", { r: 5, class: "last-dot", style: `fill:${color}`, visibility: "hidden" }, svg);
  const hit = el("rect", { x: pad.left, y: 0, width: plotW, height, fill: "transparent" }, svg);
  const tooltip = document.getElementById("tooltip");

  hit.addEventListener("mousemove", (event) => {
    const rect = svg.getBoundingClientRect();
    const mx = event.clientX - rect.left;
    const i = Math.round(((mx - pad.left) / plotW) * slots);
    const isPrediction = i >= slots;
    const idx = Math.max(0, Math.min(i, slots));
    const value = isPrediction ? resource.predicted_percent : points[idx].usage;
    const px = x(idx), py = y(value);

    crosshair.setAttribute("x1", px); crosshair.setAttribute("x2", px);
    crosshair.setAttribute("visibility", "visible");
    hoverDot.setAttribute("cx", px); hoverDot.setAttribute("cy", py);
    hoverDot.setAttribute("visibility", "visible");

    tooltip.innerHTML = isPrediction
      ? `<strong>${label} · predicted next</strong><br>${pct(value)}`
      : `<strong>${label} · reading #${points[idx].reading}</strong><br>${pct(value)}`;
    tooltip.hidden = false;
    tooltip.style.left = (event.clientX + 14) + "px";
    tooltip.style.top = (event.clientY - 10) + "px";
  });
  hit.addEventListener("mouseleave", () => {
    crosshair.setAttribute("visibility", "hidden");
    hoverDot.setAttribute("visibility", "hidden");
    tooltip.hidden = true;
  });
}

// ------------------------------------------------------------------
// Process table
// ------------------------------------------------------------------
function escapeHtml(text) {
  const div = document.createElement("div");
  div.textContent = text;
  return div.innerHTML;
}

function updateProcesses(procs) {
  const tbody = document.getElementById("proc-rows");
  if (!procs.length) {
    tbody.innerHTML = `<tr><td colspan="4" class="muted">No process data</td></tr>`;
    return;
  }
  const max = Math.max(...procs.map((p) => p.rss_kb)) || 1;
  tbody.innerHTML = procs.map((p) => `
    <tr>
      <td>${p.pid}</td>
      <td class="name">${escapeHtml(p.name)}</td>
      <td class="num">${(p.rss_kb / 1024).toFixed(1)} MB</td>
      <td class="bar-col"><div class="bar" style="width:${(p.rss_kb / max) * 100}%"></div></td>
    </tr>`).join("");
}

// ------------------------------------------------------------------
// Connection pill + main refresh loop
// ------------------------------------------------------------------
function setConnection(state, text) {
  document.getElementById("connection").className = "pill " + state;
  document.getElementById("connection-text").textContent = text;
}

let lastStats = null;
let lastHistory = null;

function render() {
  if (!lastStats || !lastHistory) return;
  drawChart("chart-cpu", lastHistory.cpu, lastStats.cpu, "--cpu", "CPU");
  drawChart("chart-memory", lastHistory.memory, lastStats.memory, "--mem", "Memory");
}

async function refresh() {
  try {
    const [statsRes, historyRes] = await Promise.all([
      fetch("/api/stats"),
      fetch(`/api/history?limit=${HISTORY_POINTS}`),
    ]);
    const stats = await statsRes.json();

    if (!statsRes.ok) {
      setConnection("pill-down", stats.error || "No data");
      return;
    }

    lastStats = stats;
    lastHistory = await historyRes.json();

    updateCard("card-cpu", stats.cpu, "CPU");
    updateCard("card-memory", stats.memory, "memory");
    updateProcesses(stats.top_processes);
    render();

    if (stats.monitor_running) {
      setConnection("pill-ok", "Monitor running");
    } else {
      setConnection("pill-down", "Monitor stopped");
    }
    document.getElementById("updated").textContent =
      "Updated " + new Date(stats.timestamp * 1000).toLocaleTimeString();
  } catch (err) {
    setConnection("pill-down", "Server unreachable");
  }
}

window.addEventListener("resize", render);
refresh();
setInterval(refresh, REFRESH_MS);
