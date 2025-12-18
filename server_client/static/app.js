/*****************************************************************************
 * @file    app.js
 * @author  Karol Stefanik
 * @brief   Frontend logic for nrf-info-board server client
 *
 * Copyright (c) 2025 Karol Stefanik
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/


async function get_display_message() {
    const rows = Array.from(document.querySelectorAll('.display-row')).map(t => t.value);
    const message = rows.join('\n');
    fetch('/get_display_message', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({ message})
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            alert('Message submitted successfully!');
            document.getElementById('sent-output').value = message;
            document.querySelectorAll('.display-row').forEach(t => t.value = '');
        } else {
            alert('Error: ' + data.error);
        }
    })
    .catch(error => {
        alert('Error sending command: ' + error);
    });
}

async function get_command(command) {
    try {
        const response = await fetch('/get_command', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({command})
        });
        const data = await response.json();
        if (data.success) {
            alert('Command sent!');
            const nextCmd = document.getElementById('next-command');
            if (nextCmd) {
                nextCmd.value = command;
            }
        } else {
            alert('Error: ' + data.error);
        }
    } catch (error) {
        alert('Error sending command: ' + error);
    }
}

async function fetch_logs() {
    try {
        const response = await fetch('/get_logs');
        const data = await response.json();
        document.getElementById('logs').textContent = data.logs.join('\n');
    } catch (error) {
        document.getElementById('logs').textContent = "Error loading logs";
    }
}

function clear_logs() {
    fetch('/clear_logs', { method: 'POST' })
        .then(() => {
            document.getElementById('logs').textContent = '';
        });
}

// Fetch logs every 2 seconds
setInterval(fetch_logs, 2000);
fetch_logs();