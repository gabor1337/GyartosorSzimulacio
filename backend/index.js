require("dotenv").config();

const http = require("http");
const mqtt = require("mqtt");
const path = require("path");
const fs = require("fs");
const { createClient } = require("@supabase/supabase-js");

// Supabase konfiguráció
const supabaseUrl = process.env.SUPABASE_URL;
const supabaseSecretKey = process.env.SUPABASE_SECRET_KEY;

if (!supabaseUrl || !supabaseSecretKey) {
  console.error("HIBA: Hiányzik a Supabase URL vagy Secret Key a .env fájlból.");
  process.exit(1);
}

const supabase = createClient(supabaseUrl, supabaseSecretKey);

// MQTT konfiguráció
const MQTT_SERVER = "192.168.100.124";
const MQTT_PORT = 1883;

const TOPIC_STATUS = "gyartosor/esp32/status";
const TOPIC_EVENT = "gyartosor/esp32/event";
const TOPIC_COMMAND = "gyartosor/esp32/command";

// Webszerver konfiguráció
const WEB_PORT = 3000;
const PUBLIC_FOLDER = path.join(__dirname, "public");

const client = mqtt.connect(`mqtt://${MQTT_SERVER}:${MQTT_PORT}`);

let latestStatus = null;
let latestEvent = null;
let lastSeenAt = null;

// MQTT kapcsolat
client.on("connect", () => {
  console.log("Sikeres MQTT-kapcsolat!");

  client.subscribe([TOPIC_STATUS, TOPIC_EVENT], (err) => {
    if (err) {
      console.error("Feliratkozási hiba:", err.message);
    } else {
      console.log("Feliratkozva az ESP32 státusz- és eseménytémaköreire.");
    }
  });
});

// MQTT üzenetek fogadása
client.on("message", async (topic, message) => {
  const content = message.toString();

  console.log("\nÚj MQTT-üzenet érkezett!");
  console.log("Témakör:", topic);

  let data;

  try {
    data = JSON.parse(content);
    console.log("Adatok:", data);
  } catch {
    console.log("Érvénytelen JSON-üzenet:", content);
    return;
  }

  lastSeenAt = new Date().toISOString();

  // Gépállapot és termelési adatok mentése
  if (topic === TOPIC_STATUS) {
    latestStatus = data;

    const { error } = await supabase
      .from("production_logs")
      .insert({
        machine_status: data.status,
        total_produced: data.totalProduced ?? 0,
        good_products: data.goodProducts ?? 0,
        defective_products: data.defectiveProducts ?? 0,

        last_cycle_time_ms: data.lastCycleTimeMs ?? 0,
        average_cycle_time_ms: data.averageCycleTimeMs ?? 0,
        cycle_count: data.cycleCount ?? 0
      });

    if (error) {
      console.error("Supabase mentési hiba:", error.message);
    } else {
      console.log("A termelési adatok sikeresen elmentve a Supabase-be.");
      console.log("Összes termék:", data.totalProduced);
      console.log("Jó termékek:", data.goodProducts);
      console.log("Selejtes termékek:", data.defectiveProducts);
      console.log("Utolsó ciklusidő:", data.lastCycleTimeMs, "ms");
      console.log("Átlagos ciklusidő:", data.averageCycleTimeMs, "ms");
      console.log("Ciklusok száma:", data.cycleCount);
    }
  }

  // Eseményadatok eltárolása
  if (topic === TOPIC_EVENT) {
    latestEvent = data;
    console.log("Új gyártási esemény:", data.event);
  }
});

// MQTT hibakezelés
client.on("error", (err) => {
  console.error("MQTT-hiba:", err.message);
});

client.on("reconnect", () => {
  console.log("Újracsatlakozás az MQTT-brokerhez...");
});

// JSON válasz küldése
function sendJson(response, statusCode, data) {
  response.writeHead(statusCode, {
    "Content-Type": "application/json; charset=utf-8",
    "Cache-Control": "no-store"
  });

  response.end(JSON.stringify(data));
}

// Webes fájlok kiszolgálása
function serveStaticFile(request, response) {
  let requestedPath;

  try {
    requestedPath = request.url === "/"
      ? "index.html"
      : decodeURIComponent(request.url.split("?")[0].slice(1));
  } catch {
    return sendJson(response, 400, {
      error: "Érvénytelen fájlútvonal."
    });
  }

  const filePath = path.resolve(PUBLIC_FOLDER, requestedPath);

  if (!filePath.startsWith(PUBLIC_FOLDER + path.sep)) {
    return sendJson(response, 403, {
      error: "Tiltott elérési út."
    });
  }

  fs.readFile(filePath, (err, content) => {
    if (err) {
      return sendJson(response, 404, {
        error: "A kért fájl nem található."
      });
    }

    const extension = path.extname(filePath);

    const contentTypes = {
      ".html": "text/html; charset=utf-8",
      ".css": "text/css; charset=utf-8",
      ".js": "application/javascript; charset=utf-8",
      ".json": "application/json; charset=utf-8",
      ".png": "image/png",
      ".jpg": "image/jpeg",
      ".svg": "image/svg+xml"
    };

    response.writeHead(200, {
      "Content-Type": contentTypes[extension] || "application/octet-stream"
    });

    response.end(content);
  });
}

// HTTP szerver
const server = http.createServer((request, response) => {
  const url = new URL(request.url, `http://${request.headers.host}`);

  // Gépállapot lekérdezése
  if (request.method === "GET" && url.pathname === "/api/status") {
    return sendJson(response, 200, {
      connected: client.connected,
      status: latestStatus,
      lastEvent: latestEvent,
      lastSeenAt
    });
  }

  // Vezérlőparancs fogadása
  if (request.method === "POST" && url.pathname === "/api/command") {
    let body = "";

    request.on("data", (chunk) => {
      body += chunk;

      if (body.length > 1024) {
        request.destroy();
      }
    });

    request.on("end", () => {
      let data;

      try {
        data = JSON.parse(body);
      } catch {
        return sendJson(response, 400, {
          error: "Érvénytelen JSON-kérés."
        });
      }

      const allowedCommands = [
        "START",
        "STOP",
        "RESET",
        "EMERGENCY_STOP"
      ];

      const command = data.command;

      if (!allowedCommands.includes(command)) {
        return sendJson(response, 400, {
          error: "Ismeretlen parancs."
        });
      }

      if (!client.connected) {
        return sendJson(response, 503, {
          error: "Az MQTT-broker nem érhető el."
        });
      }

      // Parancs továbbítása az ESP32 felé
      client.publish(
        TOPIC_COMMAND,
        JSON.stringify({ command }),
        { qos: 1 },
        (err) => {
          if (err) {
            console.error("Parancsküldési hiba:", err.message);

            return sendJson(response, 502, {
              error: "A parancs továbbítása sikertelen."
            });
          }

          console.log(`Webes parancs továbbítva: ${command}`);

          return sendJson(response, 200, {
            success: true,
            command,
            topic: TOPIC_COMMAND
          });
        }
      );
    });

    return;
  }

  // Weboldal és statikus fájlok
  if (request.method === "GET") {
    return serveStaticFile(request, response);
  }

  sendJson(response, 404, {
    error: "Nem található végpont."
  });
});

// Szerver indítása
server.listen(WEB_PORT, "127.0.0.1", () => {
  console.log(`Weboldal elérhető: http://localhost:${WEB_PORT}`);
});