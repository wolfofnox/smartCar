let statusQueue = [];
let statusActive = false;
let statusTimeout = null;
let statusStartTime = 0;
const fastStatusduration = 1000;

function processStatusQueue() {
    if (statusActive || statusQueue.length === 0) return;
    statusActive = true;
    const { type, message, duration } = statusQueue.shift();
    const statusDiv = document.querySelector('.status');
    if (!statusDiv) {
        console.error('Status div not found');
        statusActive = false;
        return;
    }
    statusDiv.className = 'status ' + type;
    statusDiv.innerHTML = `<p>${message}</p>`;
    statusStartTime = Date.now();
    const showTime = statusQueue.length > 0 ? fastStatusduration : (duration || 2000);
    statusTimeout = setTimeout(() => {
        statusDiv.className = 'status';
        statusDiv.innerHTML = '';
        statusActive = false;
        processStatusQueue();
    }, showTime);
}

function status(type, message, duration) {
    const statusDiv = document.querySelector('.status');
    if (!statusDiv) {
        console.error('Status div not found');
        return;
    }
        if (type === 'none') {
        statusDiv.className = 'status';
        statusDiv.innerHTML = '';
        statusActive = false;
        processStatusQueue();
        return;
    }
    statusQueue.push({ type, message, duration });
    if (statusActive) {
        const elapsed = Date.now() - statusStartTime;
        if (elapsed < fastStatusduration) {
            clearTimeout(statusTimeout);
            statusTimeout = setTimeout(() => {
                statusDiv.className = 'status';
                statusDiv.innerHTML = '';
                statusActive = false;
                processStatusQueue();
            }, fastStatusduration - elapsed);
        } else {
            clearTimeout(statusTimeout);
            statusActive = false;
            processStatusQueue();
        }
    } else {
        processStatusQueue();
    }
}

function msToTime(ms) {
    let totalSeconds = Math.floor(ms / 1000);
    let hours = Math.floor(totalSeconds / 3600);
    let minutes = Math.floor((totalSeconds % 3600) / 60);
    let seconds = totalSeconds % 60;
    return `${hours}:${minutes.toString().padStart(2, '0')}:${seconds.toString().padStart(2, '0')}`;
}

function updateFooter() {
    Promise.all([
        fetch('/status.json').then(r => r.json()).catch(() => ({})),
        fetch('/wifi-status.json').then(r => r.json()).catch(() => ({}))
    ]).then(([status, wifi]) => {
        let html = '';
        html += wifi.connected ? 'WiFi: Connected' : 'WiFi: Disconnected';
        html += ' | IP: ' + (wifi.ip || 'N/A');
        html += ' | Heap: ' + (status.heap || 'N/A') + ' bytes';
        html += ' | Uptime: ' + (msToTime(status.uptime) || 'N/A');
        html += ' | FW: ' + (status.version || 'N/A');
        html += ' | LED brightness: ' + (status.brightness || 0);
        html += ' | <span style="font-size:0.9em;">Last update: ' + new Date().toLocaleTimeString() + '</span>';
        document.getElementById('footer').innerHTML = html;
    });
}

fetch("/nav.html")
    .then(response => response.text())
    .then(html => document.getElementById("nav").innerHTML = html);

setInterval(updateFooter, 5000); // Update every 5 seconds
updateFooter();