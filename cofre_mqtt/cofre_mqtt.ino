#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include <Arduino.h>
#include <ESP32Servo.h>
#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>
#include <ArduinoJson.h>

Preferences prefs;

char broker[40];
char cofreTopico[20];
String topic1;
String topic2;

#define EEPROM_SIZE 8
#define EEPROM_SERVO1_ADDR 0
#define EEPROM_SERVO2_ADDR 4

#define SERVO1_PIN 18
#define SERVO2_PIN 19
#define LED_PIN 23
#define LED_TOTAL 18
#define LED_FITA1_INICIO 0
#define LED_FITA1_FIM 1
#define LED_FITA2_INICIO 2
#define LED_FITA2_FIM 17

Servo servo1;
Servo servo2;
Adafruit_NeoPixel strip(LED_TOTAL, LED_PIN, NEO_GRB + NEO_KHZ800);

bool mqttStatus = false;
int anguloFechado = 70;
int anguloAberto = 0;
int posicaoAtualServo1 = 0;
int posicaoAtualServo2 = 70;
String mensagem;

WiFiClient espClient;
PubSubClient client(espClient);

// === Protótipos ===
bool connectMQTT();
void callback(char *topic, byte *payload, unsigned int length);
void comandosCofre(String payload);
void atualizarConfiguracoes(String payload);

// === Funções auxiliares ===
String montarTopico(String cofreTopico, String subTopico = "") {
  String topico = "fiap/" + cofreTopico;
  if (subTopico.length() > 0) topico += "/" + subTopico;
  return topico;
}

void salvarPosicaoServo(int addr, int pos) {
  EEPROM.writeInt(addr, pos);
  EEPROM.commit();
}

int lerPosicaoServo(int addr, int padrao, int minimo = 0, int maximo = 180) {
  int pos = EEPROM.readInt(addr);
  if (pos < minimo || pos > maximo) pos = padrao;
  return pos;
}

// === Movimento suave ===
void moverServoSuave(Servo &servo, int &posicaoAtual, int alvo, int delayTime = 1) {
  if (alvo == posicaoAtual) return;

  if (posicaoAtual < alvo) {
    for (int i = posicaoAtual; i <= alvo; i++) {
      servo.write(i);
      delay(delayTime);
    }
  } else {
    for (int i = posicaoAtual; i >= alvo; i--) {
      servo.write(i);
      delay(delayTime);
    }
  }

  posicaoAtual = alvo;
  if (&servo == &servo1)
    salvarPosicaoServo(EEPROM_SERVO1_ADDR, posicaoAtual);
  else
    salvarPosicaoServo(EEPROM_SERVO2_ADDR, posicaoAtual);
}

// === Ações ===
void abrirPortaSuave() { moverServoSuave(servo2, posicaoAtualServo2, anguloAberto, 10); }
void fecharPortaSuave() { moverServoSuave(servo2, posicaoAtualServo2, anguloFechado, 10); }
void abrirServo1() { moverServoSuave(servo1, posicaoAtualServo1, 180, 2); }
void fecharServo1() { moverServoSuave(servo1, posicaoAtualServo1, 0, 2); }

void ligarLuzInterna() {
  for (int i = LED_FITA1_INICIO; i <= LED_FITA1_FIM; i++)
    strip.setPixelColor(i, strip.Color(255, 255, 255));
  strip.show();
}

void desligarLuzInterna() {
  for (int i = LED_FITA1_INICIO; i <= LED_FITA1_FIM; i++)
    strip.setPixelColor(i, strip.Color(0, 0, 0));
  strip.show();
}

void trocarCorAleatoriaFita2() {
  int r = random(0, 256), g = random(0, 256), b = random(0, 256);
  for (int i = LED_FITA2_INICIO; i <= LED_FITA2_FIM; i++)
    strip.setPixelColor(i, strip.Color(r, g, b));
  strip.show();
}

void apagarTudo() {
  for (int i = 0; i < LED_TOTAL; i++) strip.setPixelColor(i, 0);
  strip.show();
}

// === Setup ===
void setup(void) {
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);
  strip.begin();
  strip.show();
  servo1.attach(SERVO1_PIN, 500, 2400);
  servo2.attach(SERVO2_PIN, 500, 2400);

  posicaoAtualServo1 = lerPosicaoServo(EEPROM_SERVO1_ADDR, 0);
  posicaoAtualServo2 = lerPosicaoServo(EEPROM_SERVO2_ADDR, anguloFechado);
  servo1.write(posicaoAtualServo1);
  servo2.write(posicaoAtualServo2);

  prefs.begin("config", true);
  String savedBroker = prefs.getString("broker", "");
  String savedCofre  = prefs.getString("cofre", "");
  prefs.end();
  savedBroker.toCharArray(broker, sizeof(broker));
  savedCofre.toCharArray(cofreTopico, sizeof(cofreTopico));

  WiFiManager wm;
  WiFiManagerParameter custom_broker("broker", "MQTT Broker", broker, 40);
  WiFiManagerParameter custom_cofre("cofre", "Tópico do Cofre", cofreTopico, 20);
  wm.addParameter(&custom_broker);
  wm.addParameter(&custom_cofre);
  wm.setTimeout(180);

  if (!wm.autoConnect("ESP32_Config", "12345678")) {
    Serial.println("Falha na conexão WiFi, reiniciando...");
    delay(3000);
    ESP.restart();
  }

  strcpy(broker, custom_broker.getValue());
  strcpy(cofreTopico, custom_cofre.getValue());

  prefs.begin("config", false);
  prefs.putString("broker", broker);
  prefs.putString("cofre", cofreTopico);
  prefs.end();

  topic1 = montarTopico(cofreTopico);
  topic2 = montarTopico(cofreTopico, "config");

  Serial.println("---- Tópicos MQTT ----");
  Serial.print("Publica em: "); Serial.println(topic1);
  Serial.print("Escuta em:  "); Serial.println(topic2);
  Serial.println("----------------------------------");

  mqttStatus = connectMQTT();
}

// === Loop ===
void loop() {
  if (mqttStatus) client.loop();
  delay(10);
}

// === Conexão MQTT ===
bool connectMQTT() {
  client.setServer(broker, 1883);
  client.setCallback(callback);

  byte tentativa = 0;
  while (!client.connected() && tentativa < 5) {
    String client_id = "Cofre-" + String(WiFi.macAddress());
    Serial.printf("Conectando ao broker (%s)...\n", broker);

    if (client.connect(client_id.c_str())) {
      Serial.println("✅ Conectado ao broker MQTT!");
      client.subscribe(topic1.c_str());
      client.subscribe(topic2.c_str());
      Serial.println("📡 Inscrito nos tópicos com sucesso!");
      client.publish(topic1.c_str(), "{\"status\":\"ESP32 conectado\"}");
      return true;
    } else {
      Serial.print("❌ Falha na conexão MQTT, estado: ");
      Serial.println(client.state());
      delay(2000);
      tentativa++;
    }
  }

  Serial.println("❌ Não foi possível conectar ao broker MQTT.");
  return false;
}

// === Callback ===
void callback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];

  Serial.print("Mensagem recebida no tópico: ");
  Serial.println(topic);
  Serial.print("Conteúdo: ");
  Serial.println(msg);
  Serial.println("-----------------------");

  if (String(topic) == topic1) {
    comandosCofre(msg); // comandos diretos
  } else if (String(topic) == topic2) {
    atualizarConfiguracoes(msg); // configurações
  }
}

// === Função de comandos (topic1) ===
void comandosCofre(String payload) {
  Serial.println("Executando comando...");
  payload.trim();

  if (payload == "abrir") {
    abrirPortaSuave();
    client.publish(topic1.c_str(), "{\"status\":\"porta aberta\"}");
  } else if (payload == "fechar") {
    fecharPortaSuave();
    client.publish(topic1.c_str(), "{\"status\":\"porta fechada\"}");
  } else if (payload == "luz") {
    ligarLuzInterna();
    client.publish(topic1.c_str(), "{\"status\":\"luz ligada\"}");
  } else if (payload == "apagar") {
    apagarTudo();
    client.publish(topic1.c_str(), "{\"status\":\"luzes apagadas\"}");
  } else if(payload == "tranca_direita"){
    abrirServo1();
    client.publish(topic1.c_str(), "{\"status\":\"tranca direita\"}");
  }else if(payload == "tranca_esquerda"){
    fecharServo1();
    client.publish(topic1.c_str(), "{\"status\":\"tranca esquerda\"}");
  } else {
    client.publish(topic1.c_str(), "{\"erro\":\"comando nao encontrado\"}");
  }
}

// === Função de configuração (topic2) ===
void atualizarConfiguracoes(String payload) {
  Serial.println("Executando atualização de configuração...");
  Serial.print("Payload recebido: ");
  Serial.println(payload);

  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, payload);

  if (error) {
    client.publish(topic1.c_str(), "{\"erro\":\"JSON invalido\"}");
    return;
  }

  if (doc.containsKey("angulo-max")) {
    int novoAngulo = doc["angulo-max"];
    if (novoAngulo >= 0 && novoAngulo <= 180) {
      anguloFechado = novoAngulo;
      String resposta = "{\"status\":\"angulo-max atualizado\",\"valor\":";
      resposta += String(anguloFechado);
      resposta += "}";
      client.publish(topic1.c_str(), resposta.c_str());
    } else {
      client.publish(topic1.c_str(), "{\"erro\":\"angulo invalido (0-180)\"}");
    }
  } else {
    client.publish(topic1.c_str(), "{\"erro\":\"chave ausente no JSON\"}");
  }
}
