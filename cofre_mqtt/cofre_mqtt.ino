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

// === COR DA EQUIPE (padrão: branco) ===
int corEquipeR = 255;
int corEquipeG = 255;
int corEquipeB = 255;

Servo servo1;
Servo servo2;
Adafruit_NeoPixel strip(LED_TOTAL, LED_PIN, NEO_GRB + NEO_KHZ800);

bool mqttStatus = false;
int anguloFechado = 60;
int anguloAberto = 0;
int posicaoAtualServo1 = 0;
int posicaoAtualServo2 = 60;
int num_senhas = 1; // número de senhas necessárias para abrir o cofre (padrão = 1)

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

// === FUNÇÃO PARA APLICAR A COR DA EQUIPE NOS LEDs ===
void aplicarCorEquipe() {
  for (int i = LED_FITA2_INICIO; i <= LED_FITA2_FIM; i++) {
    strip.setPixelColor(i, strip.Color(corEquipeR, corEquipeG, corEquipeB));
  }
  strip.show();

  Serial.printf("💡 LEDs ajustados para a cor da equipe (R=%d, G=%d, B=%d)\n",
                corEquipeR, corEquipeG, corEquipeB);
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
void abrirServo1() { moverServoSuave(servo1, posicaoAtualServo1, 0, 2); }
void fecharServo1() { moverServoSuave(servo1, posicaoAtualServo1, 180, 2); }

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

void tentandoAbrir(){
  for (int i = 0; i < 3; i++) {
    ledRed();
    delay(100);
    abrirServo1();
    
    ledOff();
    delay(100);
    fecharServo1();
  }
}

void tentativaCorreta(int r, int g, int b){
  Serial.printf("Executando tentativa correta com cor RGB(%d, %d, %d)\n", r, g, b);
  for (int i = 0; i < 4; i++) {
    ledGreen();
    delay(300);
    ledOff();
    delay(300);
  }
  abrirServo1();
  ligarLuzInterna();
  abrirPortaSuave();

  for (int i = LED_FITA2_INICIO; i <= LED_FITA2_FIM; i++) {
    strip.setPixelColor(i, strip.Color(r, g, b));
  }
  strip.show();
}
void ledGreen(){
  for (int i = LED_FITA2_INICIO; i <= LED_FITA2_FIM; i++) {
    strip.setPixelColor(i, strip.Color(0, 255, 0));
  }
  strip.show();
}

void ledRed(){
  for (int i = LED_FITA2_INICIO; i <= LED_FITA2_FIM; i++) {
    strip.setPixelColor(i, strip.Color(255, 0, 0));
  }
  strip.show();
}

void ledOff(){
  for (int i = LED_FITA2_INICIO; i <= LED_FITA2_FIM; i++) {
    strip.setPixelColor(i, strip.Color(0, 0, 0));
  }
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

// Acende apenas o bloco da etapa com a cor da tentativa (r,g,b)
// e o restante com a cor do cofre (corCofreR/G/B).
// === FUNÇÃO DE TENTATIVA CORRETA POR ETAPA (ACUMULATIVA) ===
// Mantém as etapas anteriores com a cor da equipe que está tentando,
// e as etapas futuras com a cor da equipe dona do cofre.
void tentativaCorretaEtapa(int etapa, int r, int g, int b) {
  if (num_senhas <= 0) num_senhas = 1;  // Evita divisão por zero
  if (etapa < 1) etapa = 1;
  if (etapa > num_senhas) etapa = num_senhas;

  int totalLeds = (LED_FITA2_FIM - LED_FITA2_INICIO + 1);
  int ledsPorEtapa = totalLeds / num_senhas;
  int sobra = totalLeds % num_senhas;  // Caso não divida exatamente

  int limiteEtapaAtual = LED_FITA2_INICIO + (etapa * ledsPorEtapa) - 1;
  if (etapa == num_senhas) limiteEtapaAtual += sobra;
  if (limiteEtapaAtual > LED_FITA2_FIM) limiteEtapaAtual = LED_FITA2_FIM;

  Serial.printf("🏁 Etapa %d/%d - Leds por etapa: %d\n", etapa, num_senhas, ledsPorEtapa);

  for (int i = LED_FITA2_INICIO; i <= LED_FITA2_FIM; i++) {
    if (i <= limiteEtapaAtual) {
      // LEDs até a etapa atual ficam com a cor da equipe da tentativa
      strip.setPixelColor(i, strip.Color(r, g, b));
    } else {
      // LEDs restantes ficam na cor da equipe dona do cofre
      strip.setPixelColor(i, strip.Color(corEquipeR, corEquipeG, corEquipeB));
    }
  }

  strip.show();

  Serial.printf("💡 LEDs atualizados: até %d = equipe tentativa (%d,%d,%d); restante = cofre (%d,%d,%d)\n",
                limiteEtapaAtual, r, g, b, corEquipeR, corEquipeG, corEquipeB);
}


// === Setup ===
void setup(void) {
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);
  strip.begin();
  strip.show();
  // Aplica a cor da equipe nos LEDs ao iniciar
  aplicarCorEquipe();
  servo1.attach(SERVO1_PIN, 500, 2400);
  servo2.attach(SERVO2_PIN, 500, 2400);

  posicaoAtualServo1 = lerPosicaoServo(EEPROM_SERVO1_ADDR, 0);
  posicaoAtualServo2 = lerPosicaoServo(EEPROM_SERVO2_ADDR, anguloFechado);
  servo1.write(posicaoAtualServo1);
  servo2.write(posicaoAtualServo2);

  prefs.begin("config", true);
  String savedBroker = prefs.getString("broker", "");
  String savedCofre  = prefs.getString("cofre", "");
  int savedAngulo = prefs.getInt("anguloFechado", 70);
  anguloFechado = savedAngulo;
  Serial.printf("Ângulo máximo carregado da flash: %d\n", anguloFechado);
  corEquipeR = prefs.getInt("corR", 255);
  corEquipeG = prefs.getInt("corG", 255);
  corEquipeB = prefs.getInt("corB", 255);
  Serial.printf("Cor da equipe carregada da flash: R=%d, G=%d, B=%d\n", corEquipeR, corEquipeG, corEquipeB);
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

  // Evita reprocessar mensagens enviadas pelo próprio ESP
  if (msg.indexOf("\"erro\"") >= 0 || msg.indexOf("\"status\"") >= 0) {
    // Comentando a linha abaixo se não quiser nem logar:
    // Serial.println("Mensagem de status ignorada.");
    return;
  }

  Serial.print("Mensagem recebida no tópico: ");
  Serial.println(topic);
  Serial.print("Conteúdo: ");
  Serial.println(msg);
  Serial.println("-----------------------");

  if (String(topic) == topic1) {
    comandosCofre(msg);
  } else if (String(topic) == topic2) {
    atualizarConfiguracoes(msg);
  }
}


// === Função de comandos (topic1) ===
void comandosCofre(String payload) {
  Serial.println("Executando comando...");
  Serial.print("Payload recebido: ");
  Serial.println(payload);

  StaticJsonDocument<400> doc;
  DeserializationError error = deserializeJson(doc, payload);

  // Verifica se o JSON é válido
  if (error) {
    Serial.println("Erro ao interpretar JSON. Verifique o formato da mensagem.");
    client.publish(topic1.c_str(), "{\"erro\":\"JSON invalido\"}");
    return;
  }

  // Verifica se o campo 'comando' existe
  if (!doc.containsKey("comando")) {
    Serial.println("Campo 'comando' ausente no JSON!");
    client.publish(topic1.c_str(), "{\"erro\":\"campo 'comando' ausente\"}");
    return;
  }

  // O campo 'comando' pode ser uma string simples ou um objeto JSON
  if (doc["comando"].is<const char*>()) {
    // --- Caso simples: {"comando":"abrir"} etc ---
    String comando = doc["comando"].as<String>();
    comando.trim();
    comando.toLowerCase();

    if (comando == "abrir") {
      abrirPortaSuave();
      client.publish(topic1.c_str(), "{\"status\":\"porta aberta\"}");
    } else if (comando == "fechar") {
      fecharPortaSuave();
      client.publish(topic1.c_str(), "{\"status\":\"porta fechada\"}");
    } else if (comando == "luz") {
      ligarLuzInterna();
      client.publish(topic1.c_str(), "{\"status\":\"luz ligada\"}");
    } else if (comando == "apagar") {
      apagarTudo();
      client.publish(topic1.c_str(), "{\"status\":\"luzes apagadas\"}");
    } else if (comando == "tranca_direita") {
      abrirServo1();
      client.publish(topic1.c_str(), "{\"status\":\"tranca direita aberta\"}");
    } else if (comando == "tranca_esquerda") {
      fecharServo1();
      client.publish(topic1.c_str(), "{\"status\":\"tranca esquerda fechada\"}");
    } else if (comando == "correta") {
      // Caso simples: {"comando":"correta","cor":{"R":240,"G":50,"B":200}}
      int r = corEquipeR, g = corEquipeG, b = corEquipeB;
      if (doc.containsKey("cor")) {
        JsonObject cor = doc["cor"];
        r = cor["R"] | r;
        g = cor["G"] | g;
        b = cor["B"] | b;
      }

      tentativaCorreta(r, g, b);
      client.publish(topic1.c_str(), "{\"status\":\"tentativa correta\"}");
    } else {
      client.publish(topic1.c_str(), "{\"erro\":\"comando desconhecido\"}");
    }
  }

  else if (doc["comando"].is<JsonObject>()) {
    // --- Caso complexo: {"comando":{"tentando":true,"etapa":1,"cor":{"R":255,"G":0,"B":0}}} ---
    JsonObject cmd = doc["comando"];

    bool tentando = cmd["tentando"] | false;
    int etapa = cmd["etapa"] | 1;

    int r = corEquipeR, g = corEquipeG, b = corEquipeB;
    if (cmd.containsKey("cor")) {
      JsonObject cor = cmd["cor"];
      r = cor["R"] | r;
      g = cor["G"] | g;
      b = cor["B"] | b;
    }

    if (!tentando) {
      tentandoAbrir();
      client.publish(topic1.c_str(), "{\"status\":\"tentativa incorreta\"}");
    } else {
      tentativaCorretaEtapa(etapa, r, g, b);
      client.publish(topic1.c_str(), "{\"status\":\"etapa registrada\"}");
    }
  }

  else {
    client.publish(topic1.c_str(), "{\"erro\":\"formato de comando nao reconhecido\"}");
  }
}




// === Função de configuração (topic2) ===
void atualizarConfiguracoes(String payload) {
  Serial.println("Executando atualização de configuração...");
  Serial.print("Payload recebido: ");
  Serial.println(payload);

  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, payload);

  if (error) {
    Serial.println("Erro ao interpretar JSON. Verifique o formato.");
    client.publish(topic1.c_str(), "{\"erro\":\"JSON invalido\"}");
    return;
  }

  bool alterouAlgo = false; // controle de gravação

  // === Atualização de ângulo máximo ===
  if (doc.containsKey("angulo-max")) {
    int novoAngulo = doc["angulo-max"];
    if (novoAngulo >= 0 && novoAngulo <= 180) {
      anguloFechado = novoAngulo;
      Serial.printf("Novo valor de anguloFechado: %d\n", anguloFechado);

      String resposta = "{\"status\":\"angulo-max atualizado\",\"valor\":";
      resposta += String(anguloFechado) + "}";
      client.publish(topic1.c_str(), resposta.c_str());
      alterouAlgo = true;
    } else {
      client.publish(topic1.c_str(), "{\"erro\":\"angulo invalido (0-180)\"}");
    }
  }

  // === Atualização de número de senhas ===
  if (doc.containsKey("num_senhas")) {
    int novoNum = doc["num_senhas"];
    if (novoNum > 0 && novoNum <= 10) {
      num_senhas = novoNum;
      Serial.printf("Novo valor de num_senhas: %d\n", num_senhas);

      String resposta = "{\"status\":\"num_senhas atualizado\",\"valor\":";
      resposta += String(num_senhas) + "}";
      client.publish(topic1.c_str(), resposta.c_str());
      alterouAlgo = true;
    } else {
      client.publish(topic1.c_str(), "{\"erro\":\"num_senhas deve ser entre 1 e 10\"}");
    }
  }

  // === Atualização de cor da equipe ===
  if (doc.containsKey("cor_equipe")) {
    JsonObject corEqObj = doc["cor_equipe"]["cor"];
    if (corEqObj.containsKey("R")) corEquipeR = corEqObj["R"];
    if (corEqObj.containsKey("G")) corEquipeG = corEqObj["G"];
    if (corEqObj.containsKey("B")) corEquipeB = corEqObj["B"];

    Serial.printf("Nova cor da equipe definida: R=%d, G=%d, B=%d\n",
                  corEquipeR, corEquipeG, corEquipeB);

    // Atualiza LEDs imediatamente
    for (int i = LED_FITA2_INICIO; i <= LED_FITA2_FIM; i++) {
      strip.setPixelColor(i, strip.Color(corEquipeR, corEquipeG, corEquipeB));
    }
    strip.show();

    // Envia confirmação via MQTT
    String resposta = "{\"status\":\"cor_equipe atualizada\",\"R\":";
    resposta += String(corEquipeR) + ",\"G\":" + String(corEquipeG) +
                ",\"B\":" + String(corEquipeB) + "}";
    client.publish(topic1.c_str(), resposta.c_str());
    alterouAlgo = true;
  }

  // === Salva todas as alterações na flash ===
  if (alterouAlgo) {
    prefs.begin("config", false);
    prefs.putInt("anguloFechado", anguloFechado);
    prefs.putInt("num_senhas", num_senhas);
    prefs.putInt("corR", corEquipeR);
    prefs.putInt("corG", corEquipeG);
    prefs.putInt("corB", corEquipeB);
    prefs.end();
    Serial.println("✅ Configurações salvas na flash com sucesso!");
  }
}
