
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

// =====================================================
// 1. CAU HINH PHAN CUNG
// =====================================================
#define MQ2_PIN    34
#define DHT_PIN    4
#define DHT_TYPE   DHT22

#define FAN_PIN    25
#define PUMP_PIN   26

DHT dht(DHT_PIN, DHT_TYPE);

// =====================================================
// 2. WIFI
// =====================================================
const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASS = "";

// =====================================================
// 3. MQTT HIVEMQ
// =====================================================
const char* MQTT_HOST = "broker.hivemq.com";
const uint16_t MQTT_PORT = 1883;


const char* TOPIC_SENSOR = "lab3_khi_gas/son_31432337/sensor";
const char* TOPIC_STATUS = "lab3_khi_gas/son_31432337/status";
const char* TOPIC_MODE   = "lab3_khi_gas/son_31432337/mode/set";
const char* TOPIC_FAN    = "lab3_khi_gas/son_31432337/fan/set";
const char* TOPIC_PUMP   = "lab3_khi_gas/son_31432337/pump/set";

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

// =====================================================
// 4. NGUONG CANH BAO
// =====================================================
// Cac nguong gas nay la gia tri ADC 12-bit, khong phai ppm.
const int GAS_ON  = 2000;
const int GAS_OFF = 1700;

const float TEMP_ON  = 50.0;
const float TEMP_OFF = 45.0;

// =====================================================
// 5. DU LIEU DUNG CHUNG GIUA CAC TASK
// =====================================================
struct SensorData {
  int gasADC = 0;
  float temperature = NAN;
  float humidity = NAN;
  bool sensorReady = false;
};

struct SystemData {
  int gasADC = 0;
  float temperature = NAN;
  float humidity = NAN;

  bool gasAlarm = false;
  bool tempAlarm = false;

  bool fan = false;
  bool pump = false;

  bool autoMode = true;
  bool manualFan = false;
  bool manualPump = false;

  bool sensorReady = false;
};

SystemData data;

SemaphoreHandle_t dataMutex;
QueueHandle_t dataQueue;
QueueHandle_t alertQueue;

// =====================================================
// 6. CAC BIEN THOI GIAN GUI MQTT
// =====================================================
unsigned long lastSensorPublish = 0;
unsigned long lastStatusPublish = 0;

// =====================================================
// 7. HAM DOC/CAP NHAT DU LIEU CHUNG
// =====================================================
SystemData getDataSnapshot() {
  SystemData copy;

  if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    copy = data;
    xSemaphoreGive(dataMutex);
  }

  return copy;
}

// =====================================================
// 8. TASK DOC CAM BIEN
// =====================================================
void sensorTask(void* parameter) {
  unsigned long lastDHTRead = 0;

  for (;;) {
    SensorData sample;
    sample.gasADC = analogRead(MQ2_PIN);
    bool printSample = false;

    // DHT22 chi doc moi 2 giay; giu lai mau hop le gan nhat neu doc loi
    if (millis() - lastDHTRead >= 2000) {
      lastDHTRead = millis();
      printSample = true;

      float t = dht.readTemperature();
      float h = dht.readHumidity();

      if (!isnan(t) && !isnan(h)) {
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
          data.temperature = t;
          data.humidity = h;
          data.sensorReady = true;
          xSemaphoreGive(dataMutex);
        }
      } else {
        Serial.println("[SENSOR] Loi doc DHT22");
      }
    }

    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
      data.gasADC = sample.gasADC;
      sample.temperature = data.temperature;
      sample.humidity = data.humidity;
      sample.sensorReady = data.sensorReady;
      xSemaphoreGive(dataMutex);
    }

    // Hai queue luon nhan cung mot mau; do dai 1 giu lai mau moi nhat.
    xQueueOverwrite(dataQueue, &sample);
    xQueueOverwrite(alertQueue, &sample);

    if (printSample) {
      SystemData state = getDataSnapshot();
      Serial.println("\n========== FIRE GUARD ==========");
      Serial.print("Gas ADC: ");
      Serial.println(sample.gasADC);
      Serial.print("Nhiet do: ");
      if (sample.sensorReady) {
        Serial.print(sample.temperature);
        Serial.println(" C");
      } else {
        Serial.println("Loi doc DHT22");
      }
      Serial.print("Mode: ");
      Serial.println(state.autoMode ? "AUTO" : "MANUAL");
      Serial.print("Gas alarm: ");
      Serial.println(state.gasAlarm ? "CANH BAO" : "Binh thuong");
      Serial.print("Temperature alarm: ");
      Serial.println(state.tempAlarm ? "CANH BAO" : "Binh thuong");
      Serial.print("Fan: ");
      Serial.println(state.fan ? "ON" : "OFF");
      Serial.print("Pump: ");
      Serial.println(state.pump ? "ON" : "OFF");
    }

    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

// =====================================================
// 9. TASK XU LY CANH BAO VA DIEU KHIEN GPIO
// =====================================================
void alarmTask(void* parameter) {
  bool gasAlarm = false;
  bool tempAlarm = false;

  for (;;) {
    SensorData sample;
    if (xQueueReceive(alertQueue, &sample, portMAX_DELAY) != pdTRUE) {
      continue;
    }

    SystemData s = getDataSnapshot();

    // MQ-2: co hysteresis de tranh bat/tat lien tuc
    if (!gasAlarm && sample.gasADC >= GAS_ON) {
      gasAlarm = true;
    } else if (gasAlarm && sample.gasADC <= GAS_OFF) {
      gasAlarm = false;
    }

    // Nhiet do: chi danh gia khi co mau DHT hop le
    if (sample.sensorReady && !tempAlarm &&
        sample.temperature >= TEMP_ON) {
      tempAlarm = true;
    } else if (sample.sensorReady && tempAlarm &&
               sample.temperature <= TEMP_OFF) {
      tempAlarm = false;
    }

    // Canh bao tai cho duoc uu tien cao hon lenh MANUAL
    bool fanState =
        gasAlarm || (!s.autoMode && s.manualFan);

    bool pumpState =
        tempAlarm || (!s.autoMode && s.manualPump);

    // Dieu khien ngo ra
    digitalWrite(FAN_PIN, fanState ? HIGH : LOW);
    digitalWrite(PUMP_PIN, pumpState ? HIGH : LOW);

    // Cap nhat trang thai dung chung
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50))
        == pdTRUE) {
      data.gasAlarm = gasAlarm;
      data.tempAlarm = tempAlarm;
      data.fan = fanState;
      data.pump = pumpState;
      xSemaphoreGive(dataMutex);
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// =====================================================
// 10. MQTT CALLBACK: NHAN LENH TU WEB
// =====================================================
void mqttCallback(char* topic, byte* payload,
                  unsigned int length) {
  char message[32];

  unsigned int n = length;
  if (n >= sizeof(message)) n = sizeof(message) - 1;

  memcpy(message, payload, n);
  message[n] = '\0';

  // Loai bo ky tu xuong dong neu co
  String cmd = String(message);
  cmd.trim();
  cmd.toUpperCase();

  if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50))
      != pdTRUE) {
    return;
  }

  if (strcmp(topic, TOPIC_MODE) == 0) {
    if (cmd == "AUTO") {
      data.autoMode = true;
    } else if (cmd == "MANUAL") {
      data.autoMode = false;
    }
  }

  else if (strcmp(topic, TOPIC_FAN) == 0) {
    if (cmd == "ON") {
      data.manualFan = true;
    } else if (cmd == "OFF") {
      data.manualFan = false;
    }
  }

  else if (strcmp(topic, TOPIC_PUMP) == 0) {
    if (cmd == "ON") {
      data.manualPump = true;
    } else if (cmd == "OFF") {
      data.manualPump = false;
    }
  }

  xSemaphoreGive(dataMutex);

  Serial.printf("[MQTT RX] %s = %s\n", topic, message);
}

// =====================================================
// 11. KET NOI LAI WIFI KHONG CHO VO HAN
// =====================================================
unsigned long lastWiFiAttempt = 0;
bool wifiAttempted = false;

void maintainWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  if (!wifiAttempted || millis() - lastWiFiAttempt >= 10000) {
    lastWiFiAttempt = millis();
    wifiAttempted = true;

    Serial.println("[WIFI] Dang thu ket noi...");
    WiFi.begin(WIFI_SSID, WIFI_PASS);
  }
}

// =====================================================
// 12. KET NOI LAI MQTT
// =====================================================
unsigned long lastMQTTAttempt = 0;

void maintainMQTT() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  if (mqtt.connected()) {
    mqtt.loop();
    return;
  }

  if (millis() - lastMQTTAttempt < 5000) {
    return;
  }

  lastMQTTAttempt = millis();

  Serial.println("[MQTT] Dang thu ket noi...");

  // Client ID phai khac nhau neu co nhieu ESP32
  String clientId = "FireGuard_ESP32_";
  clientId += String((uint32_t)ESP.getEfuseMac(), HEX);

  if (mqtt.connect(clientId.c_str())) {
    Serial.println("[MQTT] Da ket noi");

    mqtt.subscribe(TOPIC_MODE);
    mqtt.subscribe(TOPIC_FAN);
    mqtt.subscribe(TOPIC_PUMP);
  } else {
    Serial.printf("[MQTT] Loi ket noi, state=%d\n",
                  mqtt.state());
  }
}

// =====================================================
// 13. GUI DU LIEU CAM BIEN
// =====================================================
void publishSensor(const SensorData& s) {
  char payload[128];

  if (s.sensorReady) {
    snprintf(payload, sizeof(payload),
      "{\"gas_adc\":%d,\"temperature\":%.2f,"
      "\"humidity\":%.2f}",
      s.gasADC, s.temperature, s.humidity);
  } else {
    snprintf(payload, sizeof(payload),
      "{\"gas_adc\":%d,\"temperature\":null,"
      "\"humidity\":null}",
      s.gasADC);
  }

  if (!mqtt.publish(TOPIC_SENSOR, payload)) {
    Serial.println("[MQTT] Gui du lieu cam bien that bai");
  }
}

// =====================================================
// 14. GUI TRANG THAI HE THONG
// =====================================================
void publishStatus(const SystemData& s) {
  char payload[160];

  snprintf(payload, sizeof(payload),
    "{\"fan\":%s,\"pump\":%s,\"mode\":\"%s\","
    "\"gas_alarm\":%s,\"temp_alarm\":%s}",
    s.fan ? "true" : "false",
    s.pump ? "true" : "false",
    s.autoMode ? "AUTO" : "MANUAL",
    s.gasAlarm ? "true" : "false",
    s.tempAlarm ? "true" : "false");

  mqtt.publish(TOPIC_STATUS, payload);
}

// =====================================================
// 15. TASK MANG: WIFI + MQTT + GUI DU LIEU
// =====================================================
void networkTask(void* parameter) {
  SensorData latestSample;
  bool hasSensorSample = false;

  for (;;) {
    maintainWiFi();

    if (WiFi.status() == WL_CONNECTED) {
      maintainMQTT();

      if (mqtt.connected()) {
        SensorData queuedSample;
        while (xQueueReceive(dataQueue, &queuedSample, 0) == pdTRUE) {
          latestSample = queuedSample;
          hasSensorSample = true;
        }

        unsigned long now = millis();
        SystemData s = getDataSnapshot();

        if (hasSensorSample && now - lastSensorPublish >= 2000) {
          lastSensorPublish = now;
          publishSensor(latestSample);
        }

        if (now - lastStatusPublish >= 1000) {
          lastStatusPublish = now;
          publishStatus(s);
        }
      }
    }

    // Khong de task mang chiem CPU lien tuc
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// =====================================================
// 16. SETUP
// =====================================================
void setup() {
  Serial.begin(115200);

  pinMode(MQ2_PIN, INPUT);
  pinMode(FAN_PIN, OUTPUT);
  pinMode(PUMP_PIN, OUTPUT);

  digitalWrite(FAN_PIN, LOW);
  digitalWrite(PUMP_PIN, LOW);

  analogReadResolution(12);

  dht.begin();

  dataMutex = xSemaphoreCreateMutex();

  if (dataMutex == nullptr) {
    Serial.println("Khong tao duoc Mutex!");
    while (true) {
      digitalWrite(FAN_PIN, LOW);
      digitalWrite(PUMP_PIN, LOW);
      delay(1000);
    }
  }

  dataQueue = xQueueCreate(1, sizeof(SensorData));
  alertQueue = xQueueCreate(1, sizeof(SensorData));

  if (dataQueue == nullptr || alertQueue == nullptr) {
    Serial.println("Khong tao duoc queue!");
    while (true) {
      digitalWrite(FAN_PIN, LOW);
      digitalWrite(PUMP_PIN, LOW);
      delay(1000);
    }
  }

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(mqttCallback);

  // Gioi han thoi gian cho doc phan hoi MQTT
  mqtt.setSocketTimeout(1);
  mqtt.setKeepAlive(15);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);

  // Tao task. Stack tinh theo byte tren ESP32 Arduino.
  bool tasksCreated =
    xTaskCreatePinnedToCore(
    sensorTask, "SensorTask",
    4096, nullptr, 2, nullptr, 1
  ) == pdPASS;

  tasksCreated = tasksCreated && xTaskCreatePinnedToCore(
    alarmTask, "AlarmTask",
    4096, nullptr, 3, nullptr, 1
  ) == pdPASS;

  tasksCreated = tasksCreated && xTaskCreatePinnedToCore(
    networkTask, "NetworkTask",
    6144, nullptr, 1, nullptr, 0
  ) == pdPASS;

  if (!tasksCreated) {
    Serial.println("Khong tao duoc task!");
    digitalWrite(FAN_PIN, LOW);
    digitalWrite(PUMP_PIN, LOW);
    while (true) {
      delay(1000);
    }
  }

  Serial.println("FireGuard FreeRTOS da khoi dong");
}

// =====================================================
// 17. LOOP KHONG CON XU LY CHINH
// =====================================================
void loop() {
  // Cac cong viec chinh da duoc tach thanh FreeRTOS task
  vTaskDelay(pdMS_TO_TICKS(1000));
}
