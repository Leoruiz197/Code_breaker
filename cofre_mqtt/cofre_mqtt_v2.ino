// ====== IMPORTS ======
#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include <Arduino.h>
#include <ESP32Servo.h>
#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>
#include <ArduinoJson.h>

// ====== LOGGING GLOBAL ======
#define DEBUG true
#define LOG(x) if (DEBUG) Serial.println(x)
#define LOGF(...) if (DEBUG) Serial.printf(__VA_ARGS__)

// ====== VARIÁVEIS GLOBAIS ======
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

// Cor da equipe
int corEquipeR = 255;
int corEquipeG = 255;
int corEquipeB = 255;

// Armazena a cor conquistada por cada etapa
int etapaCorR[10];
int etapaCorG[10];
int etapaCorB[10];

Servo servo1;
Servo servo2;
Adafruit_NeoPixel strip(LED_TOTAL, LED_PIN, NEO_GRB + NEO_KHZ800);

bool mqttStatus = false;
int anguloFechado = 60;
int anguloAberto = 0;
int posicaoAtualServo1 = 0;
int posicaoAtualServo2 = 60;
int num_senhas = 1;

WiFiClient espClient;
PubSubClient client(espClient);

int etapaAtual = 0;

// ==== PROTÓTIPOS ====
// wifi_mqtt.ino
bool connectMQTT();
void callback(char *topic, byte *payload, unsigned int length);

// comandos.ino
void comandosCofre(String payload);
void tentandoAbrir();

// config.ino
void atualizarConfiguracoes(String payload);

// servo.ino
void moverServoSuave(Servo &servo, int &posicaoAtual, int alvo, int delayTime);
void abrirPortaSuave();
void fecharPortaSuave();
void abrirServo1();
void fecharServo1();
void salvarPosicaoServo(int addr, int pos);
int lerPosicaoServo(int addr, int padrao, int minimo, int maximo);

// leds.ino
void aplicarCorEquipe();
void ligarLuzInterna();
void desligarLuzInterna();
void ledGreen();
void ledRed();
void ledOff();
void tentativaCorreta(int r, int g, int b);
void tentativaCorretaEtapa(int etapa, int r, int g, int b);
void trocarCorAleatoriaFita2();
void apagarTudo();

// utils.ino
String montarTopico(String cofreTopico, String subTopico = "");


// ====== SETUP ======
void setup() {
  Serial.begin(115200);
  delay(300);

  LOG("\n===== INICIANDO ESP32 =====");

  EEPROM.begin(EEPROM_SIZE);
  strip.begin();
  strip.show();

  LOG("LEDs inicializados.");
  aplicarCorEquipe();

  servo1.attach(SERVO1_PIN, 500, 2400);
  servo2.attach(SERVO2_PIN, 500, 2400);
  LOG("Servos conectados.");

  posicaoAtualServo1 = lerPosicaoServo(EEPROM_SERVO1_ADDR, 0, 0, 180);
  posicaoAtualServo2 = lerPosicaoServo(EEPROM_SERVO2_ADDR, anguloFechado, 0, 180);

  servo1.write(posicaoAtualServo1);
  servo2.write(posicaoAtualServo2);

  LOGF("Servo1 iniciado em %d°\n", posicaoAtualServo1);
  LOGF("Servo2 iniciado em %d°\n", posicaoAtualServo2);


  // ===== CARREGAR CONFIGS DA FLASH =====
  prefs.begin("config", true);
  String savedBroker = prefs.getString("broker", "");
  String savedCofre = prefs.getString("cofre", "");
  anguloFechado = prefs.getInt("anguloFechado", 60);
  num_senhas = prefs.getInt("num_senhas", 1);
  corEquipeR = prefs.getInt("corR", 255);
  corEquipeG = prefs.getInt("corG", 255);
  corEquipeB = prefs.getInt("corB", 255);
  prefs.end();

  LOGF("Config carregada → Angulo: %d | Senhas: %d\n", anguloFechado, num_senhas);
  LOGF("Cor da equipe carregada → R=%d G=%d B=%d\n", corEquipeR, corEquipeG, corEquipeB);

  savedBroker.toCharArray(broker, sizeof(broker));
  savedCofre.toCharArray(cofreTopico, sizeof(cofreTopico));


  // ===== CONFIG WiFi MANAGER =====
  WiFiManager wm;
  WiFiManagerParameter custom_broker("broker", "MQTT Broker", broker, 40);
  WiFiManagerParameter custom_cofre("cofre", "Tópico Cofre", cofreTopico, 20);

  wm.addParameter(&custom_broker);
  wm.addParameter(&custom_cofre);
  wm.setTimeout(180);

  LOG("Entrando no modo de configuração WiFiManager...");

  if (!wm.autoConnect("ESP32_Config", "12345678")) {
    LOG("Falha na configuração WiFi. Reiniciando...");
    delay(2000);
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

  LOGF("Tópico principal: %s\n", topic1.c_str());
  LOGF("Tópico config : %s\n", topic2.c_str());

  mqttStatus = connectMQTT();
}


// ====== LOOP ======
void loop() {
  if (mqttStatus) client.loop();
}
