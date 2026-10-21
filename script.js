// script.js - the web page logic.
// IMPORTANT: this file does NOT find any routes. It only
//   1. asks the C++ server for the map           (GET  /api/graph)
//   2. asks the C++ server for a route           (POST /api/route)
//   3. draws what the server answers.

let nodes = [];        // filled from the server
let roads = [];        // filled from the server
let startId = null;
let destinationId = null;
let animationId = 0;   // used to stop an old animation

const startSelect = document.getElementById("start");
const destinationSelect = document.getElementById("destination");
const algorithmSelect = document.getElementById("algorithm");
const message = document.getElementById("message");
const resultBox = document.getElementById("result");
const map = document.getElementById("map");

// ---------- talking to the C++ server ----------

async function askServerForRoute(start, destination, algorithm) {
    const response = await fetch("/api/route", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ start: start, destination: destination, algorithm: algorithm }),
    });
    const data = await response.json();
    if (!response.ok) throw new Error(data.error);
    return data;
}

async function loadMap() {
    try {
        const response = await fetch("/api/graph");
        const data = await response.json();
        nodes = data.nodes;
        roads = data.roads;
    } catch (error) {
        message.textContent = "Cannot reach the C++ server. Is it running?";
        return;
    }
    fillSelect(startSelect);
    fillSelect(destinationSelect);
    drawMap();
}

// ---------- drawing the map ----------

function fillSelect(select) {
    select.innerHTML = '<option value="">choose...</option>';
    for (const node of nodes) {
        select.innerHTML += `<option value="${node.id}">${node.id} - ${node.name}</option>`;
    }
}

function svgElement(tag, attributes) {
    const element = document.createElementNS("http://www.w3.org/2000/svg", tag);
    for (const name in attributes) element.setAttribute(name, attributes[name]);
    return element;
}

function drawMap() {
    map.innerHTML = "";
    map.setAttribute("viewBox", "20 -20 910 620");  // fits the sample city

    for (const road of roads) {
        const a = nodes[road.from];
        const b = nodes[road.to];
        const line = svgElement("line", { x1: a.x, y1: a.y, x2: b.x, y2: b.y, class: "road", pathLength: 1 });
        line.id = `road-${road.from}-${road.to}`;
        map.appendChild(line);

    }

    // road lengths are drawn after all roads, so a thick path line never hides them
    for (const road of roads) {
        const a = nodes[road.from];
        const b = nodes[road.to];
        const label = svgElement("text", { x: (a.x + b.x) / 2, y: (a.y + b.y) / 2 + 4, class: "weight" });
        label.textContent = road.weight;
        map.appendChild(label);
    }

    for (const node of nodes) {
        const group = svgElement("g", { class: "node" });
        group.id = `node-${node.id}`;
        group.appendChild(svgElement("circle", { cx: node.x, cy: node.y, r: 15 }));

        const idText = svgElement("text", { x: node.x, y: node.y + 4, class: "id" });
        idText.textContent = node.id;
        group.appendChild(idText);

        const nameText = svgElement("text", { x: node.x, y: node.y + 30 });
        nameText.textContent = node.name;
        group.appendChild(nameText);

        group.addEventListener("click", () => clickNode(node.id));
        map.appendChild(group);
    }
    colorStartAndDestination();
}

function colorStartAndDestination() {
    for (const node of nodes) {
        const group = document.getElementById(`node-${node.id}`);
        group.classList.toggle("start", node.id === startId);
        group.classList.toggle("destination", node.id === destinationId);
    }
}

function clearRoute() {
    animationId++;  // stops any running animation
    for (const node of nodes) document.getElementById(`node-${node.id}`).classList.remove("explored", "onpath");
    for (const line of document.querySelectorAll(".road")) line.classList.remove("path");
}

// Show what the server found: explored nodes first, then the final path.
function showRoute(result) {
    clearRoute();
    const myAnimation = animationId;
    let step = 0;

    function nextStep() {
        if (myAnimation !== animationId) return;  // a newer animation started

        if (step < result.exploredOrder.length) {
            document.getElementById(`node-${result.exploredOrder[step]}`).classList.add("explored");
            step++;
            setTimeout(nextStep, 150);  // only used to slow down the animation
        } else {
            // draw the final path (a road may be stored as a-b or b-a)
            for (let i = 0; i + 1 < result.path.length; i++) {
                const a = result.path[i], b = result.path[i + 1];
                const line = document.getElementById(`road-${a}-${b}`) || document.getElementById(`road-${b}-${a}`);
                if (line) {
                    line.style.animationDelay = i * 0.3 + "s";  // roads appear one after another
                    line.classList.add("path");
                }
            }
            for (const id of result.path) document.getElementById(`node-${id}`).classList.add("onpath");
        }
    }
    nextStep();
}

// ---------- choosing start and destination ----------

function setSelection(start, destination) {
    startId = start;
    destinationId = destination;
    startSelect.value = start === null ? "" : start;
    destinationSelect.value = destination === null ? "" : destination;
    colorStartAndDestination();
    clearRoute();
    resultBox.innerHTML = "";
    message.textContent = "";
}

function clickNode(id) {
    if (startId === null) setSelection(id, null);
    else if (destinationId === null) setSelection(startId, id);
    else setSelection(id, null);  // third click starts again
}

startSelect.onchange = () => setSelection(startSelect.value === "" ? null : Number(startSelect.value), destinationId);
destinationSelect.onchange = () => setSelection(startId, destinationSelect.value === "" ? null : Number(destinationSelect.value));

// ---------- results ----------

function pathText(result) {
    if (!result.reachable) return "no route";
    return result.path.map((id) => nodes[id].name).join(" → ");
}

function resultCard(result) {
    const name = result.algorithm === "astar" ? "A*" : "Dijkstra";
    return `<div class="card">
        <span class="badge ${result.algorithm}">${name}</span>
        <div class="stats">
            <div class="stat"><small>Route exists</small><b>${result.reachable ? "Yes" : "No"}</b></div>
            <div class="stat"><small>Distance</small><b>${result.reachable ? result.distance : "-"}</b></div>
            <div class="stat"><small>Nodes explored</small><b>${result.nodesExplored}</b></div>
            <div class="stat"><small>C++ time</small><b>${result.executionTimeMs.toFixed(4)} ms</b></div>
        </div>
        <p class="path-text"><b>Path:</b> ${pathText(result)}</p>
    </div>`;
}

function checkSelection() {
    if (startId === null || destinationId === null) {
        message.textContent = "Please choose a start and a destination.";
        return false;
    }
    message.textContent = "";
    return true;
}

// ---------- buttons ----------

document.getElementById("findBtn").onclick = async () => {
    if (!checkSelection()) return;
    try {
        const result = await askServerForRoute(startId, destinationId, algorithmSelect.value);
        resultBox.innerHTML = `<div class="cards">${resultCard(result)}</div>`;
        if (!result.reachable) message.textContent = "There is no road connection between these two places.";
        showRoute(result);
    } catch (error) {
        message.textContent = error.message;
    }
};

document.getElementById("compareBtn").onclick = async () => {
    if (!checkSelection()) return;
    try {
        const dijkstra = await askServerForRoute(startId, destinationId, "dijkstra");
        const astar = await askServerForRoute(startId, destinationId, "astar");
        resultBox.innerHTML =
            `<div class="cards">${resultCard(dijkstra)}${resultCard(astar)}</div>
             <p class="note">Note: the time is measured once on a very small map, so it is not reliable.
             Do not use it to say one algorithm is always faster. Compare the number of explored nodes and the distance instead.</p>`;
        showRoute(algorithmSelect.value === "astar" ? astar : dijkstra);
    } catch (error) {
        message.textContent = error.message;
    }
};

document.getElementById("resetBtn").onclick = () => setSelection(null, null);

loadMap();
