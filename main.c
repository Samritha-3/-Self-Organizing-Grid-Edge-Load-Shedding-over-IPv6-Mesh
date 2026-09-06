#include <Arduino.h>
#include <OThread.h>
#include <openthread/udp.h>
#include <openthread/dataset.h>
#include <openthread/thread.h>
#include <openthread/ip6.h>
#include "esp_openthread.h"

// ---------------- Pins (GPIO numbers) ----------------
const int LOAD_PIN = 18;       // MOSFET Gate PWM
const int STATUS_LED_PIN = 20; // Status LED
const int RELAY_PIN = 19;      // Relay IN
const int BUTTON_PIN = 2;      // Button

// Active-LOW high-impedance drive:
void relayClose() { pinMode(RELAY_PIN, OUTPUT); digitalWrite(RELAY_PIN, LOW); }
void relayOpen()  { pinMode(RELAY_PIN, INPUT); }

// ---------------- Networking & UDP ----------------
const char *MULTICAST_ADDR_STR = "ff03::1";
const uint16_t UDP_PORT = 1234;

static otUdpSocket sUdpSocket;
static bool socketOpened = false;

// ---------------- RTOS primitives ----------------
SemaphoreHandle_t stateMutex;
QueueHandle_t commandQueue;

struct GridCommand {
  int percent;
  int durationMs;
};

// Shared state
volatile int currentLoadPercent = 100;
volatile bool meshReady = false;
volatile bool relayClosedState = true;

// Button debouncing
int buttonState = HIGH;
int lastFlickerState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long pressStartTime = 0;
const unsigned long debounceDelay = 50;
const unsigned long LONG_PRESS_MS = 1500;

// Forward declarations
void sendUdpMulticast(const char *msg);
static void handleUdpReceive(void *aContext, otMessage *aMessage, const otMessageInfo *aMessageInfo);

// Helper function to send status packet to laptop dashboard
void sendStatusReport(int loadPct, const char* stateStr) {
  char statusBuf[64];
  snprintf(statusBuf, sizeof(statusBuf), "STATUS:%d:%s", loadPct, stateStr);
  sendUdpMulticast(statusBuf);
}

// =====================================================================
// Native OpenThread UDP Helper Functions
// =====================================================================
static void handleUdpReceive(void *aContext, otMessage *aMessage, const otMessageInfo *aMessageInfo) {
  char buf[32];
  uint16_t length = otMessageGetLength(aMessage) - otMessageGetOffset(aMessage);
  if (length >= sizeof(buf)) length = sizeof(buf) - 1;

  otMessageRead(aMessage, otMessageGetOffset(aMessage), buf, length);
  buf[length] = '\0';

  int percent = 0, durationMs = 0;
  if (sscanf(buf, "REDUCE:%d:%d", &percent, &durationMs) == 2) {
    GridCommand cmd = { percent, durationMs };
    xQueueSend(commandQueue, &cmd, 0);
    Serial.printf("[net] Command received: reduce %d%% for %dms\n", percent, durationMs);
  }
}

bool setupUdpSocket() {
  esp_openthread_lock_acquire(portMAX_DELAY);
  otInstance *instance = OThread.getInstance();
  if (!instance) { esp_openthread_lock_release(); return false; }

  memset(&sUdpSocket, 0, sizeof(sUdpSocket));
  if (otUdpOpen(instance, &sUdpSocket, handleUdpReceive, NULL) != OT_ERROR_NONE) {
    esp_openthread_lock_release();
    return false;
  }

  otSockAddr sockAddr;
  memset(&sockAddr, 0, sizeof(sockAddr));
  sockAddr.mPort = UDP_PORT;

  if (otUdpBind(instance, &sUdpSocket, &sockAddr, OT_NETIF_THREAD_INTERNAL) != OT_ERROR_NONE) {
    otUdpClose(instance, &sUdpSocket);
    esp_openthread_lock_release();
    return false;
  }

  otIp6Address multicastAddr;
  if (otIp6AddressFromString(MULTICAST_ADDR_STR, &multicastAddr) == OT_ERROR_NONE) {
    otIp6SubscribeMulticastAddress(instance, &multicastAddr);
  }

  esp_openthread_lock_release();
  return true;
}

void sendUdpMulticast(const char *msg) {
  esp_openthread_lock_acquire(portMAX_DELAY);
  otInstance *instance = OThread.getInstance();
  if (!instance || !socketOpened) {
    esp_openthread_lock_release();
    return;
  }

  otMessageInfo messageInfo;
  memset(&messageInfo, 0, sizeof(messageInfo));
  otIp6AddressFromString(MULTICAST_ADDR_STR, &messageInfo.mPeerAddr);
  messageInfo.mPeerPort = UDP_PORT;

  otMessage *message = otUdpNewMessage(instance, NULL);
  if (message != NULL) {
    if (otMessageAppend(message, msg, strlen(msg)) == OT_ERROR_NONE) {
      if (otUdpSend(instance, &sUdpSocket, message, &messageInfo) != OT_ERROR_NONE) {
        otMessageFree(message);
      }
    } else {
      otMessageFree(message);
    }
  }
  esp_openthread_lock_release();
}

// =====================================================================
// TASK 1: networkListenerTask
// =====================================================================
void networkListenerTask(void *pvParameters) {
  ot_device_role_t lastRole = OT_ROLE_DISABLED;
  unsigned long lastRoleCheck = 0;

  for (;;) {
    if (millis() - lastRoleCheck > 500) {
      lastRoleCheck = millis();
      esp_openthread_lock_acquire(portMAX_DELAY);
      ot_device_role_t role = OThread.otGetDeviceRole();
      esp_openthread_lock_release();

      bool nowAttached = (role == OT_ROLE_CHILD || role == OT_ROLE_ROUTER || role == OT_ROLE_LEADER);

      if (nowAttached && !socketOpened) {
        if (setupUdpSocket()) {
          socketOpened = true;
          digitalWrite(STATUS_LED_PIN, HIGH);
          Serial.println("[net] Attached to OpenThread Mesh.");
        }
      }
    }

    // Handle local button press
    int reading = digitalRead(BUTTON_PIN);
    if (reading != lastFlickerState) lastDebounceTime = millis();
    if ((millis() - lastDebounceTime) > debounceDelay) {
      if (reading != buttonState) {
        buttonState = reading;
        if (buttonState == LOW) {
          pressStartTime = millis(); 
        } else {
          unsigned long heldFor = millis() - pressStartTime;
          if (heldFor >= LONG_PRESS_MS) {
            // LONG PRESS -> Toggle Relay Blackout
            relayClosedState = !relayClosedState;
            if (relayClosedState) { 
              relayClose(); 
              sendStatusReport(100, "NOMINAL");
            } else { 
              relayOpen();  
              sendStatusReport(0, "BLACKOUT");
            }
          } else if (socketOpened) {
            // SHORT PRESS -> Broadcast mesh reduction
            sendUdpMulticast("REDUCE:30:5000");
            GridCommand cmd = { 30, 5000 };
            xQueueSend(commandQueue, &cmd, 0);
          }
        }
      }
    }
    lastFlickerState = reading;
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// =====================================================================
// TASK 2: relayControlTask (PWM Load Adjustment)
// =====================================================================
void relayControlTask(void *pvParameters) {
  ledcAttach(LOAD_PIN, 5000, 8);
  ledcWrite(LOAD_PIN, 255); // 100% duty cycle

  GridCommand cmd;
  for (;;) {
    if (xQueueReceive(commandQueue, &cmd, portMAX_DELAY) == pdTRUE) {
      int targetPercent = 100 - cmd.percent;
      int targetDuty = (targetPercent * 255) / 100;

      ledcWrite(LOAD_PIN, targetDuty);
      sendStatusReport(targetPercent, "SHEDDING_LOAD");

      vTaskDelay(pdMS_TO_TICKS(cmd.durationMs));

      ledcWrite(LOAD_PIN, 255);
      sendStatusReport(100, "NOMINAL");
    }
  }
}

// =====================================================================
// SETUP & INITIALIZATION
// =====================================================================
void setup() {
  Serial.begin(115200);
  delay(300);

  relayClose();
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, LOW);

  stateMutex = xSemaphoreCreateMutex();
  commandQueue = xQueueCreate(5, sizeof(GridCommand));

  OpenThread::begin(false);

  otOperationalDataset dataset;
  memset(&dataset, 0, sizeof(otOperationalDataset));
  dataset.mActiveTimestamp.mSeconds = 1;
  dataset.mComponents.mIsActiveTimestampPresent = true;

  dataset.mChannel = 15;
  dataset.mComponents.mIsChannelPresent = true;

  dataset.mPanId = 0x1234;
  dataset.mComponents.mIsPanIdPresent = true;

  uint8_t extPanId[8] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01, 0x02, 0x03};
  memcpy(dataset.mExtendedPanId.m8, extPanId, 8);
  dataset.mComponents.mIsExtendedPanIdPresent = true;

  const char* netName = "DRMeshNet";
  strncpy(dataset.mNetworkName.m8, netName, sizeof(dataset.mNetworkName.m8));
  dataset.mComponents.mIsNetworkNamePresent = true;

  uint8_t meshLocalPrefix[8] = {0xfd, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};
  memcpy(dataset.mMeshLocalPrefix.m8, meshLocalPrefix, 8);
  dataset.mComponents.mIsMeshLocalPrefixPresent = true;

  uint8_t networkKey[16] = {
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
    0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF
  };
  memcpy(dataset.mNetworkKey.m8, networkKey, 16);
  dataset.mComponents.mIsNetworkKeyPresent = true;

  otDatasetSetActive(OThread.getInstance(), &dataset);
  OThread.networkInterfaceUp();
  OThread.start();

  xTaskCreatePinnedToCore(networkListenerTask, "netListener", 16384, NULL, 2, NULL, 0);
  xTaskCreatePinnedToCore(relayControlTask,   "pwmCtrl",     8192, NULL, 2, NULL, 0);
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}
