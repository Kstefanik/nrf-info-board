/*****************************************************************************
 * @file    app.js
 * @author  Karol Stefanik
 * @brief   Frontend logic for nrf-info-board server client
 *
 * Copyright (c) 2025 Karol Stefanik
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

async function sendDisplayMessage() {
    const rows = Array.from(document.querySelectorAll('.display-row')).map(t => t.value);
    const message = rows.join('\n');
    if (!message.trim()) {
        alert('Please enter a message');
        return;
    }
    fetch('/send_message', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({ message, topic: "info-board/display" })
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            alert('Command sent successfully!');
            document.querySelectorAll('.display-row').forEach(t => t.value = '');
        } else {
            alert('Error: ' + data.error);
        }
    })
    .catch(error => {
        alert('Error sending command: ' + error);
    });
}

async function sendCommand(command) {
    try {
        const response = await fetch('/send_message', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({ message: command, topic: "info-board/command" })
        });
        const data = await response.json();
        if (data.success) {
            alert('Status command sent!');
        } else {
            alert('Error: ' + data.error);
        }
    } catch (error) {
        alert('Error sending status command: ' + error);
    }
}

async function fetchLogs() {
    try {
        const response = await fetch('/get_logs');
        const data = await response.json();
        document.getElementById('logs').textContent = data.logs.join('\n');
    } catch (error) {
        document.getElementById('logs').textContent = "Error loading logs";
    }
}

function clearLogs() {
    fetch('/clear_logs', { method: 'POST' })
        .then(() => {
            document.getElementById('logs').textContent = '';
        });
}

// Fetch logs every 2 seconds
setInterval(fetchLogs, 2000);
fetchLogs();