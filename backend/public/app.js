
const SUPABASE_URL = "https://supabase.com/dashboard/project/ivgmosxahfpxetlndlfk";
const SUPABASE_ANON_KEY = "sb_publishable_CeXsLd1vg33iju9VqNEFwg_5fhhA543";

// Supabase kapcsolat
const supabaseClient = window.supabase.createClient(
    SUPABASE_URL,
    SUPABASE_ANON_KEY
);

// HTML elemek
const machineStatus = document.getElementById("machineStatus");
const productionCount = document.getElementById("productionCount");
const statusDescription = document.getElementById("statusDescription");

const connectionIndicator = document.getElementById("connectionIndicator");
const connectionText = document.getElementById("connectionText");

const apiStatus = document.getElementById("apiStatus");
const mqttStatus = document.getElementById("mqttStatus");

const lastUpdate = document.getElementById("lastUpdate");
const lastEvent = document.getElementById("lastEvent");
const lastEventTime = document.getElementById("lastEventTime");

const commandMessage = document.getElementById("commandMessage");

const buttons = {
    START: document.getElementById("startButton"),
    STOP: document.getElementById("stopButton"),
    RESET: document.getElementById("resetButton"),
    EMERGENCY_STOP: document.getElementById("emergencyButton")
};

// Gépállapot megjelenítése
function updateMachineStatus(data) {
    if (!data) {
        return;
    }

    const status = data.status;

    if (!status) {
        return;
    }

    machineStatus.textContent = status.status || "ISMERETLEN";
    productionCount.textContent = status.totalProduced ?? 0;

    machineStatus.className = "status-value";

    if (status.status === "RUNNING") {
        machineStatus.classList.add("running");
        statusDescription.textContent = "A gyártás folyamatban van.";
    } else if (status.status === "STOPPED") {
        machineStatus.classList.add("stopped");
        statusDescription.textContent = "A gyártósor jelenleg áll.";
    } else if (status.status === "EMERGENCY_STOP") {
        machineStatus.classList.add("emergency");
        statusDescription.textContent = "Vészleállítás aktív!";
    }

    if (data.lastEvent) {
        lastEvent.textContent =
            data.lastEvent.event || "Ismeretlen esemény";

        lastEventTime.textContent =
            data.lastSeenAt
                ? new Date(data.lastSeenAt).toLocaleString("hu-HU")
                : "-";
    }

    lastUpdate.textContent = new Date().toLocaleTimeString("hu-HU");
}

// Legutóbbi adat lekérése a Supabase-ből
async function fetchLatestProduction() {
    const { data, error } = await supabaseClient
        .from("production_logs")
        .select("machine_status, total_produced, recorded_at")
        .order("recorded_at", { ascending: false })
        .limit(1);

    if (error) {
        console.error("Supabase lekérdezési hiba:", error.message);
        return;
    }

    if (data && data.length > 0) {
        updateProductionFromDatabase(data[0]);
    }
}

// Új termelési adat megjelenítése
function updateProductionFromDatabase(record) {
    productionCount.textContent = record.total_produced ?? 0;

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

    lastUpdate.textContent =
        new Date(record.recorded_at).toLocaleTimeString("hu-HU");
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

                updateProductionFromDatabase(payload.new);
            }
        )
        .subscribe((status) => {
            console.log("Supabase Realtime állapot:", status);

            if (status === "SUBSCRIBED") {
                console.log("Supabase Realtime sikeresen csatlakozott.");
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
buttons.START.addEventListener("click", () => {
    sendCommand("START");
});

buttons.STOP.addEventListener("click", () => {
    sendCommand("STOP");
});

buttons.RESET.addEventListener("click", () => {
    sendCommand("RESET");
});

buttons.EMERGENCY_STOP.addEventListener("click", () => {
    sendCommand("EMERGENCY_STOP");
});

// Alkalmazás indítása
async function initializeApp() {
    await fetchLatestProduction();

    startRealtime();

    await fetchStatus();

    setInterval(fetchStatus, 1000);
}

initializeApp();
