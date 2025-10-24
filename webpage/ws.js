const WS_event = {
    EVENT_NONE: 0,
    EVENT_TIMEOUT: 1,
    EVENT_ESTOP: 2,
    EVENT_REVERT_SETTINGS: 3
}

const WS_value = {
    VALUE_NONE: 0,
    CONTROL_SPEED: 1,
    CONTROL_STEERING: 2,
    CONTROL_TOP_SERVO: 3,
    CONFIG_STEERING_MAX_PULSEWIDTH: 4,
    CONFIG_STEERING_MIN_PULSEWIDTH: 5,
    CONFIG_TOP_MAX_PULSEWIDTH: 6,
    CONFIG_TOP_MIN_PULSEWIDTH: 7,
    CONFIG_WS_TIMEOUT: 8
}

let ws = {}
function setupWebSocket() {
    if (ws && ws.readyState === WebSocket.OPEN) {
        console.warn('WebSocket already connected');
        return;
    }
    message('info', 'Setting up WebSocket connection...', 5000);
    ws = new WebSocket(`ws://${location.host}/ws`);
    ws.onopen = () => {
        console.log('WebSocket connected');
        message('info', 'WebSocket connected', 3000);
    };
    ws.onmessage = (event) => console.log('Message received:', event.data);
    ws.onerror = (error) => {
        console.error('WebSocket error:', error); 
        message('error', 'WebSocket error: ' + error, 5000); 
    };
    ws.onclose = () => {
        console.log('WebSocket closed'); 
        message('warn', 'WebSocket closed', 5000);
        setTimeout(setupWebSocket, 10000);
    };
}
function sendWSBinaryControl(type, value) {
    if (ws.readyState === WebSocket.OPEN) {
        const buffer = new ArrayBuffer(3);
        const view = new DataView(buffer);
        view.setUint8(0, type);
        view.setInt16(1, value, true); // true for little-endian
        if (ws.readyState === WebSocket.OPEN) {
            ws.send(buffer);
        } else {
            console.warn('WebSocket not open, message not sent: ', buffer);
        }
    }
}

function sendWSEvent(eventType) {
    if (ws.readyState === WebSocket.OPEN) {
        const buffer = new ArrayBuffer(1);
        const view = new DataView(buffer);
        view.setUint8(0, eventType);
        ws.send(buffer);
    }
}

function sendWSMessage(msg) {
    console.log('Sending message ', msg);
    if (ws.readyState === WebSocket.OPEN) {
        ws.send(msg);
    } else {
        console.warn('WebSocket not open, message not sent: ', msg);
    }
}
setupWebSocket();