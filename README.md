# Webalapú gyártósor-felügyeleti rendszer ESP32-vel

## 1. A projekt bemutatása

A projekt egy ESP32-S3 mikrokontrollerre épülő, webalapú gyártósor-felügyeleti rendszer, amely egy ipari gyártósor alapvető működését szimulálja.

A rendszer célja a gyártási folyamat állapotának nyomon követése, a termelési adatok kezelése és megjelenítése, valamint a gyártósor webes felületen keresztüli vezérlése.

A megvalósítás az ESP32-S3 mikrokontrollert, az MQTT-alapú kommunikációt, a Node.js háttérprogramot, a Supabase adatbázist és a HTML, CSS, JavaScript technológiákra épülő webes felületet kapcsolja össze.

A projekt mérnökinformatikus szakdolgozat keretében készült.

## 2. A rendszer felépítése

A rendszer működése több egymáshoz kapcsolódó komponensre épül:

- **ESP32-S3:** a gyártósor működésének és a termelési folyamatnak a szimulációja.
- **Mosquitto:** MQTT broker, amely biztosítja az eszközök közötti üzenetváltást.
- **Node.js:** a háttérprogram, amely feldolgozza az MQTT-üzeneteket, kezeli az API-végpontokat és az adatbázis-kapcsolatot.
- **Supabase:** a termelési adatok tárolására és lekérdezésére szolgáló adatbázis.
- **Webes felület:** a gépállapotok, termelési adatok és események megjelenítése, valamint a vezérlőparancsok küldése.

### Adatkommunikáció

Az ESP32-S3 MQTT-protokollon keresztül továbbítja a gyártósor állapotát és a termelési adatokat. A Node.js háttérprogram fogadja és feldolgozza ezeket az üzeneteket, majd az adatokat elmenti a Supabase adatbázisba.

A webes felület a háttérprogram API-végpontján keresztül kéri le az aktuális állapotot. A felhasználó által kiadott vezérlőparancsok a háttérprogramon keresztül, MQTT-üzenetként jutnak el az ESP32-höz.

## 3. Megvalósított funkciók

A rendszer az alábbi funkciókat valósítja meg:

- A gyártósor működésének szimulációja.
- START, STOP, RESET és vészleállítás kezelése.
- A gépállapotok jelzése LED-ekkel.
- A termelési folyamat és a darabszám növekedésének szimulációja.
- Megfelelő és selejtes termékek nyilvántartása.
- A termelési ciklusidők mérése.
- MQTT-alapú adatkommunikáció.
- A gépállapotok és termelési adatok továbbítása a háttérrendszerbe.
- Termelési adatok mentése Supabase adatbázisba.
- Webes felügyeleti és vezérlőfelület.
- Termelési adatok és eseménynapló megjelenítése.
- Webes vezérlőparancsok továbbítása a mikrokontrollernek.

**Fontos:** a projekt szimulációs prototípus. A termelési folyamat és a termékek minősítése programozott módon történik, nem fizikai gyártógép vagy külső érzékelők alapján. A vészleállítás szoftveres funkció, nem helyettesít ipari biztonsági vészleállító rendszert.

## 4. Felhasznált hardverelemek

- ESP32-S3-DEV-N16R8 fejlesztőpanel
- Próbapanel
- Zöld, sárga és piros LED
- 220 Ω-os ellenállások
- START nyomógomb
- STOP nyomógomb
- RESET nyomógomb
- Vészleállító nyomógomb
- USB-C adatkábel

### GPIO-kiosztás

| Elem | ESP32-S3 GPIO |
|---|---|
| Zöld LED – RUNNING | GPIO17 |
| Sárga LED – STOPPED | GPIO16 |
| Piros LED – EMERGENCY_STOP | GPIO4 |
| START nyomógomb | GPIO15 |
| RESET nyomógomb | GPIO7 |
| STOP nyomógomb | GPIO6 |
| Vészleállító nyomógomb | GPIO5 |

A nyomógombok belső felhúzóellenállással (`INPUT_PULLUP`) működnek.

## 5. Alkalmazott technológiák

| Technológia | Felhasználási terület |
|---|---|
| C++ / Arduino | ESP32-S3 vezérlőprogram |
| Arduino IDE | Mikrokontroller programozása |
| Wi-Fi | Vezeték nélküli kommunikáció |
| MQTT | Üzenetküldés az eszközök között |
| Eclipse Mosquitto | MQTT broker |
| Node.js | Háttérprogram |
| JavaScript | Backend és frontend működése |
| HTML | Webes felület szerkezete |
| CSS | Webes felület megjelenítése |
| Supabase | Adattárolás és Realtime |
| PostgreSQL | Relációs adatbázis |
| Git / GitHub | Verziókövetés és forráskód tárolása |

## 6. MQTT témakörök

A rendszer az alábbi MQTT-témaköröket használja:

| Témakör | Feladat |
|---|---|
| `gyartosor/esp32/status` | Gépállapot és termelési adatok továbbítása |
| `gyartosor/esp32/event` | Termelési események továbbítása |
| `gyartosor/esp32/command` | Vezérlőparancsok fogadása |

## 7. Adatbázis

A rendszer a Supabase által biztosított PostgreSQL adatbázist használja. A termelési adatok a `production_logs` táblában kerülnek tárolásra.

A tábla főbb mezői:

- `id`
- `machine_status`
- `total_produced`
- `good_products`
- `defective_products`
- `last_cycle_time_ms`
- `average_cycle_time_ms`
- `cycle_count`
- `recorded_at`

Az adatbázis a termelési adatok későbbi lekérdezését és a webes felületen történő megjelenítését biztosítja.

## 8. Webes felület és API

A webes felület lehetővé teszi a gyártósor állapotának távoli megfigyelését és vezérlését.

A felületen megjelennek:
- Az aktuális gépállapot.
- A legyártott termékek száma.
- A megfelelő és selejtes termékek száma.
- A termelési ciklusidőkkel kapcsolatos adatok.
- A termelési események és a gyártási napló.

A háttérprogram főbb API-végpontjai:

| HTTP metódus | Végpont | Feladat |
|---|---|---|
| GET | `/api/status` | Aktuális gépállapot lekérdezése |
| POST | `/api/command` | Vezérlőparancs továbbítása |

A webes felület az aktuális állapotot rendszeres lekérdezéssel frissíti, a Supabase Realtime pedig az új adatbázis-bejegyzések követését támogatja.

## 9. Projekt könyvtárszerkezete

A repository főbb elemei:

```text
GyartosorSzimulacio/
├── backend/
│   ├── public/
│   │   ├── index.html
│   │   ├── style.css
│   │   └── app.js
│   └── index.js
├── docs/
├── Gyartosor_Szimulacio.ino
├── README.md
└── .gitignore
```

- `Gyartosor_Szimulacio.ino` – az ESP32-S3 vezérlőprogramja.
- `backend/index.js` – Node.js háttérprogram.
- `backend/public/` – a webes felület fájljai.
- `docs/` – projekt dokumentációja.
- `.gitignore` – a verziókövetésből kizárt fájlok és beállítások.
- `README.md` – a projekt bemutatása és műszaki áttekintése.

## 10. Fejlesztési környezet

A projekt elkészítéséhez használt főbb fejlesztőeszközök:

- Arduino IDE
- Visual Studio Code
- Node.js
- Eclipse Mosquitto
- Supabase
- Git és GitHub

A backend működéséhez szükséges kapcsolati és hozzáférési adatokat környezeti változók tárolják. A titkos adatokat tartalmazó `.env` fájl nem kerül verziókövetésbe.

## 11. Összefoglalás

A megvalósított projekt egy ESP32-S3 alapú, webes felügyeleti rendszer prototípusa, amely egyesíti a mikrokontroller-programozást, az IoT-kommunikációt, a háttérprogram-fejlesztést és az adatbázis-kezelést.

A rendszer lehetőséget biztosít a szimulált gyártási folyamat állapotának megfigyelésére, a termelési adatok tárolására és a gyártósor webes felületen keresztüli vezérlésére.