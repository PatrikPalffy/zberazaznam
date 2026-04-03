# Telemetrický systém – bakalárska práca

Tento projekt predstavuje návrh a implementáciu telemetrického systému založeného na mikrokontroléri ESP32. Systém slúži na zber, spracovanie a odosielanie údajov zo senzorov do cloudovej databázy Firebase v reálnom čase.

## Popis projektu

Zariadenie zbiera dáta z viacerých senzorov, konkrétne:
- MPU6500 (akcelerometer a gyroskop)
- BMP280 (teplota, tlak, nadmorská výška)
- GPS NEO-6M (geografická poloha, rýchlosť, počet satelitov)

Získané údaje sú spracované mikrokontrolérom ESP32 a následne odosielané prostredníctvom Wi-Fi pripojenia do Firebase Realtime Database. Dáta sú ukladané vo formáte JSON a aktualizované v pravidelných intervaloch.

## Hlavné funkcie

- meranie pohybu (zrýchlenie a rotácia)
- meranie teploty, tlaku a nadmorskej výšky
- získavanie GPS súradníc
- odosielanie dát do cloudovej databázy Firebase
- real-time monitoring údajov
- výpis dát cez Serial Monitor

## Použité technológie

- ESP32 (NodeMCU)
- Arduino IDE
- Firebase Realtime Database
- Wi-Fi pripojenie (aktuálne využitý mobilný hotspot)
- I2C komunikácia (senzory)
- UART komunikácia (GPS modul)

## Spôsob pripojenia

Zariadenie sa pripája na internet prostredníctvom Wi-Fi siete. Počas testovania bol použitý mobilný hotspot, ktorý zabezpečuje pripojenie k internetu a umožňuje odosielanie dát do Firebase databázy.

## Obmedzenia systému
- systém je závislý od dostupnosti Wi-Fi pripojenia
- GPS modul vyžaduje priamu viditeľnosť satelitov pre správnu funkčnosť
- v interiéri môžu byť GPS údaje nepresné alebo nedostupné

## Možnosti rozšírenia
- pridanie ďalších senzorov
- implementácia lokálneho ukladania dát (napr. SD karta)
- vizualizácia dát (webová alebo mobilná aplikácia)
- optimalizácia spotreby energie

## Autor

Patrik Palffy

## Typ práce

Bakalárska práca – Aplikovaná informatika
Univerzita Konštantína Filozofa v Nitre

## Štruktúra dát (JSON)

Príklad odosielaných dát:

```json
{
  "timestamp_ms": 123456,
  "mpu6500": {
    "temperature_c": 25.3,
    "accel_g": { "x": 0.01, "y": 0.02, "z": 0.98 },
    "gyro_dps": { "x": 0.5, "y": 0.1, "z": 0.0 }
  },
  "bmp280": {
    "temperature_c": 24.8,
    "pressure_hpa": 1012.3,
    "altitude_m": 120.5
  },
  "gps": {
    "latitude": 48.123456,
    "satellites": 7
  }
}

