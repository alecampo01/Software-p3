let port;
let reader;
let writer;
let outputStream;
let inputStream;
let keepReading = true;
let isConnected = false;

// Job state
let gcodeLines = [];
let currentLineIdx = 0;
let isJobRunning = false;
let isWaitingForOk = false;

// DOM Elements
const btnConnect = document.getElementById('btnConnect');
const statusIndicator = document.getElementById('statusIndicator');
const statusText = document.getElementById('statusText');
const btnStartJob = document.getElementById('btnStartJob');
const gcodePreview = document.getElementById('gcodePreview');

// Translator DOM Elements
const btnTranslate = document.getElementById('btnTranslate');
const textInput = document.getElementById('textInput');
const fileInput = document.getElementById('fileInput');
const brailleOutput = document.getElementById('brailleOutput');
const jobProgress = document.getElementById('jobProgress');
const jobStatus = document.getElementById('jobStatus');
const terminalOutput = document.getElementById('terminalOutput');
const cmdInput = document.getElementById('cmdInput');
const btnSendCmd = document.getElementById('btnSendCmd');
const btnTestSolenoid = document.getElementById('btnTestSolenoid');
const angleSelect = document.getElementById('angleSelect');
const angleJogBtns = document.querySelectorAll('.angle-jog-btn');
const standardJogBtns = document.querySelectorAll('.jog-btn:not(.angle-jog-btn)');

// --- Serial Connection ---

btnConnect.addEventListener('click', async () => {
    if (isConnected) {
        await disconnect();
    } else {
        await connect();
    }
});

async function connect() {
    try {
        if (!navigator.serial) {
            logToTerminal("Error: Web Serial API not supported in this browser. Use Chrome/Edge.", "error");
            return;
        }

        port = await navigator.serial.requestPort();
        await port.open({ baudRate: 115200 }); // Common baud rate for ESP32/GRBL

        // Setup stream decoding/encoding
        const textDecoder = new TextDecoderStream();
        const readableStreamClosed = port.readable.pipeTo(textDecoder.writable);
        inputStream = textDecoder.readable;
        reader = inputStream.getReader();

        const textEncoder = new TextEncoderStream();
        const writableStreamClosed = textEncoder.readable.pipeTo(port.writable);
        outputStream = textEncoder.writable;
        writer = outputStream.getWriter();

        isConnected = true;
        updateConnectionUI(true);
        logToTerminal("Connected to Machine", "success");
        
        keepReading = true;
        readLoop();
    } catch (e) {
        logToTerminal(`Connection Error: ${e.message}`, "error");
    }
}

async function disconnect() {
    keepReading = false;
    
    if (reader) {
        await reader.cancel();
        reader.releaseLock();
    }
    if (writer) {
        await writer.close();
        writer.releaseLock();
    }
    if (port) {
        await port.close();
    }

    isConnected = false;
    updateConnectionUI(false);
    logToTerminal("Disconnected", "warning");
}

function updateConnectionUI(connected) {
    if (connected) {
        statusIndicator.className = 'indicator connected';
        statusText.textContent = 'Connected';
        btnConnect.textContent = 'Disconnect';
        btnConnect.classList.remove('btn-primary');
        btnConnect.classList.add('btn-secondary');
    } else {
        statusIndicator.className = 'indicator disconnected';
        statusText.textContent = 'Disconnected';
        btnConnect.textContent = 'Connect to Machine';
        btnConnect.classList.add('btn-primary');
        btnConnect.classList.remove('btn-secondary');
    }
}

// --- Serial Read/Write ---

async function readLoop() {
    let buffer = "";
    while (port.readable && keepReading) {
        try {
            const { value, done } = await reader.read();
            if (done) break;
            if (value) {
                buffer += value;
                const lines = buffer.split('\n');
                buffer = lines.pop(); // keep incomplete line
                
                for (const line of lines) {
                    const cl = line.trim();
                    if (cl) {
                        logToTerminal(`RX: ${cl}`);
                        handleIncomingMessage(cl);
                    }
                }
            }
        } catch (error) {
            logToTerminal(`Read error: ${error.message}`, "error");
            break;
        }
    }
}

async function sendCommand(cmd) {
    if (!isConnected || !writer) {
        logToTerminal("Cannot send: Not connected", "error");
        return;
    }
    
    // Add newline for CNC
    const formattedCmd = cmd.trim() + "\n";
    logToTerminal(`TX: ${cmd.trim()}`, "tx");
    
    try {
        await writer.write(formattedCmd);
    } catch (e) {
        logToTerminal(`Write error: ${e.message}`, "error");
    }
}

function handleIncomingMessage(msg) {
    // GRBL typical OK response
    if (msg.toLowerCase() === 'ok') {
        if (isJobRunning && isWaitingForOk) {
            isWaitingForOk = false;
            sendNextGcodeLine();
        }
    }
}

// --- Translator Logic ---

btnTranslate.addEventListener('click', () => {
    const inputText = textInput.value;
    const inputArchivo = fileInput.files[0];
    const urlBackend = 'https://the-braille-interpreter.onrender.com//traducir';

    brailleOutput.value = "Translating...";
    gcodePreview.value = "Generating G-Code...";
    btnStartJob.disabled = true;

    const formData = new FormData();

    if (inputArchivo) {
        formData.append('archivo', inputArchivo);
        hacerPeticion(urlBackend, { method: 'POST', body: formData });
    } else if (inputText.trim() !== "") {
        hacerPeticion(urlBackend, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ texto: inputText })
        });
    } else {
        brailleOutput.value = "Please enter text or upload a file.";
        gcodePreview.value = "";
    }
});

async function hacerPeticion(url, opciones) {
    try {
        const respuesta = await fetch(url, opciones);
        const datos = await respuesta.json();
        
        brailleOutput.value = datos.braille;
        gcodePreview.value = datos.gcode;
        
        // Prepare job
        gcodeLines = datos.gcode.split('\n').filter(line => line.trim().length > 0);
        if (gcodeLines.length > 0) {
            btnStartJob.disabled = false;
            logToTerminal(`Generated ${gcodeLines.length} lines of G-code`);
        }
        
    } catch (error) {
        brailleOutput.value = "Error connecting to server.";
        gcodePreview.value = "";
    }
}

// --- Job Control ---

btnStartJob.addEventListener('click', () => {
    if (!isConnected) {
        alert("Please connect to the machine first!");
        return;
    }
    
    if (!isJobRunning) {
        startJob();
    } else {
        stopJob();
    }
});

function startJob() {
    isJobRunning = true;
    currentLineIdx = 0;
    isWaitingForOk = false;
    
    btnStartJob.textContent = 'Stop Job';
    btnStartJob.style.backgroundColor = 'var(--danger)';
    
    jobStatus.textContent = 'Running...';
    sendNextGcodeLine();
}

function stopJob() {
    isJobRunning = false;
    isWaitingForOk = false;
    
    btnStartJob.textContent = 'Start Job';
    btnStartJob.style.backgroundColor = '';
    
    jobStatus.textContent = 'Stopped';
}

function sendNextGcodeLine() {
    if (!isJobRunning) return;
    
    if (currentLineIdx >= gcodeLines.length) {
        jobStatus.textContent = 'Job Complete!';
        isJobRunning = false;
        btnStartJob.textContent = 'Start Job';
        btnStartJob.style.backgroundColor = '';
        jobProgress.style.width = '100%';
        return;
    }
    
    // Update progress
    const progress = (currentLineIdx / gcodeLines.length) * 100;
    jobProgress.style.width = `${progress}%`;
    
    const line = gcodeLines[currentLineIdx].trim();
    
    // Skip empty lines or comments
    if (line.length === 0 || line.startsWith(';')) {
        currentLineIdx++;
        setTimeout(sendNextGcodeLine, 10); // Small delay to avoid stack overflow
        return;
    }
    
    isWaitingForOk = true;
    sendCommand(line);
    currentLineIdx++;
}

// --- Manual Controls ---

angleJogBtns.forEach(btn => {
    btn.addEventListener('click', () => {
        const axis = btn.getAttribute('data-axis');
        const dir = parseInt(btn.getAttribute('data-dir'));
        const angle = parseFloat(angleSelect.value);

        // Read per-axis calibration from UI inputs
        const mmPerRev = axis === 'X'
            ? parseFloat(document.getElementById('mmPerRevX').value)
            : parseFloat(document.getElementById('mmPerRevY').value);

        const mm = (angle / 360.0) * mmPerRev * dir;
        const cmd = `G91 G0 ${axis}${mm.toFixed(4)}`;
        sendCommand(cmd);
        logToTerminal(`Jog ${axis}${dir > 0 ? '+' : '-'} ${angle}° → ${mm.toFixed(4)}mm`);
    });
});

standardJogBtns.forEach(btn => {
    btn.addEventListener('click', () => {
        const cmd = btn.getAttribute('data-cmd');
        if (cmd) {
            sendCommand(cmd);
        }
    });
});

btnTestSolenoid.addEventListener('click', () => {
    // Fire solenoid by going down, then lift
    // G1 Z-0.6 (Fire), wait 100ms, G0 Z2 (Lift)
    sendCommand("G1 Z-0.6 F150");
    setTimeout(() => {
        sendCommand("G0 Z2.0");
    }, 150); 
});

btnSendCmd.addEventListener('click', () => {
    const cmd = cmdInput.value;
    if (cmd) {
        sendCommand(cmd);
        cmdInput.value = '';
    }
});

cmdInput.addEventListener('keypress', (e) => {
    if (e.key === 'Enter') {
        btnSendCmd.click();
    }
});

// --- Terminal UI ---

function logToTerminal(text, type = "normal") {
    const div = document.createElement('div');
    div.textContent = text;
    
    if (type === "error") div.style.color = "var(--danger)";
    if (type === "success") div.style.color = "var(--success)";
    if (type === "warning") div.style.color = "orange";
    if (type === "tx") div.style.color = "#93c5fd"; // light blue for TX
    
    terminalOutput.appendChild(div);
    terminalOutput.scrollTop = terminalOutput.scrollHeight;
}
