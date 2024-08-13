#ifndef CONTROLCENTERHTML_H
#define CONTROLCENTERHTML_H

constexpr const char control_center_html[] = R"=====(
<!DOCTYPE html>
<html lang="en">
    <head>
        <link rel="icon" href="data:,">
        <meta charset="utf-8" />
        <title>Arduino Command Center</title>
        <meta content='width=device-width, initial-scale=1' name='viewport'/>
        <style>

        @-webkit-keyframes glowing {
            from {
                box-shadow: 0 0 1px #bc9958, 0 0 2px #bc9958, 5px 0 0px #2f2f2f;
            }
            to {
                box-shadow: 0 0 5px #bc9958, 0 0 0px #2f2f2f, 0 0 15px #2f2f2f;
            }
        }
        html, body {
            font-family: "Trebuchet MS";
            font-size: 16px;
            background-color: #bc9958;
            color: #2f2f2f;
            overflow-x: hidden;
            max-width: 100vw;
        }

        h1{
            font-size: 24px;
            margin-top: 20px;
            text-align: center;
            letter-spacing: -6px;
        }

        h2 {
            font-size: 20px;
            display: block;
            margin: 18px 0 0;
        }
        h4 {
            font-size: 14px;
            font-weight: normal;
            margin: 0;
            padding: 0;
            color: white;
        }
        .button-container {
            display: flex;
            flex-direction: column;
            align-items: center;
            padding-top: 10px;
        }
        .button {
            width: 100%;
            font-size: 20px;
            padding: 8px;
            margin: 8px;
            border-radius: 5px;
            background-color: transparent;
            color: #2f2f2f;
            border: 2px solid #2f2f2f;
            box-shadow: none;
        }
        .button:hover {
            background-color: #2f2f2f;
            color: #bc9958;
        }
        #color-button-container .button:hover {
            animation: glowing 1s ease-in-out infinite alternate;
        }
        #palette-button-container .button:hover {
            animation: glowing 1s ease-in-out infinite alternate;
        }
        .sliders {
            margin-top: 8px;
        }
        .slider {
            width: 100%;
        }

        hr {
            display: block;
            margin-top: 16px;
            border: 2px solid #2f2f2f;
        }
        .color-slider-box {
            padding: 30px;
        }

        .brightness-slider-box {
            padding: 30px;
            background: linear-gradient(90deg, rgb(30, 30, 30), rgb(225, 225, 225));
        }

        .arduino-stats {
            margin-top: 10px;
        }

        .table-row {
            display: table-row;
        }

        .table-cell {
            display: table-cell;
            padding-top: 5px;
            padding-right: 10px;
        }

        .status-key {
            font-size: 14px;
            font-weight: bold;
        }

        .status-value {
            font-size: 14px;
        }

        </style>
    </head>
    <body>
<script>

function RequestState(min_update_interval)
{
    this.previous_request_time = 0;
    this.previous_request_path = "";
    this.min_update_interval = min_update_interval;
}

var color_request_state = new RequestState(100);
var brightness_request_state = new RequestState(100);
var radius_request_state = new RequestState(100);
var temp_request_state = new RequestState(100);
var periodic_status_update;
var color_box;
var brightness_box;
var inner_radius_box;
var outer_radius_box;
var refresh_interval_box;

const showColorSliderModes = ['breathe', 'solid-color', 'sparkle', 'strobe'];
const showPaletteModes = ['palette-stream', 'palette-cycle'];

function background_request(path, request_state = null, force = false, response_handler = null)
{
    if (request_state) {
        const now = Date.now();

        if (!force && (request_state.previous_request_time + request_state.min_update_interval > now)) {
            return;
        }

        if (path == request_state.previous_request_path) {
            return;
        }

        request_state.previous_request_time = now;
        request_state.previous_request_path = path;
    }

    fetch(path)
        .then((response) => {
            if (!response.ok) {
                throw new Error(`HTTP ${response.status} for ${path}`);
            }
            else if (response_handler) {
                response_handler(response);
            }
        })
        .catch((error) => {
            error_box = document.getElementById("error-box");
            error_box.innerText = error.message;
        });
}

function set_rgb(r, g, b) {
    background_request(`/color/rgb/${r}/${g}/${b}`);
    let colorBox = document.getElementById("color-box");
    colorBox.style.backgroundColor = `rgb(${r}, ${g}, ${b})`;
}

function set_devices(devices_mask) {
    background_request(`/devices/${devices_mask}`);
}

function set_hue(element, final = false) {
    color_box.style.backgroundColor = "hsl(" + element.value + ", 100%, 50%)";
    hue = Math.round(255 * element.value / 360);
    background_request("/color/hue/" + hue, color_request_state, final);
}

function set_palette(palette) {
    background_request(`/palette/${palette}`);
}

function set_brightness(element, final = false) {
    background_request("/brightness/" + element.value, brightness_request_state, final);
}

function set_light_show_mode(element) {
    background_request("/mode/" + element.id);
    updateColorWheelVisibility(element.id);
}

function set_origin(element) {
    background_request("/origin/" + element.id);
}

function set_radius(element, final = false) {
    if (element.id == "inner") {
        inner_radius_box.innerText = element.value + "m";
    }
    else if (element.id == "outer") {
        outer_radius_box.innerText = element.value + "m";
    }

    background_request("/radius/" + element.id + "/" + element.value, radius_request_state, final);
}

function set_thermometer(element, final = false) {
    if (element.id == "min") {
        min_temp_box.innerText = element.value + "F";
    }
    else if (element.id == "max") {
        max_temp_box.innerText = element.value + "F";
    }

    background_request("/thermometer/" + element.id + "/" + element.value, temp_request_state, final);
}

function set_refresh(element) {
    clearInterval(periodic_status_update);
    refresh_interval = element.value * 1000;
    refresh_interval_box.innerText = element.value + "s";
    periodic_status_update = setInterval(request_status, element.value * 1000);
}

function get_json_value(json, path, default_value = "unknown") {
    let json_object = json;

    if (path.length == 0) {
        return default_value;
    }

    for (i = 0; i < path.length; i++) {
        if (path[i] in json_object) {
            if (i == path.length - 1) {
                return json_object[path[i]];
            }
            else {
                json_object = json_object[path[i]];
            }
        }
        else {
            break;
        }
    }

    return default_value;
}

function update_status(response) {
    response.json().then(json => {
        text = JSON.stringify(json);

        temp = get_json_value(json, ["temperature"]);
        document.getElementById("status-temperature").innerText = temp + " F";

        lat = get_json_value(json, ["position", "latitude"]);
        long = get_json_value(json, ["position", "longitude"]);
        document.getElementById("status-position").innerText = lat + ", " + long;

        date_time = get_json_value(json, ["date_time"]);
        document.getElementById("status-date-time").innerText = date_time;

        speed = get_json_value(json, ["speed"]);
        document.getElementById("status-speed").innerText = speed + " km/h";

        light_show_mode = get_json_value(json, ["light_show_mode"]);
        document.getElementById("status-light-show-mode").innerText = light_show_mode;
        updateColorWheelVisibility(light_show_mode);

        brightness = get_json_value(json, ["brightness"]);
        percentage = Math.round(100 * brightness / 255);
        document.getElementById("status-brightness").innerText = percentage + "% (" + brightness + "/255)";

        center_radius = get_json_value(json, ["center_radius"]);
        perimeter_radius = get_json_value(json, ["perimeter_radius"]);
        document.getElementById("status-radius").innerText = "Center: " + center_radius + " m, Perimeter: " + perimeter_radius + " m";

        thermometer_min = get_json_value(json, ["thermometer_min"]);
        thermometer_max = get_json_value(json, ["thermometer_max"]);
        document.getElementById("status-thermometer").innerText = "Min: " + thermometer_min + " F, Max: " + thermometer_max + " F";

        lat = get_json_value(json, ["origin", "latitude"]);
        long = get_json_value(json, ["origin", "longitude"]);
        dist = get_json_value(json, ["distance_to_origin"]);
        unit = "m";
        if (dist > 1000) {
            dist /= 1000;
            unit = "km";
        }
        dist = Math.round(dist);
        document.getElementById("status-origin").innerText = lat + ", " + long + " (" + dist + " " + unit + " away)";

        devices_mask = get_json_value(json, ["synced_devices"]);
        devices_list = "";

        if (devices_mask == 0) {
            devices_list = "None";
        }
        else if (devices_mask == 0xff) {
            devices_list = "All";
        }
        else {
            if (devices_mask & 0x1) {
                devices_list += "Bike";
            }

            if (devices_mask & 0x2) {
                devices_list += "Coat";
            }
        }

        document.getElementById("status-synced-devices").innerText = devices_list;

        r = get_json_value(json, ["color", "r"]);
        g = get_json_value(json, ["color", "g"]);
        b = get_json_value(json, ["color", "b"]);
        document.getElementById("status-color").innerText = "R:" + r + " G:" + g + " B:" + b;
    });
}

function request_status() {
    background_request("/status", null, false, update_status);
}

const updateColorWheelVisibility = currentLightShow => {
    const colorButtonContainer = document.getElementById('color-button-container');
    const colorBoxSlider = document.getElementById('color-box');
    const colorHeader = document.getElementById('color-heading');
    if(showColorSliderModes.includes(currentLightShow)) {
        colorBoxSlider.style.display = 'block';
        colorButtonContainer.style.display = 'block';
        colorHeader.style.display = 'block';
    } else {
        colorBoxSlider.style.display = 'none';
        colorButtonContainer.style.display = 'none';
        colorHeader.style.display = 'none';
    }
}

const updatePaletteVisability = currentLightShow => {
    const paletteButtonContainber = document.getElementById('palette-button-container');
    const paletteheader = document.getElementById('palette-heading');
    if(showPaletteModes.includes(currentLightShow)) {
        paletteButtonContainer.style.display = 'block';
        paletteHeader.style.display = 'block';
    } else {
        paletteButtonContainer.style.display = 'none';
        paletteHeader.style.display = 'none';
    }
}

const setButtonColors = () => {
    const buttons = document.getElementById('color-button-container').querySelectorAll('.button');
    buttons.forEach(button => {
        button.style.backgroundColor = button.id;
    });
}

document.addEventListener('DOMContentLoaded', function(){
    color_box = document.getElementById("color-box");
    color_box.style.backgroundColor = "hsl(180, 100%, 50%)";
    brightness_box = document.getElementById("brightness-box");
    inner_radius_box = document.getElementById("inner-radius-box");
    outer_radius_box = document.getElementById("outer-radius-box");
    min_temp_box = document.getElementById("min-temp-box");
    max_temp_box = document.getElementById("max-temp-box");
    refresh_interval_box = document.getElementById("refresh-interval-box");
    request_status();
    periodic_status_update = setInterval(request_status, 10000);
    setButtonColors();
});

</script>

        <h1>)`(</h1>
        <h2>Light Show Mode</h2>
        <div class="button-container">
            <input id="color-wheel" type="button" class="button" value="Color Wheel" onclick="set_light_show_mode(this);"/>
            <input id="color-radial" type="button" class="button" value="Color Radial" onclick="set_light_show_mode(this);"/>
            <input id="color-speedometer" type="button" class="button" value="Color Speedometer" onclick="set_light_show_mode(this);"/>
            <input id="color-thermometer" type="button" class="button" value="Color Thermometer" onclick="set_light_show_mode(this);"/>
            <input id="color-clock" type="button" class="button" value="Color Clock" onclick="set_light_show_mode(this);"/>
            <input id="palette-stream" type="button" class="button" value="Palette Stream" onclick="set_light_show_mode(this);"/>
            <input id="palette-cycle" type="button" class="button" value="Palette Cycle" onclick="set_light_show_mode(this);"/>
            <input id="solid-color" type="button" class="button" value="Solid Color" onclick="set_light_show_mode(this);"/>
            <input id="spectrum-cycle" type="button" class="button" value="Spectrum Cycle" onclick="set_light_show_mode(this);"/>
            <input id="spectrum-stream" type="button" class="button" value="Spectrum Stream" onclick="set_light_show_mode(this);"/>
            <input id="spectrum-sparkle" type="button" class="button" value="Spectrum Sparkle" onclick="set_light_show_mode(this);"/>
            <input id="strobe" type="button" class="button" value="Strobe" onclick="set_light_show_mode(this);"/>
            <input id="sparkle" type="button" class="button" value="Sparkle" onclick="set_light_show_mode(this);"/>
            <input id="position-status" type="button" class="button" value="Position Status" onclick="set_light_show_mode(this);"/>
            <input id="breathe" type="button" class="button" value="Breathe" onclick="set_light_show_mode(this);"/>
        </div>

        <h2 id="palette-heading">Palette</h2>
        <div class="button-container" id="palette-button-container">
            <input type="button" class="button" value="cool" id="cool" onclick="set_palette('cool');"/>
            <input type="button" class="button" value="earth" id="earth" onclick="set_palette('earth');"/>
            <input type="button" class="button" value="everglow" id="everglow" onclick="set_palette('everglow');"/>
            <input type="button" class="button" value="heart" id="heart" onclick="set_palette('heart');"/>
            <input type="button" class="button" value="lava" id="lava" onclick="set_palette('lava');"/>
            <input type="button" class="button" value="pinksplash" id="pinksplash" onclick="set_palette('pinksplash');"/>
            <input type="button" class="button" value="r" id="r" onclick="set_palette('r');"/>
            <input type="button" class="button" value="sofia" id="sofia" onclick="set_palette('sofia');"/>
            <input type="button" class="button" value="sunset" id="sunset" onclick="set_palette('sunset');"/>
            <input type="button" class="button" value="trove" id="trove" onclick="set_palette('trove');"/>
            <input type="button" class="button" value="velvet" id="velvet" onclick="set_palette('velvet');"/>
            <input type="button" class="button" value="vga" id="vga" onclick="set_palette('vga');"/>

        </div>

        <h2 id="color-heading">Color</h2>
        <div class="button-container" id="color-button-container">
            <input type="button" class="button" value="Red" id="red" onclick="set_rgb(255, 0, 0);"/>
            <input type="button" class="button" value="Orange" id="orange" onclick="set_rgb(221, 35, 0);"/>
            <input type="button" class="button" value="Yellow" id="yellow" onclick="set_rgb(255, 255, 0);"/>
            <input type="button" class="button" value="Green" id="green" onclick="set_rgb(0, 255, 0);"/>
            <input type="button" class="button" value="Blue" id="blue" onclick="set_rgb(0, 0, 255);"/>
            <input type="button" class="button" value="Purple" id="purple" onclick="set_rgb(128, 0, 128);"/>
            <input type="button" class="button" value="White" id="white" onclick="set_rgb(255, 255, 255);"/>
            <input type="button" class="button" value="Off" id="gray" onclick="set_rgb(0, 0, 0);"/>
        </div>
        <div class="sliders">
            <div id ="color-box" class="color-slider-box">
                <h4>Color Select:</h4>
                <input type="range" min="0" max="360" vale="180" id="color-slider-input" class="slider" oninput="set_hue(this);" onmouseup="set_hue(this, true);" ontouchend="set_hue(this, true);">
            </div>
            <div id="brightness-box" class="brightness-slider-box">
                <h4>Brightness:</h4>
                <input type="range" min="0" max="255" value="128" class="slider" oninput="set_brightness(this);" onmouseup="set_brightness(this, true);" ontouchend="set_brightness(this, true);">
            </div>
        </div>
        <hr />
        <h2>Origin</h2>
        <div>
            <div class="button-container">
                <input id="default" type="button" class="button" value="Use Default Point" onclick="set_origin(this);"/>
                <input id="initial" type="button" class="button" value="Use Initial Acquisition Point" onclick="set_origin(this);"/>
                <input id="recenter" type="button" class="button" value="Recenter to Current Position" onclick="set_origin(this);"/>
            </div>
        </div>
        <h2>Sync Other Devices</h2>
        <div>
            <div class="button-container">
                <input type="button" class="button" value="Bike" onclick="set_devices(1);"/>
                <input type="button" class="button" value="Coat" onclick="set_devices(2);"/>
                <input type="button" class="button" value="All" onclick="set_devices(255);"/>
                <input type="button" class="button" value="None" onclick="set_devices(0);"/>
            </div>
        </div>
        <h2>Playa Configuration</h2>
        <div>
            <div class="heading2 table-cell">Center Radius</div><div id="inner-radius-box" class="heading2 table-cell">220m</div>
            <input type="range" min="50" max="500" value="220" class="slider" id="inner" oninput="set_radius(this);" onmouseup="set_radius(this, true);" ontouchend="set_radius(this, true);">
            <div class="heading2 table-cell">Perimeter Radius</div><div id="outer-radius-box" class="heading2 table-cell">2500m</div>
            <input type="range" min="550" max="2500" value="2000" class="slider" id="outer" oninput="set_radius(this);" onmouseup="set_radius(this, true);" ontouchend="set_radius(this, true);">
        </div>

        <h2>Thermometer Configuration</h2>
        <div>
            <div class="heading2 table-cell">Min Temperature</div><div id="min-temp-box" class="heading2 table-cell">70F</div>
            <input type="range" min="30" max="80" value="70" class="slider" id="min" oninput="set_thermometer(this);" onmouseup="set_thermometer(this, true);" ontouchend="set_thermometer(this, true);">
            <div class="heading2 table-cell">Max Temperature</div><div id="max-temp-box" class="heading2 table-cell">95F</div>
            <input type="range" min="70" max="120" value="95" class="slider" id="max" oninput="set_thermometer(this);" onmouseup="set_thermometer(this, true);" ontouchend="set_thermometer(this, true);">
        </div>

        <h2>Arduino Status</h2>
        <div>
            <div class="heading2 table-cell">Refresh Interval</div>
            <div id="refresh-interval-box" class="heading2 table-cell">20s</div>
            <input type="range" min="10" max="60" value="20" class="slider" id="inner" oninput="set_refresh(this);" onmouseup="set_refresh(this);" ontouchend="set_refresh(this);">
        </div>
        <div class="arduino-stats">
            <div class="table-row">
                <div class="table-cell status-key">Date / Time</div>
                <div id="status-date-time" class="table-cell status-value">Unknown</div>
            </div>
            <div class="table-row">
                <div class="table-cell status-key">Current Position</div>
                <div id="status-position" class="table-cell status-value">Unknown</div>
            </div>
            <div class="table-row">
                <div class="table-cell status-key">Temperature</div>
                <div id="status-temperature" class="table-cell status-value">Unknown</div>
            </div>
            <div class="table-row">
                <div class="table-cell status-key">Speed</div>
                <div id="status-speed" class="table-cell status-value">Unknown</div>
            </div>
            <div class="table-row">
                <div class="table-cell status-key">Light Show Mode</div>
                <div id="status-light-show-mode" class="table-cell status-value">Unknown</div>
            </div>
            <div class="table-row">
                <div class="table-cell status-key">Color</div>
                <div id="status-color" class="table-cell status-value">Unknown</div>
            </div>
            <div class="table-row">
                <div class="table-cell status-key">Brightness</div>
                <div id="status-brightness" class="table-cell status-value">Unknown</div>
            </div>
            <div class="table-row">
                <div class="table-cell status-key">Origin Position</div>
                <div id="status-origin" class="table-cell status-value">Unknown</div>
            </div>
            <div class="table-row">
                <div class="table-cell status-key">Synced Devices</div>
                <div id="status-synced-devices" class="table-cell status-value">Unknown</div>
            </div>
            <div class="table-row">
                <div class="table-cell status-key">Playa Radius</div>
                <div id="status-radius" class="table-cell status-value">Unknown</div>
            </div>
            <div class="table-row">
                <div class="table-cell status-key">Thermometer</div>
                <div id="status-thermometer" class="table-cell status-value">Unknown</div>
            </div>
            <h3>Request Errors</h3>
            <div id="error-box">None</div>
        </div>
    </body>
</html>)=====";

#endif // CONTROLCENTERHTML_H
