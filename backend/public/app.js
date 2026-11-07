// Supabase konfiguráció
const SUPABASE_URL = "https://ivgmosxahfpxetlndlfk.supabase.co";
const SUPABASE_ANON_KEY = "sb_publishable_CeXsLd1vg33iju9VqNEFwg_5fhhA543";

const supabaseClient = window.supabase.createClient(
    SUPABASE_URL,
    SUPABASE_ANON_KEY
);

// HTML elemek
const machineStatus = document.getElementById("machineStatus");
const productionCount = document.getElementById("productionCount");
const goodProductsElement = document.getElementById("goodProducts");
const defectiveProductsElement = document.getElementById("defectiveProducts");
const lastCycleTimeElement = document.getElementById("lastCycleTime");
const averageCycleTimeElement = document.getElementById("averageCycleTime");
const cycleCountElement = document.getElementById("cycleCount");
const statusDescription = document.getElementById("statusDescription");

const connectionIndicator = document.getElementById("connectionIndicator");
const connectionText = document.getElementById("connectionText");
const apiStatus = document.getElementById("apiStatus");
const mqttStatus = document.getElementById("mqttStatus");

const lastUpdate = document.getElementById("lastUpdate");
const lastEvent = document.getElementById("lastEvent");
const lastEventTime = document.getElementById("lastEventTime");
const commandMessage = document.getElementById("commandMessage");

const goodBar = document.getElementById("goodBar");
const defectBar = document.getElementById("defectBar");
const goodPercentage = document.getElementById("goodPercentage");
const defectPercentage = document.getElementById("defectPercentage");

const logsTableBody = document.getElementById("logsTableBody");
const logsMessage = document.getElementById("logsMessage");
const refreshLogsButton = document.getElementById("refreshLogsButton");
const filterButtons = document.querySelectorAll(".filter-button");

const buttons = {
    START: document.getElementById("startButton"),
    STOP: document.getElementById("stopButton"),
    RESET: document.getElementById("resetButton"),
    EMERGENCY_STOP: document.getElementById("emergencyButton")
};

let productionLogs = [];
let activeLogFilter = "all";

function safeNumber(value) {
    const number = Number(value);
    return Number.isFinite(number) ? number : 0;
}

function formatCycleTime(value) {
    const milliseconds = safeNumber(value);

    if (milliseconds <= 0) {
        return "0 ms";
    }

    if (milliseconds >= 1000) {
        return `${(milliseconds / 1000).toFixed(2)} s`;
    }

    return `${milliseconds.toFixed(0)} ms`;
}

// Termelési mutatók frissítése
function updateProductionStats(data) {
    const total = safeNumber(data.totalProduced ?? data.total_produced);
    const good = safeNumber(data.goodProducts ?? data.good_products);
    const defective = safeNumber(data.defectiveProducts ?? data.defective_products);
    const lastCycleTime = safeNumber(data.lastCycleTimeMs ?? data.last_cycle_time_ms);
    const averageCycleTime = safeNumber(data.averageCycleTimeMs ?? data.average_cycle_time_ms);
    const cycleCount = safeNumber(data.cycleCount ?? data.cycle_count);

    lastCycleTimeElement.textContent = formatCycleTime(lastCycleTime);
    averageCycleTimeElement.textContent = formatCycleTime(averageCycleTime);
    cycleCountElement.textContent = cycleCount;

    productionCount.textContent = total;
    goodProductsElement.textContent = good;
    defectiveProductsElement.textContent = defective;

    const qualityTotal = good + defective;
    const goodRate = qualityTotal > 0 ? (good / qualityTotal) * 100 : 0;
    const defectRate = qualityTotal > 0 ? (defective / qualityTotal) * 100 : 0;

    goodBar.style.width = `${goodRate}%`;
    defectBar.style.width = `${defectRate}%`;

    goodPercentage.textContent = `Jó termékek: ${goodRate.toFixed(1)}%`;
    defectPercentage.textContent = `Selejtarány: ${defectRate.toFixed(1)}%`;
}

// Gépállapot megjelenítése
function updateMachineStatus(data) {
    if (!data || !data.status) return;

    const status = data.status.status;
    machineStatus.textContent = status || "ISMERETLEN";
    machineStatus.className = "status-value";

    if (status === "RUNNING") {
        machineStatus.classList.add("running");
        statusDescription.textContent = "A gyártás folyamatban van.";
    } else if (status === "STOPPED") {
        machineStatus.classList.add("stopped");
        statusDescription.textContent = "A gyártósor jelenleg áll.";
    } else if (status === "EMERGENCY_STOP") {
        machineStatus.classList.add("emergency");
        statusDescription.textContent = "Vészleállítás aktív!";
    } else {
        statusDescription.textContent = "A gép állapota frissült.";
    }

    updateProductionStats(data.status);

    if (data.lastEvent) {
        lastEvent.textContent = data.lastEvent.event || "Ismeretlen esemény";
        lastEventTime.textContent = data.lastSeenAt
            ? new Date(data.lastSeenAt).toLocaleString("hu-HU")
            : "-";
    }

    lastUpdate.textContent = new Date().toLocaleTimeString("hu-HU");
}

// Supabase rekord megjelenítése
function updateProductionFromDatabase(record) {
    if (!record) return;

    productionCount.textContent = safeNumber(record.total_produced);
    goodProductsElement.textContent = safeNumber(record.good_products);
    defectiveProductsElement.textContent = safeNumber(record.defective_products);

    const status = record.machine_status;

    if (status) {
        machineStatus.textContent = status;
        machineStatus.className = "status-value";

        if (status === "RUNNING") {
            machineStatus.classList.add("running");
            statusDescription.textContent = "A gyártás folyamatban van.";
        } else if (status === "STOPPED") {
            machineStatus.classList.add("stopped");
            statusDescription.textContent = "A gyártósor jelenleg áll.";
        } else if (status === "EMERGENCY_STOP") {
            machineStatus.classList.add("emergency");
            statusDescription.textContent = "Vészleállítás aktív!";
        }
    }

    updateProductionStats(record);
    lastUpdate.textContent = new Date(record.recorded_at).toLocaleTimeString("hu-HU");
}

// Naplóesemény meghatározása
function getLogEvent(current, previous) {
    if (previous) {
        const currentDefects = safeNumber(current.defective_products);
        const previousDefects = safeNumber(previous.defective_products);
        const currentGood = safeNumber(current.good_products);
        const previousGood = safeNumber(previous.good_products);

        if (currentDefects > previousDefects) {
            return { label: "Selejtes termék", type: "defect" };
        }

        if (currentGood > previousGood) {
            return { label: "Jó termék", type: "good" };
        }
    }

    if (current.machine_status === "EMERGENCY_STOP") {
        return { label: "Vészleállítás", type: "defect" };
    }

    return { label: "Állapotfrissítés", type: "status" };
}

// Napló megjelenítése
function renderLogs() {
    const chronological = [...productionLogs].reverse();

    const rows = chronological.map((record, index) => {
        const previous = index > 0 ? chronological[index - 1] : null;

        return {
            record,
            event: getLogEvent(record, previous)
        };
    }).reverse();

    const filteredRows = activeLogFilter === "all"
        ? rows
        : rows.filter(item => item.event.type === activeLogFilter);

    logsTableBody.innerHTML = "";

    if (filteredRows.length === 0) {
        logsTableBody.innerHTML =
            '<tr><td colspan="7" class="empty-log">Nincs megjeleníthető naplóbejegyzés.</td></tr>';

        logsMessage.textContent = productionLogs.length
            ? `${productionLogs.length} rekord betöltve.`
            : "Még nem érkezett termelési adat.";

        return;
    }

    filteredRows.forEach(({ record, event }) => {
        const row = document.createElement("tr");

        const date = record.recorded_at
            ? new Date(record.recorded_at).toLocaleString("hu-HU")
            : "-";

        const eventClass = event.type === "good"
            ? "log-good"
            : event.type === "defect"
                ? "log-defect"
                : "log-status";

        row.innerHTML = `
            <td>${date}</td>
            <td><span class="log-tag ${eventClass}">${event.label}</span></td>
            <td>${record.machine_status || "-"}</td>
            <td>${safeNumber(record.total_produced)}</td>
            <td>${safeNumber(record.good_products)}</td>
            <td>${safeNumber(record.defective_products)}</td>
            <td>${formatCycleTime(record.last_cycle_time_ms)}</td>
        `;

        logsTableBody.appendChild(row);
    });

    logsMessage.textContent =
        `${filteredRows.length} naplóbejegyzés megjelenítve.`;
}

// Naplóadatok lekérése
async function fetchProductionLogs() {
    logsMessage.textContent = "Naplóadatok betöltése...";

    const { data, error } = await supabaseClient
        .from("production_logs")
        .select("machine_status, total_produced, good_products, defective_products, last_cycle_time_ms, average_cycle_time_ms, cycle_count, recorded_at")
        .order("recorded_at", { ascending: false })
        .limit(100);

    if (error) {
        console.error("Napló lekérdezési hiba:", error.message);

        logsMessage.textContent =
            `Naplóbetöltési hiba: ${error.message}`;

        logsTableBody.innerHTML =
            '<tr><td colspan="7" class="empty-log">Nem sikerült betölteni a naplót.</td></tr>';

        return;
    }

    productionLogs = data || [];
    renderLogs();

    if (productionLogs.length > 0) {
        updateProductionFromDatabase(productionLogs[0]);
    }
}

// Supabase Realtime feliratkozás
function startRealtime() {
    supabaseClient
        .channel("production-logs-changes")
        .on(
            "postgres_changes",
            {
                event: "INSERT",
                schema: "public",
                table: "production_logs"
            },
            (payload) => {
                console.log("Új Supabase rekord:", payload.new);

                productionLogs.unshift(payload.new);
                productionLogs = productionLogs.slice(0, 100);

                updateProductionFromDatabase(payload.new);
                renderLogs();
            }
        )
        .subscribe((status, error) => {
            console.log("Supabase Realtime állapot:", status);

            if (status === "SUBSCRIBED") {
                console.log("Supabase Realtime sikeresen csatlakozott.");
            } else if (status === "CHANNEL_ERROR" || status === "TIMED_OUT") {
                console.error("Supabase Realtime kapcsolódási hiba:", error);
            }
        });
}

// API állapotának frissítése
async function fetchStatus() {
    try {
        const response = await fetch("/api/status");

        if (!response.ok) {
            throw new Error("API-hiba");
        }

        const data = await response.json();

        connectionIndicator.className = "indicator online";
        connectionText.textContent = "API elérhető";
        apiStatus.textContent = "Kapcsolódva";

        if (data.connected) {
            mqttStatus.textContent = "Kapcsolódva";
            mqttStatus.style.color = "#43b581";
        } else {
            mqttStatus.textContent = "Nincs kapcsolat";
            mqttStatus.style.color = "#e05252";
        }

        updateMachineStatus(data);

    } catch (error) {
        connectionIndicator.className = "indicator offline";
        connectionText.textContent = "Kapcsolat megszakadt";
        apiStatus.textContent = "Nem elérhető";
        mqttStatus.textContent = "Ismeretlen";
    }
}

// Vezérlőparancs küldése
async function sendCommand(command) {
    commandMessage.textContent = `${command} parancs küldése...`;

    Object.values(buttons).forEach(button => {
        button.disabled = true;
    });

    try {
        const response = await fetch("/api/command", {
            method: "POST",
            headers: {
                "Content-Type": "application/json"
            },
            body: JSON.stringify({ command })
        });

        const result = await response.json();

        if (!response.ok) {
            throw new Error(result.error || "Parancsküldési hiba");
        }

        commandMessage.textContent =
            `${command} parancs továbbítva az ESP32 felé.`;

    } catch (error) {
        commandMessage.textContent = `Hiba: ${error.message}`;
    } finally {
        Object.values(buttons).forEach(button => {
            button.disabled = false;
        });
    }
}

// Vezérlőgombok eseménykezelése
buttons.START.addEventListener("click", () => sendCommand("START"));
buttons.STOP.addEventListener("click", () => sendCommand("STOP"));
buttons.RESET.addEventListener("click", () => sendCommand("RESET"));
buttons.EMERGENCY_STOP.addEventListener("click", () => sendCommand("EMERGENCY_STOP"));

// Napló frissítése
refreshLogsButton.addEventListener("click", fetchProductionLogs);

// Napló szűrése
filterButtons.forEach(button => {
    button.addEventListener("click", () => {
        activeLogFilter = button.dataset.filter;

        filterButtons.forEach(item => {
            item.classList.toggle("active", item === button);
        });

        renderLogs();
    });
});

// Alkalmazás indítása
async function initializeApp() {
    await fetchProductionLogs();

    startRealtime();

    await fetchStatus();

    setInterval(fetchStatus, 1000);
}

initializeApp();