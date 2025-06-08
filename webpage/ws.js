let ws = {}
function setupWebSocket() {
    if (ws && ws.readyState === WebSocket.OPEN) {
        console.warn('WebSocket already connected');
        return;
    }
    status('info', 'Setting up WebSocket connection...', 5000);
    ws = new WebSocket(`ws://${location.host}/ws`);
    ws.onopen = () => {
        console.log('WebSocket connected');
        status('info', 'WebSocket connected', 3000);
    };
    ws.onmessage = (event) => console.log('Message received:', event.data);
    ws.onerror = (error) => {
        console.error('WebSocket error:', error); 
        status('error', 'WebSocket error: ' + error, 5000); 
    };
    ws.onclose = () => {
        console.log('WebSocket closed'); 
        status('warn', 'WebSocket closed', 5000);
        setTimeout(setupWebSocket, 10000);
    };
}
function sendWSMessage(msg) {
    console.log('Sending message ', msg);
    if (ws.readyState === WebSocket.OPEN) {
        ws.send(msg);
    } else {
        console.warn('WebSocket not open, message not sent:', msg);
    }
}
setupWebSocket();