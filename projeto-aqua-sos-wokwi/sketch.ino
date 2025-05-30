#include <WiFi.h>
#include <PubSubClient.h>

// WiFi e MQTT config
const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* mqttServer = "broker.hivemq.com";
const int mqttPort = 1883;
const char* mqttTopic = "aquasos/agua/nivel";

// Pinos
#define TRIG_PIN 5
#define ECHO_PIN 18
#define RED_LED 4
#define BUZZER_PIN 12
#define BUTTON_PIN 13

WiFiClient espClient;
PubSubClient client(espClient);

int lastNivelPercent = -1;
bool alarmSilenced = false;

// Sequência da melodia
const int melody[] = {1000, 1500, 1200};
const int noteDurations[] = {200, 200, 300};
const int nNotes = sizeof(melody)/sizeof(melody[0]);
int melodyIndex = 0;
unsigned long lastNoteTime = 0;
bool buzzerActive = false;

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Conectando-se a ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi conectado");
  Serial.println("Endereço IP: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Tentando conexão MQTT...");
    if (client.connect("ESP32Sensor")) {
      Serial.println("Conectado");
    } else {
      Serial.print("Falha, rc=");
      Serial.print(client.state());
      Serial.println(" tentando novamente em 2 segundos");
      delay(2000);
    }
  }
}

long readDistanceCM() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0) {
    return 400;
  }
  long distance = duration * 0.034 / 2;
  return distance;
}

void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  setup_wifi();
  client.setServer(mqttServer, mqttPort);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  long distance = readDistanceCM();

  int nivelPercent;
  if (distance > 200) {
    nivelPercent = 0;
  } else {
    nivelPercent = map(distance, 2, 200, 100, 0);
    nivelPercent = constrain(nivelPercent, 0, 100);
  }

  // LED alarme
  if (nivelPercent < 20) {
    digitalWrite(RED_LED, HIGH);
    Serial.println("Nível CRÍTICO! LED ALARME ACESO!");
  } else {
    digitalWrite(RED_LED, LOW);
  }

  // BOTÃO: se apertado, silencia o alarme
  if (digitalRead(BUTTON_PIN) == LOW && !alarmSilenced) {
    alarmSilenced = true;
    noTone(BUZZER_PIN);
    Serial.println("BUZZER SILENCIADO PELO BOTÃO!");
    delay(300);
    while (digitalRead(BUTTON_PIN) == LOW) { delay(10); }
  }

  // Alarme sonoro: toca enquanto nivel = 0 e não está silenciado
  if (nivelPercent == 0 && !alarmSilenced) {
    unsigned long now = millis();
    if (!buzzerActive || (now - lastNoteTime > noteDurations[melodyIndex] + 50)) {
      // Próxima nota
      tone(BUZZER_PIN, melody[melodyIndex], noteDurations[melodyIndex]);
      lastNoteTime = now;
      melodyIndex = (melodyIndex + 1) % nNotes;
      buzzerActive = true;
      Serial.println("BUZZER: Nota tocando!");
    }
  } else {
    noTone(BUZZER_PIN);
    buzzerActive = false;
    melodyIndex = 0;
    // Se o nível subir, reseta silenciador
    if (alarmSilenced && nivelPercent > 0) {
      alarmSilenced = false;
      Serial.println("Silenciador RESETADO, pronto para novo alerta!");
    }
  }

  // Só publica se mudou
  if (nivelPercent != lastNivelPercent) {
    String payload = "{\"pontoId\": 1, \"nivel\": " + String(nivelPercent) + "}";
    Serial.print("Enviando payload MQTT: ");
    Serial.println(payload);
    client.publish(mqttTopic, payload.c_str());
    lastNivelPercent = nivelPercent;
  }

  delay(50); // Ciclo bem curto para boa resposta ao botão
}
