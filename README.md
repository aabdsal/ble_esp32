# BLE Mando-Robot con ESP32-S3

## Objetivo

Controlar un robot de 6 servos usando un mando basado en ESP32-S3 mediante Bluetooth Low Energy.

El objetivo principal del proyecto no es solo mover el robot, sino especializarnos en Bluetooth Low Energy sobre ESP32-S3 usando ESP-IDF, entendiendo los conceptos principales de BLE y aplicándolos en una comunicación real entre dos microcontroladores.

## Conceptos a estudiar

- Bluetooth Low Energy
- Diferencia entre Bluetooth Classic y BLE
- GAP
- GATT
- Central vs Peripheral
- GATT Client vs GATT Server
- Advertising
- Scanning
- Connection
- Service
- Characteristic
- UUID
- Read
- Write
- Notify
- MTU
- Pairing y bonding, si da tiempo

## Microcontroladores

Se van a usar dos ESP32-S3:

- ESP32-S3 Mando
- ESP32-S3 Robot

## Stack de desarrollo

- ESP-IDF
- NimBLE
- Lenguaje C
- Bluetooth Low Energy

## Comunicación BLE

La comunicación se realizará mediante BLE usando una arquitectura cliente-servidor GATT.

### ESP32-S3 Robot

- Rol GAP: Peripheral
- Rol GATT: Server
- Función:
  - Hace advertising.
  - Espera conexión del mando.
  - Expone un servicio BLE personalizado.
  - Recibe comandos mediante una characteristic de escritura.
  - Ejecuta movimientos en los servos.
  - Opcionalmente notifica su estado al mando.

### ESP32-S3 Mando

- Rol GAP: Central
- Rol GATT: Client
- Función:
  - Escanea dispositivos BLE.
  - Busca el robot por nombre o UUID de servicio.
  - Se conecta al robot.
  - Descubre el servicio y las characteristics.
  - Escribe comandos al pulsar botones.

## Arquitectura general

```text
+-------------------+          BLE           +-------------------+
| ESP32-S3 Mando    | ---------------------> | ESP32-S3 Robot    |
|                   |                        |                   |
| GAP Central       |                        | GAP Peripheral    |
| GATT Client       |                        | GATT Server       |
| Botones GPIO      |                        | Control servos    |
+-------------------+                        +-------------------+
```