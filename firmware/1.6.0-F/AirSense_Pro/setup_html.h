#ifndef SETUP_HTML_H
#define SETUP_HTML_H

#include <Arduino.h>

const char SETUP_HTML[] PROGMEM = R"rawliteral(

<!DOCTYPE html>

<html>

<head>

<meta charset="UTF-8">

<meta name="viewport"
      content="width=device-width, initial-scale=1.0">

<title>AirSense Pro</title>

<style>

* {
    box-sizing: border-box;
}

body {
    margin: 0;
    padding: 20px;
    font-family: Arial, sans-serif;
    background: #f2f5f8;
    color: #222;
}

.container {
    max-width: 500px;
    margin: 30px auto;
}

.card {
    background: white;
    border-radius: 18px;
    padding: 28px;
    box-shadow: 0 8px 25px rgba(0,0,0,0.10);
}

.logo {
    text-align: center;
    color: #1976d2;
    font-size: 32px;
    font-weight: bold;
    margin-bottom: 8px;
}

.subtitle {
    text-align: center;
    color: #666;
    margin-bottom: 30px;
}

label {
    display: block;
    font-weight: bold;
    margin-bottom: 8px;
    margin-top: 20px;
}

select,
input[type="password"],
input[type="text"] {
    width: 100%;
    padding: 13px;
    border: 1px solid #ccc;
    border-radius: 10px;
    font-size: 16px;
    background: white;
}

button {
    width: 100%;
    margin-top: 15px;
    padding: 13px;
    border: none;
    border-radius: 10px;
    background: #1976d2;
    color: white;
    font-size: 16px;
    font-weight: bold;
    cursor: pointer;
}

button:active {
    transform: scale(0.98);
}

button:disabled {
    background: #999;
}

.password-row {
    display: flex;
    align-items: center;
    margin-top: 10px;
}

.password-row input {
    width: auto;
    margin-right: 8px;
}

.password-row label {
    margin: 0;
    font-weight: normal;
}

.status {
    text-align: center;
    margin-top: 18px;
    min-height: 24px;
    color: #666;
    font-weight: bold;
}

.footer {
    text-align: center;
    margin-top: 20px;
    font-size: 12px;
    color: #888;
}

.success {
    color: #16803c;
}

.error {
    color: #d32f2f;
}

</style>

</head>

<body>

<div class="container">

<div class="card">

<div class="logo">
AirSense Pro
</div>

<div class="subtitle">
Smart Environmental Monitor
</div>


<label for="networks">
Available Wi-Fi Networks
</label>

<select id="networks">

<option value="">
Press Scan to find networks
</option>

</select>


<button id="scanButton"
        onclick="scanWiFi()">

Scan Wi-Fi

</button>


<label for="password">
Wi-Fi Password
</label>

<input
    type="password"
    id="password"
    placeholder="Enter Wi-Fi password"
    autocomplete="off"
>


<div class="password-row">

<input
    type="checkbox"
    id="showPassword"
    onchange="togglePassword()"
>

<label for="showPassword">
Show Password
</label>

</div>


<button id="connectButton"
        onclick="connectWiFi()">

Connect to Wi-Fi

</button>


<div class="status"
     id="status">
</div>

</div>


<div class="footer">
AirSense Pro Setup Portal
</div>

</div>


<script>


// =====================================================
// Scan Wi-Fi
// =====================================================

async function scanWiFi()
{
    const select =
        document.getElementById("networks");

    const status =
        document.getElementById("status");

    const button =
        document.getElementById("scanButton");


    button.disabled = true;

    button.innerText = "Scanning...";

    status.className = "status";

    status.innerText =
        "Scanning Wi-Fi networks...";


    select.innerHTML =
        "<option>Scanning...</option>";


    try
    {
        const response =
            await fetch("/scan");


        if (!response.ok)
        {
            throw new Error("Scan failed");
        }


        const networks =
            await response.json();


        select.innerHTML = "";


        if (networks.length === 0)
        {
            select.innerHTML =
                "<option value=''>No networks found</option>";

            status.innerText =
                "No Wi-Fi networks found.";

            return;
        }


        networks.forEach(function(network)
        {
            const option =
                document.createElement("option");


            option.value =
                network.ssid;


            option.textContent =
                network.ssid +
                " (" +
                network.rssi +
                " dBm)";


            select.appendChild(option);
        });


        status.innerText =
            networks.length +
            " network(s) found.";

    }
    catch(error)
    {
        select.innerHTML =
            "<option value=''>Scan failed</option>";

        status.className =
            "status error";

        status.innerText =
            "Unable to scan Wi-Fi.";
    }


    button.disabled = false;

    button.innerText = "Scan Wi-Fi";
}


// =====================================================
// Show / Hide Password
// =====================================================

function togglePassword()
{
    const password =
        document.getElementById("password");

    const checkbox =
        document.getElementById("showPassword");


    if (checkbox.checked)
    {
        password.type = "text";
    }
    else
    {
        password.type = "password";
    }
}


// =====================================================
// Connect Wi-Fi
// =====================================================

async function connectWiFi()
{
    const select =
        document.getElementById("networks");

    const password =
        document.getElementById("password");

    const status =
        document.getElementById("status");

    const button =
        document.getElementById("connectButton");


    const ssid =
        select.value;


    if (!ssid)
    {
        status.className =
            "status error";

        status.innerText =
            "Please select a Wi-Fi network.";

        return;
    }


    if (password.value.length < 8)
    {
        status.className =
            "status error";

        status.innerText =
            "Please enter a valid Wi-Fi password.";

        return;
    }


    button.disabled = true;

    button.innerText =
        "Connecting...";


    status.className =
        "status";

    status.innerText =
        "Connecting to " + ssid + "...";


    try
    {
        const body =
            new URLSearchParams();


        body.append("ssid", ssid);

        body.append(
            "password",
            password.value
        );


        const response =
            await fetch(
                "/connect",
                {
                    method: "POST",
                    headers:
                    {
                        "Content-Type":
                            "application/x-www-form-urlencoded"
                    },
                    body: body.toString()
                }
            );


        const result =
            await response.json();


        if (result.success)
        {
            status.className =
                "status success";

            status.innerText =
                "Wi-Fi connected successfully!";

        }
        else
        {
            status.className =
                "status error";

            status.innerText =
                result.message ||
                "Wi-Fi connection failed.";
        }

    }
    catch(error)
    {
        status.className =
            "status error";

        status.innerText =
            "Connection request failed.";
    }


    button.disabled = false;

    button.innerText =
        "Connect to Wi-Fi";
}

</script>

</body>

</html>

)rawliteral";

#endif