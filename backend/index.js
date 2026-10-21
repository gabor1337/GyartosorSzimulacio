
require("dotenv").config();

const mqtt = require("mqtt");
const { createClient } = require("@supabase/supabase-js");

const supabaseUrl = process.env.SUPABASE_URL;
const supabaseSecretKey = process.env.SUPABASE_SECRET_KEY;

if (!supabaseUrl || !supabaseSecretKey) {
  console.error("HIBA: Hianyzik a Supabase URL vagy Secret Key a .env fajlbol.");
  process.exit(1);
}

const supabase = createClient(supabaseUrl, supabaseSecretKey);

const MQTT_SERVER = "192.168.100.124";

const client = mqtt.connect(`mqtt://${MQTT_SERVER}:1883`);

client.on("connect", () => {
  console.log("Sikeres MQTT-kapcsolat!");

  client.subscribe(
    ["gyartosor/esp32/status", "gyartosor/esp32/event"],
    (err) => {
      if (err) {
        console.error("Feliratkozasi hiba:", err.message);
      } else {
        console.log("Feliratkozva az ESP32 temakorokre.");
      }
    }
  );
});

client.on("message", async (topic, message) => {
  const content = message.toString();

  console.log("\nUj MQTT-uzenet erkezett!");
  console.log("Temakor:", topic);

  let data;

  try {
    data = JSON.parse(content);
    console.log("Adatok:", data);
  } catch {
    console.log("Uzenet:", content);
    return;
  }

  // Csak az allapotjelenteseket mentjuk az adatbazisba.
  if (topic === "gyartosor/esp32/status") {
    const { error } = await supabase
      .from("production_logs")
      .insert({
        machine_status: data.status,
        total_produced: data.totalProduced
      });

    if (error) {
      console.error("Supabase mentesi hiba:", error.message);
    } else {
      console.log("Az allapot sikeresen elmentve a Supabase-be.");
    }
  }
});

client.on("error", (err) => {
  console.error("MQTT-hiba:", err.message);
});

client.on("reconnect", () => {
  console.log("Ujracsatlakozas az MQTT-brokerhez...");
});
