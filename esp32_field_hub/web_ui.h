#pragma once

#include <Arduino.h>

const char WEB_UI[] PROGMEM = R"FIELDHUB(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="theme-color" content="#101522">
<title>ESP32 Field Hub</title>
<style>
:root{color-scheme:dark;--bg:#101522;--panel:#191f2e;--line:#2c3548;--text:#eef2fa;--muted:#9ba8bd;--accent:#69e0bd;--blue:#87aaff;--danger:#ff8b91}
*{box-sizing:border-box}body{margin:0;background:radial-gradient(ellipse at 30% -20%,#263d55 0,transparent 55%),var(--bg);color:var(--text);font:15px/1.5 system-ui,-apple-system,Segoe UI,sans-serif}
main{max-width:1120px;margin:auto;padding:30px 22px 70px}.top{display:flex;justify-content:space-between;align-items:flex-start;gap:18px;margin-bottom:25px}.eyebrow{color:var(--accent);font-size:11px;letter-spacing:.18em;text-transform:uppercase;font-weight:800}.title{font-size:clamp(28px,5vw,43px);letter-spacing:-.04em;margin:4px 0}.subtitle,.muted{color:var(--muted)}.badge{border:1px solid var(--line);border-radius:999px;padding:8px 12px;color:var(--muted);font-size:12px;white-space:nowrap}.badge:before{content:"";display:inline-block;width:8px;height:8px;border-radius:50%;background:var(--accent);margin-right:8px}
.grid{display:grid;grid-template-columns:repeat(12,minmax(0,1fr));gap:15px}.card{background:linear-gradient(145deg,rgba(30,38,55,.96),rgba(22,28,41,.96));border:1px solid var(--line);border-radius:17px;padding:19px;min-width:0;box-shadow:0 12px 30px #080b1260}.span-12{grid-column:span 12}.span-8{grid-column:span 8}.span-6{grid-column:span 6}.span-4{grid-column:span 4}
h2{font-size:17px;margin:0 0 12px;letter-spacing:-.02em}h3{font-size:14px;margin:20px 0 8px}p{margin:6px 0 12px}.stats{display:grid;grid-template-columns:repeat(4,1fr);gap:10px}.stat{background:#121826;border:1px solid #293247;border-radius:12px;padding:13px}.stat strong{display:block;font-size:23px}.stat span{font-size:12px;color:var(--muted)}
.row{display:flex;align-items:center;justify-content:space-between;gap:12px;flex-wrap:wrap}.actions{display:flex;gap:8px;flex-wrap:wrap}.button{appearance:none;border:1px solid #43516b;background:#242d40;color:var(--text);border-radius:9px;padding:9px 13px;font:inherit;font-weight:650;cursor:pointer;transition:.15s}.button:hover{border-color:var(--accent);transform:translateY(-1px)}.button.primary{background:var(--accent);border-color:var(--accent);color:#08241d}.button:disabled{opacity:.5;cursor:wait;transform:none}
.mode{display:flex;gap:7px;flex-wrap:wrap}.mode button{flex:1;min-width:100px}.mode button.selected{border-color:var(--accent);color:var(--accent);background:#17312f}
.table-wrap{overflow:auto}table{width:100%;border-collapse:collapse;text-align:left}th,td{padding:10px 8px;border-bottom:1px solid #2a3244}th{font-size:11px;color:var(--muted);font-weight:700;text-transform:uppercase;letter-spacing:.08em}td{font-size:13px}.signal{font-variant-numeric:tabular-nums;white-space:nowrap}.empty{text-align:center;color:var(--muted);padding:22px}
.fields{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:12px}.field{display:flex;flex-direction:column;gap:5px}.field label{font-size:12px;color:#c5cede}.field input,.field select{width:100%;padding:10px 11px;background:#111725;border:1px solid #38445a;border-radius:8px;color:var(--text);font:inherit}.field input:focus,.field select:focus{outline:2px solid #69e0bd80;border-color:var(--accent)}.field small{color:var(--muted);font-size:11px}.wide{grid-column:1/-1}.section{border-top:1px solid var(--line);margin-top:19px;padding-top:2px}.notice{border-left:3px solid var(--blue);padding:9px 12px;background:#1a2636;color:#c5cede;border-radius:5px;font-size:12px}.toast{position:fixed;bottom:20px;left:50%;transform:translateX(-50%);background:#202b3d;border:1px solid var(--line);padding:11px 16px;border-radius:10px;display:none;box-shadow:0 10px 35px #0008;z-index:5}.toast.error{border-color:var(--danger)}.footer{margin-top:17px;color:var(--muted);font-size:12px}
@media(max-width:760px){main{padding:22px 14px 50px}.span-8,.span-6,.span-4{grid-column:span 12}.stats{grid-template-columns:repeat(2,1fr)}.top{align-items:flex-start}.badge{font-size:10px}.card{padding:16px}}
@media(max-width:420px){.fields{grid-template-columns:1fr}.wide{grid-column:auto}.top{display:block}.badge{display:inline-block;margin-top:8px}}
</style>
</head>
<body>
<main>
  <header class="top">
    <div><div class="eyebrow">ESP32-C3 · local control</div><h1 class="title">Field Hub</h1><div class="subtitle">Wi-Fi, Bluetooth LE, and your OLED assistant in one place.</div></div>
    <div id="connection" class="badge">Connecting</div>
  </header>
  <div class="grid">
    <section class="card span-12">
      <div class="row"><div><h2>At a glance</h2><p class="muted" id="networkInfo">Waiting for device status…</p></div><div class="actions"><button class="button" onclick="loadAll()">Refresh data</button></div></div>
      <div class="stats">
        <div class="stat"><strong id="wifiCount">—</strong><span>Wi-Fi networks</span></div>
        <div class="stat"><strong id="bleCount">—</strong><span>BLE advertisements</span></div>
        <div class="stat"><strong id="wifiState">—</strong><span>Wi-Fi scan</span></div>
        <div class="stat"><strong id="bleState">—</strong><span>Bluetooth scan</span></div>
      </div>
    </section>
    <section class="card span-12">
      <div class="row"><div><h2>On-device display</h2><p class="muted">Choose what appears on the 72×40 OLED.</p></div></div>
      <div class="mode">
        <button class="button" data-mode="cycle" onclick="setMode('cycle')">Auto cycle</button>
        <button class="button" data-mode="wifi" onclick="setMode('wifi')">Wi-Fi</button>
        <button class="button" data-mode="ble" onclick="setMode('ble')">Bluetooth LE</button>
        <button class="button" data-mode="assistant" onclick="setMode('assistant')">Assistant eyes</button>
      </div>
    </section>
    <section class="card span-6">
      <div class="row"><div><h2>Nearby Wi-Fi</h2><p class="muted">SSID, signal, channel, and security. No connection attempts.</p></div><button id="wifiButton" class="button primary" onclick="startScan('wifi')">Scan now</button></div>
      <div class="table-wrap"><table><thead><tr><th>Network</th><th>Signal</th><th>Channel</th><th>Security</th></tr></thead><tbody id="wifiRows"><tr><td class="empty" colspan="4">No scan results yet.</td></tr></tbody></table></div>
    </section>
    <section class="card span-6">
      <div class="row"><div><h2>Nearby Bluetooth LE</h2><p class="muted">Advertising devices and signal strength. The device does not connect to them.</p></div><button id="bleButton" class="button primary" onclick="startScan('ble')">Scan now</button></div>
      <div class="table-wrap"><table><thead><tr><th>Advertised name</th><th>Address</th><th>Signal</th></tr></thead><tbody id="bleRows"><tr><td class="empty" colspan="3">No scan results yet.</td></tr></tbody></table></div>
    </section>
    <section class="card span-12">
      <div class="row"><div><h2>Customize your Field Hub</h2><p class="muted">Settings are stored on the device and survive a restart.</p></div></div>
      <form id="settingsForm">
        <div class="section"><h3>Scanning & display</h3><div class="fields">
          <div class="field"><label for="mode">OLED mode</label><select id="mode" name="mode"><option value="cycle">Auto cycle</option><option value="wifi">Wi-Fi scanner</option><option value="ble">Bluetooth LE scanner</option><option value="assistant">Assistant eyes</option></select></div>
          <div class="field"><label for="pageInterval">Screen rotation (ms)</label><input id="pageInterval" name="pageInterval" type="number" min="1200" max="15000" required></div>
          <div class="field"><label for="wifiInterval">Wi-Fi scan interval (seconds)</label><input id="wifiInterval" name="wifiInterval" type="number" min="10" max="600" required></div>
          <div class="field"><label for="bleInterval">BLE scan interval (seconds)</label><input id="bleInterval" name="bleInterval" type="number" min="15" max="600" required></div>
          <div class="field"><label for="bleDuration">BLE scan length (seconds)</label><input id="bleDuration" name="bleDuration" type="number" min="2" max="10" required></div>
          <div class="field"><label for="contrast">OLED contrast (0–255)</label><input id="contrast" name="contrast" type="number" min="0" max="255" required></div>
        </div></div>
        <div class="section"><h3>Device access point</h3><div class="fields">
          <div class="field"><label for="apSsid">Access point name</label><input id="apSsid" name="apSsid" maxlength="31" required></div>
          <div class="notice wide">The Wi-Fi and dashboard have no password. Anyone nearby can join, view scan results, and change device settings.</div>
        </div></div>
        <div class="section"><h3>Optional router connection</h3><div class="fields">
          <div class="field"><label for="stationSsid">Router Wi-Fi name</label><input id="stationSsid" name="stationSsid" maxlength="32" autocomplete="off"></div>
          <div class="field"><label for="stationPassword">Router Wi-Fi password</label><input id="stationPassword" name="stationPassword" type="password" maxlength="63" autocomplete="new-password" placeholder="Leave blank to keep current"><small>Clear the network name to remove saved router credentials.</small></div>
          <div class="notice wide">The Field Hub access point stays available when router Wi-Fi is configured. Router-mode dashboard traffic also uses local HTTP.</div>
        </div></div>
        <div class="actions" style="margin-top:17px"><button class="button primary" type="submit">Save settings</button></div>
      </form>
    </section>
  </div>
  <div class="footer">Field Hub only scans nearby Wi-Fi beacons and Bluetooth LE advertisements. Results are kept in RAM and are not uploaded or connected to.</div>
</main>
<div id="toast" class="toast" role="status"></div>
<script>
const $=id=>document.getElementById(id);
let currentMode='cycle',polling=false;
async function api(path,options={}){const response=await fetch(path,{cache:'no-store',...options});if(!response.ok){let detail='Request failed ('+response.status+')';try{const data=await response.json();if(data.error)detail=data.error}catch{}throw new Error(detail)}return response.status===204?null:response.json()}
function toast(message,error=false){const node=$('toast');node.textContent=message;node.className='toast'+(error?' error':'');node.style.display='block';clearTimeout(toast.timer);toast.timer=setTimeout(()=>node.style.display='none',3600)}
function cell(value,cls=''){const td=document.createElement('td');td.textContent=value;td.className=cls;return td}
function renderWifi(networks){const body=$('wifiRows');body.replaceChildren();if(!networks.length){const row=document.createElement('tr');const td=cell('No networks found.','empty');td.colSpan=4;row.append(td);body.append(row);return}for(const item of networks){const row=document.createElement('tr');row.append(cell(item.ssid),cell(item.rssi+' dBm','signal'),cell('CH '+item.channel),cell(item.secured?'Secured':'Open'));body.append(row)}}
function renderBle(devices){const body=$('bleRows');body.replaceChildren();if(!devices.length){const row=document.createElement('tr');const td=cell('No devices found.','empty');td.colSpan=3;row.append(td);body.append(row);return}for(const item of devices){const row=document.createElement('tr');row.append(cell(item.name),cell(item.address),cell(item.rssi+' dBm','signal'));body.append(row)}}
async function loadSettings(){const s=await api('/api/settings');for(const key of ['mode','pageInterval','wifiInterval','bleInterval','bleDuration','contrast','apSsid','stationSsid'])$(key).value=s[key];currentMode=s.mode;syncMode()}
function syncMode(){document.querySelectorAll('[data-mode]').forEach(button=>button.classList.toggle('selected',button.dataset.mode===currentMode))}
async function loadAll(){if(polling)return;polling=true;try{const [status,wifi,ble]=await Promise.all([api('/api/status'),api('/api/wifi'),api('/api/ble')]);$('connection').textContent='Connected · '+status.apSsid;$('networkInfo').textContent='Access point '+status.apSsid+' · '+status.apIp+(status.stationConnected?' · Router '+status.stationSsid+' · '+status.stationIp:'');$('wifiCount').textContent=status.wifiCount;$('bleCount').textContent=status.bleCount;$('wifiState').textContent=status.wifiScanning?'Scanning…':status.wifiScanFailed?'Scan error':'Ready';$('bleState').textContent=status.bleScanning?'Scanning…':'Ready';$('wifiButton').disabled=status.wifiScanning||status.bleScanning;$('bleButton').disabled=status.bleScanning||status.wifiScanning;currentMode=status.mode;syncMode();renderWifi(wifi.networks);renderBle(ble.devices)}catch(error){$('connection').textContent='Disconnected';toast(error.message,true)}finally{polling=false}}
async function startScan(kind){try{await api('/api/'+kind+'/scan',{method:'POST'});toast(kind==='wifi'?'Wi-Fi scan started':'Bluetooth LE scan started');await loadAll()}catch(error){toast(error.message,true)}}
async function setMode(mode){$('mode').value=mode;$('settingsForm').requestSubmit()}
$('settingsForm').addEventListener('submit',async event=>{event.preventDefault();const data=new URLSearchParams(new FormData(event.currentTarget));try{const result=await api('/api/settings',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:data});$('stationPassword').value='';currentMode=$('mode').value;syncMode();toast(result.networkRestarted?'Settings saved. Reconnect to the access point if its name changed.':'Settings saved.')}catch(error){toast(error.message,true)}});
loadAll();loadSettings().catch(error=>toast(error.message,true));setInterval(loadAll,2500);
</script>
</body>
</html>
)FIELDHUB";
