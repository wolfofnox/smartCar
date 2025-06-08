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

fetch("/nav.html")
    .then(response => response.text())
    .then(html => document.getElementById("nav").innerHTML = html);