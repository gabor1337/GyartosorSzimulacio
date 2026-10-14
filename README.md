# Webalapú gyártósor-felügyeleti rendszer ESP32-vel

## 1. A projekt bemutatása

A projekt célja egy webalapú gyártósor-felügyeleti rendszer prototípusának elkészítése, amely egy ESP32 mikrokontroller segítségével szimulálja egy ipari gyártósor alapvető működését.

A rendszer hosszabb távú célja, hogy lehetővé tegye a gyártási folyamat állapotának távoli megfigyelését, a termelési adatok nyomon követését és a hibák jelzését egy webes felületen keresztül.

A fejlesztés egy mérnökinformatikus szakdolgozat része, amely a mikrokontroller-programozást, az IoT-kommunikációt, az adatkezelést és a webfejlesztést kapcsolja össze.

## 2. Jelenlegi funkciók

A projekt jelenlegi változata az alábbi funkciókat valósítja meg:

- A gyártósor állapotának szimulálása.
- A gyártósor indítása és leállítása nyomógombokkal.
- Vészleállítás és visszaállítás kezelése.
- A gyártósor állapotának jelzése LED-ekkel.
- A legyártott darabok számának szimulált nyilvántartása.
- A működési állapot és a termelési adatok megjelenítése a soros monitoron.
- A nyomógombok pergésének szoftveres kezelése.

A jelenlegi prototípusban a termelési folyamat és a darabszám növekedése szimulált, nem tényleges gyártógépről vagy érzékelőről származik.

## 3. Felhasznált hardverelemek

- ESP32-S3-DEV-N16R8 mikrokontroller
- Próbapanel
- Zöld, sárga és piros LED
- 220 Ω-os ellenállások
- START nyomógomb
- STOP nyomógomb
- RESET nyomógomb
- VÉSZLEÁLLÍTÁS nyomógomb
- USB-C adatkábel

### GPIO-kiosztás

| Elem | ESP32 GPIO |
|---|---:|
| Zöld LED – működés | GPIO17 |
| Sárga LED – leállított állapot | GPIO16 |
| Piros LED – vészleállítás | GPIO4 |
| START gomb | GPIO15 |
| RESET gomb | GPIO7 |
| STOP gomb | GPIO6 |
| VÉSZLEÁLLÍTÁS gomb | GPIO5 |

A nyomógombok belső felhúzóellenállással (`INPUT_PULLUP`) működnek. A LED-ek áramkorlátozását 220 Ω-os ellenállások biztosítják.

**Fontos:** a prototípus vészleállítása kizárólag szoftveres szimuláció. Nem helyettesít ipari környezetben alkalmazott, biztonsági szempontból megfelelően kialakított vészleállító áramkört.

## 4. Tervezett továbbfejlesztések

A projekt későbbi fejlesztési szakaszaiban a következő funkciók megvalósítása a cél:

- Wi-Fi-kommunikáció az ESP32 és a háttérrendszer között.
- MQTT-protokoll használata az adatok továbbítására.
- Háttéralkalmazás és adatbázis kialakítása.
- Webes kezelőfelület létrehozása a gyártósor állapotának megjelenítésére.
- Termelési adatok és események tárolása.
- Grafikonok és statisztikák készítése.
- A rendelkezésre álló adatok alapján az OEE (Overall Equipment Effectiveness) mutató számításának előkészítése.

Ezek a funkciók a fejlesztés következő szakaszainak részét képezik, ezért nem tekinthetők a jelenlegi változatban már megvalósítottnak.

## 5. A projekt felépítése

A repository tervezett könyvtárszerkezete:

```text
Gyartosor_Szimulacio/
├── GyartosorSzimulacio.ino
├── README.md
├── docs/
│   └── dokumentacio.md
└── images/
    ├── eszkoz-felulnezet.jpg
    └── bekotes.jpg
```

- `GyartosorSzimulacio.ino` – az ESP32 vezérlőprogramja.
- `README.md` – a projekt rövid bemutatása.
- `docs/dokumentacio.md` – a projekt részletes műszaki dokumentációja.
- `images/` – a hardverről és a bekötésről készült képek.

A mappaszerkezet a fejlesztés során változhat.

## 6. Fejlesztési környezet

- Arduino IDE
- C++ / Arduino framework
- ESP32 Arduino core
- Git és GitHub a verziókövetéshez

A további fejlesztések során a kommunikációs és webes komponensekhez szükséges eszközök és könyvtárak is bekerülnek a projektbe.

## 7. A projekt állapota

**Jelenlegi fázis:** hardveres prototípus és a gyártósor működésének szimulációja.

A fejlesztés folyamatos. A kommunikációs réteg, az adatkezelés és a webes felügyeleti felület kialakítása a következő feladatok közé tartozik.

## 8. Dokumentáció

A részletes műszaki dokumentáció a [`docs/dokumentacio.md`](docs/dokumentacio.md) fájlban található.

