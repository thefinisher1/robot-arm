/**
 * @file web_ui.h
 * @brief Self-contained HTML, CSS, and JavaScript single-page web interface
 *        stored in PROGMEM for the 4-DOF robot arm.
 */

#pragma once

#include <Arduino.h>

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>4-DOF Robot Arm Controller</title>
    <style>
        :root {
            --bg-color: #0d1117;
            --card-bg: rgba(22, 27, 34, 0.85);
            --border-color: rgba(255, 255, 255, 0.1);
            --accent-blue: #58a6ff;
            --accent-green: #238636;
            --accent-red: #da3633;
            --accent-amber: #d29922;
            --text-main: #f0f6fc;
            --text-sub: #8b949e;
        }

        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
            -webkit-tap-highlight-color: transparent;
        }

        body {
            background: linear-gradient(135deg, #0b0e14 0%, #161b22 100%);
            color: var(--text-main);
            min-height: 100vh;
            padding: 16px;
            display: flex;
            flex-direction: column;
            align-items: center;
        }

        .container {
            width: 100%;
            max-width: 580px;
            display: flex;
            flex-direction: column;
            gap: 16px;
        }

        .card {
            background: var(--card-bg);
            border: 1px solid var(--border-color);
            border-radius: 14px;
            padding: 20px;
            box-shadow: 0 8px 24px rgba(0,0,0,0.4);
            backdrop-filter: blur(10px);
        }

        header {
            display: flex;
            justify-content: space-between;
            align-items: center;
        }

        h1 {
            font-size: 1.3rem;
            font-weight: 600;
            letter-spacing: -0.5px;
        }

        .status-badge {
            display: inline-flex;
            align-items: center;
            gap: 6px;
            padding: 4px 10px;
            border-radius: 20px;
            font-size: 0.8rem;
            font-weight: 500;
            background: rgba(255,255,255,0.06);
            border: 1px solid var(--border-color);
        }

        .status-dot {
            width: 8px;
            height: 8px;
            border-radius: 50%;
            background: var(--accent-red);
            transition: background 0.3s;
        }

        .status-dot.online {
            background: #3fb950;
            box-shadow: 0 0 8px #3fb950;
        }

        .slider-group {
            display: flex;
            flex-direction: column;
            gap: 16px;
            margin-top: 10px;
        }

        .joint-row {
            display: flex;
            flex-direction: column;
            gap: 6px;
        }

        .joint-header {
            display: flex;
            justify-content: space-between;
            font-size: 0.9rem;
            color: var(--text-sub);
        }

        .joint-title {
            color: var(--text-main);
            font-weight: 500;
        }

        .joint-value {
            font-family: monospace;
            font-weight: 600;
            color: var(--accent-blue);
        }

        input[type="range"] {
            -webkit-appearance: none;
            width: 100%;
            height: 8px;
            border-radius: 4px;
            background: #30363d;
            outline: none;
            cursor: pointer;
        }

        input[type="range"]::-webkit-slider-thumb {
            -webkit-appearance: none;
            width: 22px;
            height: 22px;
            border-radius: 50%;
            background: var(--accent-blue);
            cursor: pointer;
            box-shadow: 0 0 10px rgba(88, 166, 255, 0.5);
            transition: transform 0.1s;
        }

        input[type="range"]:active::-webkit-slider-thumb {
            transform: scale(1.15);
        }

        .btn-row {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 12px;
            margin-top: 6px;
        }

        button {
            border: none;
            border-radius: 8px;
            padding: 12px 16px;
            font-size: 0.95rem;
            font-weight: 600;
            cursor: pointer;
            transition: opacity 0.2s, transform 0.1s;
            display: flex;
            justify-content: center;
            align-items: center;
            gap: 8px;
        }

        button:active {
            transform: scale(0.98);
        }

        .btn-home {
            background: #21262d;
            color: var(--text-main);
            border: 1px solid var(--border-color);
        }

        .btn-home:hover {
            background: #30363d;
        }

        .btn-sweep {
            background: #1f6feb;
            color: #ffffff;
            border: 1px solid rgba(255,255,255,0.15);
        }

        .btn-sweep:hover {
            background: #388bfd;
        }

        .btn-sweep.active {
            background: #8957e5;
            box-shadow: 0 0 10px rgba(137, 87, 229, 0.5);
        }

        .btn-power {
            background: var(--accent-green);
            color: #ffffff;
        }

        .btn-power.disabled-state {
            background: var(--accent-red);
        }

        .btn-ota {
            background: #238636;
            color: #ffffff;
            border: 1px solid rgba(255, 255, 255, 0.15);
            width: 100%;
            margin-top: 10px;
        }

        .btn-ota:hover {
            background: #2ea043;
        }

        .btn-ota.active {
            background: #1f6feb;
            box-shadow: 0 0 12px rgba(31, 111, 235, 0.6);
        }

        .telemetry-row {
            display: flex;
            justify-content: space-between;
            font-size: 0.8rem;
            color: var(--text-sub);
            padding-top: 8px;
            border-top: 1px solid var(--border-color);
            margin-top: 12px;
        }

        .telemetry-val {
            font-family: monospace;
            color: var(--text-main);
        }
    </style>
</head>
<body>
    <div class="container">
        <!-- Header & Status -->
        <div class="card">
            <header>
                <div>
                    <h1>4-DOF Robotic Arm</h1>
                    <span style="font-size: 0.8rem; color: var(--text-sub);">ESP32 FreeRTOS Controller</span>
                </div>
                <div class="status-badge" id="statusBadge">
                    <span class="status-dot" id="statusDot"></span>
                    <span id="statusText">Disconnected</span>
                </div>
            </header>
        </div>

        <!-- Joint Sliders Card -->
        <div class="card">
            <h2 style="font-size: 1rem; color: var(--text-sub); margin-bottom: 8px;">Joint Positions (Degrees)</h2>
            <div class="slider-group">
                <div class="joint-row">
                    <div class="joint-header">
                        <span class="joint-title">Base (CH0)</span>
                        <span class="joint-value"><span id="val0">90</span>°</span>
                    </div>
                    <input type="range" id="joint0" min="0" max="180" value="90" oninput="onJointInput(0, this.value)">
                </div>

                <div class="joint-row">
                    <div class="joint-header">
                        <span class="joint-title">Shoulder (CH1)</span>
                        <span class="joint-value"><span id="val1">45</span>°</span>
                    </div>
                    <input type="range" id="joint1" min="15" max="165" value="45" oninput="onJointInput(1, this.value)">
                </div>

                <div class="joint-row">
                    <div class="joint-header">
                        <span class="joint-title">Elbow (CH2)</span>
                        <span class="joint-value"><span id="val2">120</span>°</span>
                    </div>
                    <input type="range" id="joint2" min="10" max="170" value="120" oninput="onJointInput(2, this.value)">
                </div>

                <div class="joint-row">
                    <div class="joint-header">
                        <span class="joint-title">Wrist Pitch (CH3)</span>
                        <span class="joint-value"><span id="val3">90</span>°</span>
                    </div>
                    <input type="range" id="joint3" min="0" max="180" value="90" oninput="onJointInput(3, this.value)">
                </div>
            </div>
        </div>

        <!-- Speed & Control Buttons -->
        <div class="card">
            <div class="joint-row" style="margin-bottom: 16px;">
                <div class="joint-header">
                    <span class="joint-title">Trajectory Speed</span>
                    <span class="joint-value"><span id="valSpeed">50</span>%</span>
                </div>
                <input type="range" id="speedSlider" min="5" max="100" value="50" oninput="onSpeedInput(this.value)">
            </div>

            <div class="btn-row">
                <button class="btn-home" onclick="sendHomeCommand()">🏠 Home Pose</button>
                <button class="btn-sweep" id="btnSweep" onclick="toggleSweepDemo()">🔄 Sweep Demo</button>
                <button class="btn-power" id="btnPower" onclick="toggleServoPower()">⚡ Servos: ON</button>
            </div>

            <div style="margin-top: 10px;">
                <button class="btn-ota" id="btnOta" onclick="sendEnableOtaCommand()">📡 Enable Wireless OTA</button>
            </div>

            <div class="telemetry-row">
                <span>Actual Joint Telemetry:</span>
                <span class="telemetry-val" id="telemetryAngles">-- , -- , -- , --</span>
            </div>
        </div>

        <!-- OTA Wireless Flashing & Firmware Update Card -->
        <div class="card" id="otaCard" style="display: none; border-color: rgba(88, 166, 255, 0.4); background: rgba(13, 17, 23, 0.95); margin-top: 16px;">
            <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 12px;">
                <h2 style="font-size: 1rem; color: var(--accent-blue);">📡 Wireless OTA Firmware Flashing</h2>
                <span style="background: rgba(35, 134, 54, 0.2); color: #3fb950; border: 1px solid #238636; font-size: 0.75rem; padding: 2px 8px; border-radius: 12px; font-weight: 600;">ACTIVE</span>
            </div>

            <div style="background: rgba(22, 27, 34, 0.9); padding: 12px; border-radius: 8px; border: 1px solid var(--border-color); margin-bottom: 12px; font-size: 0.85rem;">
                <div style="margin-bottom: 6px;">
                    <span style="color: var(--text-sub);">ESP32 OTA IP Address:</span>
                    <strong style="color: #58a6ff; font-family: monospace; font-size: 1.05rem; margin-left: 6px;" id="otaIpDisplay">--</strong>
                </div>
                <div style="margin-bottom: 8px;">
                    <span style="color: var(--text-sub);">OTA Network Port:</span>
                    <span style="color: var(--text-main); font-family: monospace;">3232</span>
                    <span style="color: var(--text-sub); margin-left: 14px;">mDNS Hostname:</span>
                    <span style="color: var(--text-main); font-family: monospace;">esp32-robotarm.local</span>
                </div>
                <div style="color: var(--text-sub); margin-bottom: 4px;">PlatformIO Flash Command:</div>
                <div style="display: flex; gap: 8px; align-items: center;">
                    <input type="text" id="pioCmdInput" readonly style="flex: 1; background: #0b0e14; border: 1px solid var(--border-color); color: #7ee787; font-family: monospace; font-size: 0.8rem; padding: 8px 10px; border-radius: 4px;" value="pio run -t upload --upload-port ...">
                    <button class="btn-home" style="padding: 8px 14px; font-size: 0.8rem;" onclick="copyPioCommand()">📋 Copy</button>
                </div>
            </div>

            <!-- Direct Browser Firmware Upload -->
            <div style="background: rgba(22, 27, 34, 0.9); padding: 12px; border-radius: 8px; border: 1px solid var(--border-color); font-size: 0.85rem;">
                <div style="font-weight: 600; margin-bottom: 6px; color: var(--text-main);">Direct Web Browser Upload (.bin):</div>
                <div style="display: flex; gap: 8px; align-items: center; margin-bottom: 8px;">
                    <input type="file" id="binFileInput" accept=".bin" style="font-size: 0.8rem; color: var(--text-sub); flex: 1;">
                    <button class="btn-sweep" id="btnUploadBin" style="padding: 7px 16px; font-size: 0.8rem;" onclick="uploadFirmwareBin()">⬆ Flash</button>
                </div>
                <div id="uploadProgressContainer" style="display: none; margin-top: 8px;">
                    <div style="display: flex; justify-content: space-between; font-size: 0.75rem; color: var(--text-sub); margin-bottom: 4px;">
                        <span id="uploadStatusText">Uploading firmware...</span>
                        <span id="uploadPercent">0%</span>
                    </div>
                    <div style="width: 100%; height: 8px; background: #21262d; border-radius: 4px; overflow: hidden;">
                        <div id="uploadProgressBar" style="width: 0%; height: 100%; background: #238636; transition: width 0.2s;"></div>
                    </div>
                </div>
            </div>
        </div>
    </div>

    <script>
        // Number of joints in the configuration
        const NUM_JOINTS = 4;
        let ws = null;
        let isPowerEnabled = true;

        // Current UI state tracking
        let jointAngles = [90, 45, 120, 90];
        let currentSpeed = 50;

        // Rate-limiting / throttling variables (~20 Hz = 50ms)
        let lastSendTime = 0;
        const SEND_INTERVAL_MS = 50;
        let pendingSendTimeout = null;

        function connectWebSocket() {
            const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
            const wsUrl = `${protocol}//${window.location.host}/ws`;

            ws = new WebSocket(wsUrl);

            ws.onopen = () => {
                document.getElementById('statusDot').className = 'status-dot online';
                document.getElementById('statusText').innerText = 'Connected';
            };

            ws.onclose = () => {
                document.getElementById('statusDot').className = 'status-dot';
                document.getElementById('statusText').innerText = 'Reconnecting...';
                // Attempt automatic reconnection every 2 seconds
                setTimeout(connectWebSocket, 2000);
            };

            ws.onerror = () => {
                ws.close();
            };

            ws.onmessage = (event) => {
                try {
                    const data = JSON.parse(event.data);
                    if (data.type === 'state') {
                        handleStateUpdate(data);
                    } else if (data.type === 'ota_status') {
                        handleOtaStatus(data);
                    }
                } catch (e) {
                    console.error('Invalid telemetry JSON:', e);
                }
            };
        }

        function handleStateUpdate(data) {
            if (Array.isArray(data.joints)) {
                const formatted = data.joints.map(v => Number(v).toFixed(1) + '°').join(' | ');
                document.getElementById('telemetryAngles').innerText = formatted;
            }
            if (typeof data.enabled === 'boolean') {
                updatePowerUI(data.enabled);
            }
        }

        function onJointInput(index, value) {
            jointAngles[index] = parseFloat(value);
            document.getElementById('val' + index).innerText = value;
            scheduleMoveTransmission();
        }

        function onSpeedInput(value) {
            currentSpeed = parseFloat(value);
            document.getElementById('valSpeed').innerText = value;
            scheduleMoveTransmission();
        }

        // Throttle transmission to 20 messages per second (every 50ms)
        function scheduleMoveTransmission() {
            const now = performance.now();
            const elapsed = now - lastSendTime;

            if (elapsed >= SEND_INTERVAL_MS) {
                lastSendTime = now;
                sendMoveCommand();
            } else if (!pendingSendTimeout) {
                pendingSendTimeout = setTimeout(() => {
                    lastSendTime = performance.now();
                    sendMoveCommand();
                    pendingSendTimeout = null;
                }, SEND_INTERVAL_MS - elapsed);
            }
        }

        function sendMoveCommand() {
            if (!ws || ws.readyState !== WebSocket.OPEN) return;
            const payload = {
                cmd: "move",
                joints: jointAngles,
                speed: currentSpeed
            };
            ws.send(JSON.stringify(payload));
        }

        function sendHomeCommand() {
            if (!ws || ws.readyState !== WebSocket.OPEN) return;
            // Update UI sliders to default home
            jointAngles = [90, 45, 120, 90];
            for (let i = 0; i < NUM_JOINTS; i++) {
                const slider = document.getElementById('joint' + i);
                if (slider) slider.value = jointAngles[i];
                const readout = document.getElementById('val' + i);
                if (readout) readout.innerText = jointAngles[i];
            }
            ws.send(JSON.stringify({ cmd: "home" }));
        }

        function toggleServoPower() {
            if (!ws || ws.readyState !== WebSocket.OPEN) return;
            isPowerEnabled = !isPowerEnabled;
            ws.send(JSON.stringify({ cmd: "enable", value: isPowerEnabled }));
            updatePowerUI(isPowerEnabled);
        }

        function updatePowerUI(enabled) {
            isPowerEnabled = enabled;
            const btn = document.getElementById('btnPower');
            if (enabled) {
                btn.className = 'btn-power';
                btn.innerText = '⚡ Servos: ON';
            } else {
                btn.className = 'btn-power disabled-state';
                btn.innerText = '🛑 Servos: OFF';
            }
        }

        let sweepInterval = null;
        let sweepTarget = 180;

        function toggleSweepDemo() {
            const btn = document.getElementById('btnSweep');
            if (sweepInterval) {
                clearInterval(sweepInterval);
                sweepInterval = null;
                btn.className = 'btn-sweep';
                btn.innerText = '🔄 Sweep Demo';
                sendHomeCommand();
            } else {
                btn.className = 'btn-sweep active';
                btn.innerText = '⏹ Stop Sweep';
                sweepTarget = 180;

                const runSweepStep = () => {
                    // Sweeps base (CH0) between 0 and 180 (matches reference code behavior)
                    jointAngles[0] = sweepTarget;
                    const slider = document.getElementById('joint0');
                    if (slider) slider.value = sweepTarget;
                    const val = document.getElementById('val0');
                    if (val) val.innerText = sweepTarget;

                    sendMoveCommand();
                    sweepTarget = (sweepTarget === 180) ? 0 : 180;
                };

                runSweepStep();
                sweepInterval = setInterval(runSweepStep, 3000);
            }
        }

        function sendEnableOtaCommand() {
            if (!ws || ws.readyState !== WebSocket.OPEN) {
                alert('WebSocket not connected. Please ensure ESP32 is online.');
                return;
            }
            const btn = document.getElementById('btnOta');
            if (btn) btn.innerText = '⏳ Initializing OTA on ESP32...';
            ws.send(JSON.stringify({ cmd: "enable_ota" }));
        }

        function handleOtaStatus(data) {
            if (data.enabled) {
                const otaCard = document.getElementById('otaCard');
                if (otaCard) otaCard.style.display = 'block';

                const otaIpDisplay = document.getElementById('otaIpDisplay');
                if (otaIpDisplay) otaIpDisplay.innerText = data.ip;

                const pioCmdInput = document.getElementById('pioCmdInput');
                if (pioCmdInput) pioCmdInput.value = `pio run -t upload --upload-port ${data.ip}`;

                const btnOta = document.getElementById('btnOta');
                if (btnOta) {
                    btnOta.className = 'btn-ota active';
                    btnOta.innerText = `📡 OTA Active! IP: ${data.ip} (Port: ${data.port})`;
                }
            }
        }

        function copyPioCommand() {
            const input = document.getElementById('pioCmdInput');
            if (input) {
                input.select();
                navigator.clipboard.writeText(input.value).then(() => {
                    alert('Copied PlatformIO upload command to clipboard!\n\n' + input.value);
                }).catch(() => {
                    document.execCommand('copy');
                    alert('Copied PlatformIO upload command to clipboard!\n\n' + input.value);
                });
            }
        }

        function uploadFirmwareBin() {
            const fileInput = document.getElementById('binFileInput');
            if (!fileInput.files || fileInput.files.length === 0) {
                alert('Please select a compiled firmware.bin file first.');
                return;
            }
            const file = fileInput.files[0];
            const formData = new FormData();
            formData.append('update', file);

            const progressContainer = document.getElementById('uploadProgressContainer');
            const progressBar = document.getElementById('uploadProgressBar');
            const uploadPercent = document.getElementById('uploadPercent');
            const uploadStatusText = document.getElementById('uploadStatusText');
            const btnUpload = document.getElementById('btnUploadBin');

            progressContainer.style.display = 'block';
            btnUpload.disabled = true;
            uploadStatusText.innerText = 'Uploading ' + file.name + '...';
            progressBar.style.background = '#238636';

            const xhr = new XMLHttpRequest();
            xhr.open('POST', '/update', true);

            xhr.upload.onprogress = (e) => {
                if (e.lengthComputable) {
                    const percent = Math.round((e.loaded / e.total) * 100);
                    progressBar.style.width = percent + '%';
                    uploadPercent.innerText = percent + '%';
                }
            };

            xhr.onload = () => {
                btnUpload.disabled = false;
                if (xhr.status === 200) {
                    uploadStatusText.innerText = 'Upload complete! ESP32 is flashing & rebooting...';
                    progressBar.style.width = '100%';
                    progressBar.style.background = '#238636';
                    setTimeout(() => {
                        window.location.reload();
                    }, 5000);
                } else {
                    uploadStatusText.innerText = 'Flash failed: ' + xhr.responseText;
                    progressBar.style.background = '#da3633';
                }
            };

            xhr.onerror = () => {
                btnUpload.disabled = false;
                uploadStatusText.innerText = 'Network error during firmware upload.';
                progressBar.style.background = '#da3633';
            };

            xhr.send(formData);
        }

        // Initialize WebSocket connection on page load
        window.addEventListener('DOMContentLoaded', connectWebSocket);
    </script>
</body>
</html>
)rawliteral";
