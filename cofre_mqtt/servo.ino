// ===============================================
// Move servo suavemente
// ===============================================
void moverServoSuave(Servo &servo, int &posicaoAtual, int alvo, int delayTime) {
  LOGF("🔧 Movendo servo de %d° para %d°\n", posicaoAtual, alvo);

  if (alvo < 0) alvo = 0;
  if (alvo > 180) alvo = 180;

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
}

// ===============================================
void abrirPortaSuave() {
  LOG("🔓 Abrindo porta...");
  moverServoSuave(servo2, posicaoAtualServo2, anguloAberto, 10);
}

void fecharPortaSuave() {
  LOG("🔒 Fechando porta...");
  moverServoSuave(servo2, posicaoAtualServo2, anguloFechado, 10);
}

void abrirServo1() {
  moverServoSuave(servo1, posicaoAtualServo1, 30, 3);
}

void fecharServo1() {
  moverServoSuave(servo1, posicaoAtualServo1, 90, 3);
}

// ===============================================
void salvarPosicaoServo(int addr, int pos) {
  EEPROM.writeInt(addr, pos);
  EEPROM.commit();
}

// ===============================================
int lerPosicaoServo(int addr, int padrao, int minimo, int maximo) {
  int pos = EEPROM.readInt(addr);
  if (pos < minimo || pos > maximo) pos = padrao;
  return pos;
}
