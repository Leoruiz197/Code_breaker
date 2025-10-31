#include <Arduino.h>
#include <ESP32Servo.h>
#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>

#define EEPROM_SIZE 8 // 4 bytes p/ cada servo
#define EEPROM_SERVO1_ADDR 0
#define EEPROM_SERVO2_ADDR 4

// === CONFIGURAÇÕES DE PINOS ===
#define SERVO1_PIN 18
#define SERVO2_PIN 19
#define LED_PIN 23
#define LED_TOTAL 18 // 2 da primeira fita + 16 da segunda

// === ÍNDICES DE FAIXAS ===
#define LED_FITA1_INICIO 0
#define LED_FITA1_FIM 1
#define LED_FITA2_INICIO 2
#define LED_FITA2_FIM 17

Servo servo1;
Servo servo2;
Adafruit_NeoPixel strip(LED_TOTAL, LED_PIN, NEO_GRB + NEO_KHZ800);

// === VARIÁVEIS DE CONTROLE DE SERVO ===
int anguloFechado = 70;
int anguloAberto = 0;
int posicaoAtualServo1 = 0;
int posicaoAtualServo2 = 70;

// === FUNÇÕES EEPROM ===
void salvarPosicaoServo(int addr, int pos) {
  EEPROM.writeInt(addr, pos);
  EEPROM.commit();
  Serial.print("Posição salva na EEPROM (addr ");
  Serial.print(addr);
  Serial.print("): ");
  Serial.println(pos);
}

int lerPosicaoServo(int addr, int padrao, int minimo = 0, int maximo = 180) {
  int pos = EEPROM.readInt(addr);
  if (pos < minimo || pos > maximo) pos = padrao;
  Serial.print("Posição lida da EEPROM (addr ");
  Serial.print(addr);
  Serial.print("): ");
  Serial.println(pos);
  return pos;
}

// === MOVIMENTO SUAVE DOS SERVOS ===
void moverServoSuave(Servo &servo, int &posicaoAtual, int alvo, int delayTime = 10) {
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

  // Salva no endereço correto
  if (&servo == &servo1)
    salvarPosicaoServo(EEPROM_SERVO1_ADDR, posicaoAtual);
  else
    salvarPosicaoServo(EEPROM_SERVO2_ADDR, posicaoAtual);
}

// === FUNÇÕES DO SERVO 2 ===
void abrirPortaSuave() {
  Serial.println("Abrindo porta (servo2)...");
  moverServoSuave(servo2, posicaoAtualServo2, anguloAberto, 10);
  Serial.println("Porta aberta.");
}

void fecharPortaSuave() {
  Serial.println("Fechando porta (servo2)...");
  moverServoSuave(servo2, posicaoAtualServo2, anguloFechado, 10);
  Serial.println("Porta fechada.");
}

// === FUNÇÕES DO SERVO 1 ===
void abrirServo1() {
  Serial.println("Abrindo servo 1...");
  moverServoSuave(servo1, posicaoAtualServo1, 180, 10);
  Serial.println("Servo 1 aberto.");
}

void fecharServo1() {
  Serial.println("Fechando servo 1...");
  moverServoSuave(servo1, posicaoAtualServo1, 0, 10);
  Serial.println("Servo 1 fechado.");
}

// === FUNÇÕES PARA LUZ INTERNA ===
void ligarLuzInterna() {
  Serial.println("Ligando luz interna...");
  for (int i = LED_FITA1_INICIO; i <= LED_FITA1_FIM; i++) {
    strip.setPixelColor(i, strip.Color(255, 255, 255));
  }
  strip.show();
  Serial.println("Luz interna ligada.");
}

void desligarLuzInterna() {
  Serial.println("Desligando luz interna...");
  for (int i = LED_FITA1_INICIO; i <= LED_FITA1_FIM; i++) {
    strip.setPixelColor(i, strip.Color(0, 0, 0));
  }
  strip.show();
  Serial.println("Luz interna desligada.");
}

// === FUNÇÃO PARA TROCAR COR ALEATÓRIA NA FITA 2 ===
void trocarCorAleatoriaFita2() {
  int r = random(0, 256);
  int g = random(0, 256);
  int b = random(0, 256);

  Serial.print("Nova cor da fita 2: R=");
  Serial.print(r);
  Serial.print(" G=");
  Serial.print(g);
  Serial.print(" B=");
  Serial.println(b);

  for (int i = LED_FITA2_INICIO; i <= LED_FITA2_FIM; i++) {
    strip.setPixelColor(i, strip.Color(r, g, b));
  }
  strip.show();
}

// === FUNÇÃO PARA APAGAR A LUZ DA PORTA ===
void apagarLuzPorta() {
  Serial.println("Apagando luz da porta...");
  for (int i = LED_FITA2_INICIO; i <= LED_FITA2_FIM; i++) {
    strip.setPixelColor(i, strip.Color(0, 0, 0));
  }
  strip.show();
  Serial.println("Luz da porta apagada.");
}

// === APAGAR TODAS AS LUZES ===
void apagarTudo() {
  apagarLuzPorta();
  desligarLuzInterna();
}

// === FUNÇÕES EXTRAS DE MENU ===
void funcao8() { Serial.println("Executando função 8"); }
void funcao9() { Serial.println("Executando função 9"); }

// === MENU SERIAL ===
void mostrarMenu() {
  Serial.println();
  Serial.println("=== MENU PRINCIPAL ===");
  Serial.println("1 - Trocar cor da porta");
  Serial.println("2 - Abrir Porta (servo2)");
  Serial.println("3 - Fechar Porta (servo2)");
  Serial.println("4 - Girar tranca esquerda (servo1)");
  Serial.println("5 - Girar tranca direita (servo1)");
  Serial.println("6 - Ligar Luz Interna");
  Serial.println("7 - Apagar luzes");
  Serial.println("8 - Executar função 8");
  Serial.println("9 - Executar função 9");
  Serial.println("======================");
  Serial.println("Digite o número da opção e pressione ENTER:");
  Serial.println();
}

// === SETUP ===
void setup() {
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);

  // Inicializa LEDs
  strip.begin();
  strip.show();

  // Inicializa servos
  servo1.attach(SERVO1_PIN, 500, 2400);
  servo2.attach(SERVO2_PIN, 500, 2400);

  // Lê posições armazenadas
  posicaoAtualServo1 = lerPosicaoServo(EEPROM_SERVO1_ADDR, 0);
  posicaoAtualServo2 = lerPosicaoServo(EEPROM_SERVO2_ADDR, anguloFechado, anguloAberto, anguloFechado);

  servo1.write(posicaoAtualServo1);
  servo2.write(posicaoAtualServo2);

  Serial.println("Sistema iniciado com sucesso.");
  mostrarMenu();
}

// === LOOP ===
void loop() {
  if (Serial.available() > 0) {
    char opcao = Serial.read();

    if (opcao == '\n' || opcao == '\r') return;

    Serial.print("Opção selecionada: ");
    Serial.println(opcao);

    switch (opcao) {
      case '1': trocarCorAleatoriaFita2(); break;
      case '2': abrirPortaSuave(); break;
      case '3': fecharPortaSuave(); break;
      case '4': abrirServo1(); break;
      case '5': fecharServo1(); break;
      case '6': ligarLuzInterna(); break;
      case '7': apagarTudo(); break;
      case '8': funcao8(); break;
      case '9': funcao9(); break;
      default:
        Serial.println("Opção inválida. Digite um número de 1 a 9.");
        break;
    }

    mostrarMenu();
  }
}