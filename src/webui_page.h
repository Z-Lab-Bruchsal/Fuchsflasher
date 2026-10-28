#pragma once

// Single-Page-Oberfläche, wird unter "/" ausgeliefert.
static const char INDEX_HTML[] = R"HTML(<!doctype html>
<html lang="de">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>fuchsflasher</title>
<style>
:root{--bg:#f4f4f2;--card:#fff;--fg:#1d1d1b;--mute:#6b6b66;--line:#ddd;--accent:#d9661f;--ok:#2e8b4a;--err:#c0392b;--warn:#b7791f}
@media (prefers-color-scheme:dark){:root{--bg:#161615;--card:#22221f;--fg:#ecece8;--mute:#9a9a93;--line:#3a3a36;--accent:#f08a3e;--ok:#4cbb6c;--err:#e5604f;--warn:#e0a84a}}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.45 system-ui,sans-serif}
main{max-width:720px;margin:0 auto;padding:16px}
h1{font-size:22px;margin:4px 0 16px}h1 span{color:var(--accent)}
h2{font-size:16px;margin:0 0 12px}
section{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:16px;margin-bottom:14px}
.row{display:flex;flex-wrap:wrap;gap:8px;align-items:center;margin:8px 0}
.grid{display:grid;grid-template-columns:auto 1fr;gap:4px 14px}
.grid div:nth-child(odd){color:var(--mute)}
label{display:flex;gap:6px;align-items:center}
input[type=text],input[type=password]{padding:7px 9px;border:1px solid var(--line);border-radius:6px;background:var(--bg);color:var(--fg);font:inherit}
input.hex{width:4.2em;font-family:ui-monospace,monospace;text-transform:uppercase}
button{padding:8px 14px;border:1px solid var(--line);border-radius:6px;background:var(--bg);color:var(--fg);font:inherit;cursor:pointer}
button.primary{background:var(--accent);border-color:var(--accent);color:#fff;font-weight:600}
button.big{font-size:18px;padding:12px 22px}
button:disabled{opacity:.5;cursor:default}
.badge{display:inline-block;padding:2px 10px;border-radius:99px;font-weight:600;color:#fff;background:var(--mute)}
.badge.ok{background:var(--ok)}.badge.error{background:var(--err)}.badge.busy{background:var(--warn)}
.mute{color:var(--mute);font-size:13px}
.msg{padding:9px 12px;border-radius:6px;margin:8px 0;display:none}
.msg.show{display:block}.msg.err{background:color-mix(in srgb,var(--err) 18%,transparent)}
.msg.ok{background:color-mix(in srgb,var(--ok) 18%,transparent)}.msg.warn{background:color-mix(in srgb,var(--warn) 22%,transparent)}
table{width:100%;border-collapse:collapse}td{padding:6px 4px;border-top:1px solid var(--line)}
td:last-child{text-align:right;white-space:nowrap}
.mono{font-family:ui-monospace,monospace}
[hidden]{display:none!important}
</style>
</head>
<body>
<main>
<h1><span>fuchs</span>flasher · ATtiny85</h1>

<div id="reboot" class="msg warn">Einige Änderungen werden erst nach einem Neustart wirksam.
  <button id="btnReboot">Jetzt neu starten</button></div>
<div id="msg" class="msg"></div>

<section>
  <h2>Status</h2>
  <div class="row"><span id="state" class="badge">–</span><span id="lastMsg"></span></div>
  <div class="grid">
    <div>Image</div><div id="imgName">–</div>
    <div>Signatur</div><div id="sig" class="mono">–</div>
    <div>Fuses (gelesen)</div><div id="fusesRead" class="mono">–</div>
    <div>Dauer</div><div id="duration">–</div>
    <div>Zähler</div><div><span id="cntOk">0</span> OK · <span id="cntFail">0</span> Fehler</div>
  </div>
  <div class="row">
    <button id="btnFlash" class="primary big">Jetzt flashen</button>
    <button id="btnCounter">Zähler zurücksetzen</button>
  </div>
</section>

<section>
  <h2>Firmware-Quelle</h2>
  <div class="row">
    <label><input type="radio" name="source" value="embedded"> eingebettet (fest einkompiliert)</label>
    <label><input type="radio" name="source" value="sd" id="srcSd"> SD-Karte</label>
  </div>
  <p id="sdHint" class="mute"></p>
  <div id="sdPanel" hidden>
    <table id="files"></table>
    <div class="row">
      <input type="file" id="upFile" accept=".hex">
      <button id="btnUpload">Hochladen</button>
    </div>
    <p class="mute">Nur Intel-HEX (*.hex), max. 8 KB Flash. Ungültige Dateien werden abgelehnt.
      <a href="/api/log">Flash-Log herunterladen</a></p>
  </div>
</section>

<section>
  <h2>Programmier-Optionen</h2>
  <label><input type="checkbox" id="autoFlash"> Automatik-Modus: neuen Chip erkennen und automatisch flashen</label>
  <p class="mute">Der Sockel wird ca. 3× pro Sekunde abgefragt. Nach dem Flashen wartet der fuchsflasher,
    bis der Chip entnommen wurde. Der Taster funktioniert weiterhin.</p>
  <label><input type="checkbox" id="verify"> Flash nach dem Schreiben prüfen (Verify)</label>
  <div class="row">
    <label><input type="checkbox" id="writeFuses"> Fuses schreiben</label>
    L <input class="hex" id="lfuse" maxlength="2">
    H <input class="hex" id="hfuse" maxlength="2">
    E <input class="hex" id="efuse" maxlength="2">
  </div>
  <p class="mute">Werkszustand: L=62 H=DF E=FF · 8 MHz intern: L=E2.
    RSTDISBL (H Bit 7) und SPIEN (H Bit 5) sind gesperrt, sonst ist der Chip per ISP nicht mehr programmierbar.</p>
  <div class="row">
    <label><input type="checkbox" id="writeLock"> Lock-Bits schreiben</label>
    <input class="hex" id="lock" maxlength="2">
    <span class="mute">FC = Auslesen gesperrt</span>
  </div>
</section>

<section>
  <h2>WLAN &amp; Hardware</h2>
  <label><input type="checkbox" id="sdEnabled"> SD-Kartenleser angeschlossen</label>
  <label><input type="checkbox" id="wifiOn"> WLAN-Access-Point beim Start aktivieren</label>
  <p class="mute">Ist das WLAN aus: Taster beim Einschalten gedrückt halten, dann startet es trotzdem.</p>
  <div class="row"><label>SSID <input type="text" id="apSsid" maxlength="32"></label></div>
  <div class="row"><label>Passwort <input type="password" id="apPass" maxlength="63"></label>
    <span class="mute">leer = offenes WLAN</span></div>
</section>

<div class="row"><button id="btnSave" class="primary">Einstellungen speichern</button></div>
</main>

<script>
const $ = s => document.querySelector(s);
const hex = v => v.toString(16).toUpperCase().padStart(2, '0');
let sdActive = false;

async function api(method, url, body) {
  const opt = {method};
  if (body instanceof FormData) opt.body = body;
  else if (body) { opt.body = JSON.stringify(body); opt.headers = {'Content-Type': 'application/json'}; }
  const r = await fetch(url, opt);
  let j = {};
  try { j = await r.json(); } catch (e) {}
  if (!r.ok) throw new Error(j.error || j.message || ('HTTP ' + r.status));
  return j;
}

function show(text, kind) {
  const m = $('#msg');
  m.textContent = text;
  m.className = 'msg show ' + kind;
}

function renderStatus(s) {
  const st = $('#state');
  const names = {idle: s.auto === 'waitChip' ? 'wartet auf Chip' : 'bereit',
                 busy: 'läuft …', ok: 'OK', error: 'Fehler'};
  st.textContent = names[s.state] || s.state;
  st.className = 'badge ' + s.state;
  const l = s.last;
  const parts = [];
  if (s.state === 'idle' && l) parts.push('letzter Vorgang: ' + l.message);
  else if (l && l.error) parts.push('Fehler ' + l.error + ': ' + l.message);
  if (s.auto === 'waitRemoval') parts.push('Chip entnehmen');
  else if (s.auto === 'waitChip') parts.push('Automatik aktiv');
  $('#lastMsg').textContent = parts.join(' · ');
  $('#sig').textContent = l ? l.signature : '–';
  $('#fusesRead').textContent = l && l.lfuse !== undefined
    ? 'L=' + hex(l.lfuse) + ' H=' + hex(l.hfuse) + ' E=' + hex(l.efuse) : '–';
  $('#duration').textContent = l ? (l.durationMs / 1000).toFixed(1) + ' s' : '–';
  $('#cntOk').textContent = s.counterOk;
  $('#cntFail').textContent = s.counterFail;
  $('#imgName').textContent = s.image.name + (s.image.valid
    ? ' (' + s.image.size + ' Bytes)' : ' – ungültig: ' + s.image.message);
  $('#reboot').classList.toggle('show', s.rebootPending);

  sdActive = s.sdEnabled && s.sdMounted;
  $('#srcSd').disabled = !sdActive;
  $('#sdPanel').hidden = !sdActive;
  $('#sdHint').textContent = !s.sdEnabled ? 'SD-Kartenleser ist unter „WLAN & Hardware“ deaktiviert.'
    : !s.sdMounted ? 'SD-Kartenleser aktiviert, aber keine Karte gefunden (Karte prüfen, Neustart).' : '';
}

function renderConfig(c) {
  document.querySelectorAll('input[name=source]').forEach(r => r.checked = r.value === c.source);
  for (const k of ['autoFlash', 'verify', 'writeFuses', 'writeLock', 'sdEnabled', 'wifiOn']) $('#' + k).checked = c[k];
  for (const k of ['lfuse', 'hfuse', 'efuse', 'lock']) $('#' + k).value = hex(c[k]);
  $('#apSsid').value = c.apSsid;
  $('#apPass').value = c.apPass;
}

async function loadFiles() {
  if (!sdActive) return;
  const d = await api('GET', '/api/files');
  const t = $('#files');
  t.innerHTML = '';
  if (!d.files.length) t.innerHTML = '<tr><td class="mute">Keine HEX-Dateien auf der Karte.</td></tr>';
  for (const f of d.files) {
    const sel = '/' + f.name === d.selected;
    const tr = document.createElement('tr');
    tr.innerHTML = '<td class="mono"></td><td class="mute"></td><td></td>';
    tr.cells[0].textContent = f.name + (sel ? '  ✓' : '');
    tr.cells[1].textContent = (f.size / 1024).toFixed(1) + ' KB';
    const bSel = document.createElement('button');
    bSel.textContent = 'Auswählen';
    bSel.onclick = () => act(async () => {
      renderStatus(await api('POST', '/api/select?name=' + encodeURIComponent(f.name)));
      await refreshAll();
      show(f.name + ' ausgewählt.', 'ok');
    });
    const bDel = document.createElement('button');
    bDel.textContent = 'Löschen';
    bDel.onclick = () => confirm(f.name + ' löschen?') && act(async () => {
      await api('DELETE', '/api/files?name=' + encodeURIComponent(f.name));
      await refreshAll();
    });
    tr.cells[2].append(bSel, ' ', bDel);
    t.append(tr);
  }
}

function readHex(id) {
  const v = $('#' + id).value.trim();
  if (!/^[0-9a-fA-F]{1,2}$/.test(v)) throw new Error(id + ': zweistelliger Hex-Wert erwartet');
  return parseInt(v, 16);
}

function collectConfig() {
  const c = {
    source: (document.querySelector('input[name=source]:checked') || {}).value || 'embedded',
    apSsid: $('#apSsid').value, apPass: $('#apPass').value,
  };
  for (const k of ['autoFlash', 'verify', 'writeFuses', 'writeLock', 'sdEnabled', 'wifiOn']) c[k] = $('#' + k).checked;
  for (const k of ['lfuse', 'hfuse', 'efuse', 'lock']) c[k] = readHex(k);
  if (!(c.hfuse & 0x80)) throw new Error('hfuse: RSTDISBL darf nicht programmiert werden');
  if (c.hfuse & 0x20) throw new Error('hfuse: SPIEN muss programmiert bleiben');
  if (c.writeLock && !confirm('Lock-Bits schreiben? Der Flash lässt sich danach nicht mehr auslesen.')) {
    throw new Error('abgebrochen');
  }
  return c;
}

async function act(fn) {
  try { await fn(); } catch (e) { show(e.message, 'err'); }
}

async function refreshStatus() {
  renderStatus(await api('GET', '/api/status'));
}

async function refreshAll() {
  renderConfig(await api('GET', '/api/config'));
  await refreshStatus();
  await loadFiles();
}

$('#btnFlash').onclick = () => act(async () => {
  $('#btnFlash').disabled = true;
  renderStatus(Object.assign(await api('GET', '/api/status'), {state: 'busy', last: null}));
  try {
    await api('POST', '/api/flash');
    show('Erfolgreich geflasht.', 'ok');
  } finally {
    $('#btnFlash').disabled = false;
    await refreshStatus();
  }
});

$('#btnCounter').onclick = () => confirm('Zähler zurücksetzen?') && act(async () => {
  await api('POST', '/api/counter/reset');
  await refreshStatus();
});

$('#btnSave').onclick = () => act(async () => {
  const r = await api('POST', '/api/config', collectConfig());
  await refreshAll();
  if (r.warnings.length) show('Gespeichert. Hinweis: ' + r.warnings.join(' · '), 'warn');
  else show('Gespeichert.', 'ok');
});

$('#btnUpload').onclick = () => act(async () => {
  const f = $('#upFile').files[0];
  if (!f) throw new Error('Bitte eine HEX-Datei wählen');
  const fd = new FormData();
  fd.append('file', f, f.name);
  const r = await api('POST', '/api/upload', fd);
  $('#upFile').value = '';
  await loadFiles();
  show(r.name.slice(1) + ' hochgeladen.', 'ok');
});

$('#btnReboot').onclick = () => act(async () => {
  await api('POST', '/api/reboot');
  show('Neustart … Seite in ein paar Sekunden neu laden (ggf. WLAN neu verbinden).', 'warn');
});

act(refreshAll);
setInterval(() => act(refreshStatus), 3000);
</script>
</body>
</html>
)HTML";
