/*
 * XiaomiBLE.cpp — BLE GATT client for Xiaomi Mi Temperature and Humidity Monitor 2
 *
 * Works with the ORIGINAL Xiaomi firmware — no custom firmware required.
 *
 * Read strategy (tried in order each cycle):
 *   1. Subscribe to NOTIFY on the temperature characteristic, wait up to 10 s.
 *   2. If no notification arrives, fall back to a direct readValue() (READ property).
 *
 * Notification / read payload (3 bytes):
 *   byte 0-1 : temperature as int16_t little-endian, unit = 0.01 °C
 *   byte 2   : humidity as uint8_t, unit = 1 %
 *
 * GATT UUIDs (LYWSD03MMC original firmware):
 *   Service  : EBE0CCB0-7A0A-4B0C-8A1A-6FF2997DA3A6
 *   Char     : EBE0CCC1-7A0A-4B0C-8A1A-6FF2997DA3A6
 *
 * If you have the LYWSD02 (larger e-ink display), change the UUIDs to:
 *   Service  : 226C0000-6476-4566-7562-66734470666D
 *   Char     : 226CAA55-6476-4566-7562-66734470666D
 *
 * Crash fix: the NimBLE client is created ONCE and reused across cycles.
 * Calling NimBLEDevice::deleteClient() while NimBLE's internal task still holds
 * a reference to the same pointer causes a LoadProhibited panic on Core 0.
 *
 * This task does NOT participate in the watchdog health bitmask.
 */

#include "XiaomiBLE.h"
#include "ProjectHeater.h"

static const char* SVC_UUID  = "EBE0CCB0-7A0A-4B0C-8A1A-6FF2997DA3A6";
static const char* CHAR_UUID = "EBE0CCC1-7A0A-4B0C-8A1A-6FF2997DA3A6";

static volatile bool s_notifyReceived = false;
static volatile bool s_connected      = false;
// Cached from first successful discovery — avoids re-running GATT characteristic
// discovery on every connection cycle (the root cause of the characteristicDiscCB crash).
static NimBLERemoteCharacteristic* s_pChar = nullptr;

// NimBLE calls onDisconnect() from its internal task on Core 0.
// Without a concrete callbacks object the library jumps through a null function pointer
// and crashes with LoadProhibited (EXCVADDR 0x00000020).
class XiaomiClientCB : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient*)              { s_connected = true;  }
    void onDisconnect(NimBLEClient*, int)      { s_connected = false; }
};
static XiaomiClientCB s_clientCB;

static void parsePayload(const uint8_t* data, size_t len) {
    if (len < 3) return;
    g_homeTemp       = (int16_t)((data[1] << 8) | data[0]) / 100.0f;
    g_xiaomiHumidity = data[2];
    D_PRINT(F("[Xiaomi] Temp: "));
    D_PRINT(g_homeTemp);
    D_PRINT(F(" C  Hum: "));
    D_PRINTLN(g_xiaomiHumidity);
}

static void onNotify(NimBLERemoteCharacteristic*, uint8_t* data, size_t len, bool) {
    parsePayload(data, len);
    s_notifyReceived = true;
}

// Connects, reads temperature, disconnects.  The client pointer is created once by the
// calling task and reused every cycle — never deleted — to avoid the NimBLE UAF panic.
static bool readSensor(NimBLEClient* pClient) {
    // Disconnect cleanly if a previous cycle left the connection open.
    // Only safe here because no GATT operations are in flight at this point.
    if (pClient->isConnected()) {
        pClient->disconnect();
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    // deleteAttributes=false on subsequent connections: reuse the cached service and
    // characteristic objects so we never re-run GATT discovery.  GATT discovery uses
    // a local stack variable as the callback arg; if ulTaskNotifyTake returns early
    // (leftover notification), the callback fires with a dangling pointer → crash.
    bool firstConnect = (s_pChar == nullptr);
    if (!pClient->connect(NimBLEAddress(XIAOMI_SENSOR_MAC), firstConnect)) {
        D_PRINTLN(F("[Xiaomi] Connect failed"));
        s_pChar = nullptr;  // force full rediscovery next cycle
        return false;
    }

    // Only do GATT discovery on the first connection (or after a cache miss).
    if (firstConnect) {
        vTaskDelay(pdMS_TO_TICKS(1500));  // let NimBLE finish service discovery

        if (!pClient->isConnected()) {
            D_PRINTLN(F("[Xiaomi] Disconnected during discovery"));
            return false;
        }

        NimBLERemoteService* pSvc = pClient->getService(SVC_UUID);
        if (!pSvc) {
            D_PRINTLN(F("[Xiaomi] Service not found"));
            return false;
        }

        s_pChar = pSvc->getCharacteristic(CHAR_UUID);
        if (!s_pChar) {
            D_PRINTLN(F("[Xiaomi] Characteristic not found"));
            return false;
        }
        D_PRINTLN(F("[Xiaomi] GATT discovery complete — cached for future cycles"));
    }

    NimBLERemoteCharacteristic* pChar = s_pChar;

    bool gotReading = false;

    // --- Path 1: notification subscription ---
    if (pChar->canNotify() && pClient->isConnected()) {
        s_notifyReceived = false;
        pChar->subscribe(true, onNotify);

        // Poll for up to 10 s waiting for the sensor to push a notification
        for (int i = 0; i < 100 && !s_notifyReceived && pClient->isConnected(); i++) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        gotReading = s_notifyReceived;

        if (pClient->isConnected()) pChar->unsubscribe();
    }

    // --- Path 2: direct read fallback ---
    if (!gotReading && pChar->canRead() && pClient->isConnected()) {
        std::string val = pChar->readValue();
        if (val.size() >= 3) {
            parsePayload((const uint8_t*)val.data(), val.size());
            gotReading = true;
        }
    }

    if (!gotReading) {
        D_PRINTLN(F("[Xiaomi] No reading received"));
        s_pChar = nullptr;  // force rediscovery next cycle — something went wrong
    }

    if (pClient->isConnected()) pClient->disconnect();
    vTaskDelay(pdMS_TO_TICKS(1000));  // Let NimBLE finish the disconnect before returning
    return gotReading;
}

// Waits for a notification from TaskTimeScheduler (sent 7 s after every cloud push),
// then runs a full BLE connect → read → disconnect cycle.
void TaskXiaomiBLE(void* pv) {
    (void)pv;
    NimBLEDevice::init("");

    // Create the client once and reuse it forever — never call deleteClient()
    NimBLEClient* pClient = NimBLEDevice::createClient();
    // false = do not delete callbacks on disconnect (callbacks is a static object)
    pClient->setClientCallbacks(&s_clientCB, false);

    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        readSensor(pClient);
    }
}
