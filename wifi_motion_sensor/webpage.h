#pragma once

// The web page (HTML/CSS/JS) lives here, in its own header file, instead of
// inside the .ino. Arduino's auto-prototype generator (ctags) scans the .ino
// file's raw text and gets confused by nested JS braces inside a raw string
// literal, which produces bogus "does not name a type" errors. Header files
// are NOT run through that scanner, so keeping the HTML here avoids the bug.

const char index_html[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>WiFi Motion Radar</title>
<script src="https://cdnjs.cloudflare.com/ajax/libs/Chart.js/4.4.1/chart.umd.min.js"></script>
<style>
  body { background:#0b0f14; color:#e6edf3; font-family:sans-serif; margin:0; padding:20px; }
  h1 { font-size:20px; font-weight:600; margin-bottom:4px; }
  #status { font-size:13px; color:#8b949e; margin-bottom:16px; }
  #status.live { color:#3fb950; }
  canvas { background:#161b22; border-radius:8px; padding:10px; }
</style>
</head>
<body>
  <h1>WiFi Motion Radar</h1>
  <div id="status">connecting...</div>
  <canvas id="chart" height="220"></canvas>

<script>
  const maxPoints = 100;
  const labels = Array(maxPoints).fill('');
  const dataPoints = Array(maxPoints).fill(0);

  const ctx = document.getElementById('chart').getContext('2d');
  const chart = new Chart(ctx, {
    type: 'line',
    data: { labels: labels, datasets: [{
      label: 'Movement intensity',
      data: dataPoints,
      borderColor: '#58a6ff',
      backgroundColor: 'rgba(88,166,255,0.15)',
      fill: true,
      tension: 0.3,
      pointRadius: 0,
      borderWidth: 2
    }]},
    options: {
      animation: false,
      responsive: true,
      scales: {
        x: { display: false },
        y: { beginAtZero: true, suggestedMax: 5, grid: { color: '#30363d' }, ticks: { color: '#8b949e' } }
      },
      plugins: { legend: { display: false } }
    }
  });

  function connect() {
    const ws = new WebSocket(`ws://${location.host}/ws`);
    ws.onopen = () => { document.getElementById('status').textContent = 'live'; document.getElementById('status').className = 'live'; };
    ws.onclose = () => { document.getElementById('status').textContent = 'disconnected — retrying...'; document.getElementById('status').className = ''; setTimeout(connect, 1000); };
    ws.onmessage = (evt) => {
      const msg = JSON.parse(evt.data);
      dataPoints.push(msg.m);
      dataPoints.shift();
      chart.update('none');
    };
  }
  connect();
</script>
</body>
</html>
)HTML";
