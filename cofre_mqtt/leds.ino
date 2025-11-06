// ===============================================
void aplicarCorEquipe() {
  LOGF("Aplicando cor padrão da equipe R=%d G=%d B=%d\n",
        corEquipeR, corEquipeG, corEquipeB);

  for (int i = LED_FITA2_INICIO; i <= LED_FITA2_FIM; i++)
    strip.setPixelColor(i, strip.Color(corEquipeR, corEquipeG, corEquipeB));

  strip.show();
}

// ===============================================
void ligarLuzInterna() {
  LOG("Luz interna ligada.");
  for (int i = LED_FITA1_INICIO; i <= LED_FITA1_FIM; i++)
    strip.setPixelColor(i, strip.Color(255, 255, 255));
  strip.show();
}

void desligarLuzInterna() {
  LOG("Luz interna desligada.");
  for (int i = LED_FITA1_INICIO; i <= LED_FITA1_FIM; i++)
    strip.setPixelColor(i, 0);
  strip.show();
}

// ===============================================
void ledGreen() {
  LOG("LEDs verdes");
  for (int i = LED_FITA2_INICIO; i <= LED_FITA2_FIM; i++)
    strip.setPixelColor(i, strip.Color(0, 255, 0));
  strip.show();
}

void ledRed() {
  LOG("LEDs vermelhos");
  for (int i = LED_FITA2_INICIO; i <= LED_FITA2_FIM; i++)
    strip.setPixelColor(i, strip.Color(255, 0, 0));
  strip.show();
}

void ledOff() {
  LOG("LEDs apagados");
  for (int i = LED_FITA2_INICIO; i <= LED_FITA2_FIM; i++)
    strip.setPixelColor(i, 0);
  strip.show();
}

// ===============================================
void tentativaCorreta(int r, int g, int b) {
  LOGF("✅ Tentativa correta RGB(%d,%d,%d)\n", r, g, b);

  for (int i = 0; i < 4; i++) {
    ledGreen();
    delay(200);
    ledOff();
    delay(150);
  }

  abrirServo1();
  ligarLuzInterna();
  abrirPortaSuave();

  for (int i = LED_FITA2_INICIO; i <= LED_FITA2_FIM; i++)
    strip.setPixelColor(i, strip.Color(r, g, b));

  strip.show();
}

// ===============================================
void tentativaCorretaEtapa(int etapa, int r, int g, int b) {
  LOGF("✅ Etapa %d/%d — RGB(%d,%d,%d)\n", etapa, num_senhas, r, g, b);

  int total = LED_FITA2_FIM - LED_FITA2_INICIO + 1;
  int ledsEtapa = total / num_senhas;
  int limite = LED_FITA2_INICIO + (etapa * ledsEtapa) - 1;

  if (limite > LED_FITA2_FIM) limite = LED_FITA2_FIM;

  LOGF("Acendendo LEDs até %d\n", limite);

  for (int i = LED_FITA2_INICIO; i <= LED_FITA2_FIM; i++) {
    if (i <= limite)
      strip.setPixelColor(i, strip.Color(r, g, b));
    else
      strip.setPixelColor(i, strip.Color(corEquipeR, corEquipeG, corEquipeB));
  }

  strip.show();

  etapaAtual = etapa;

  // salva cor da etapa
  etapaCorR[etapa] = r;
  etapaCorG[etapa] = g;
  etapaCorB[etapa] = b;
}

// ===============================================
void trocarCorAleatoriaFita2() {
  int r = random(256), g = random(256), b = random(256);
  LOGF("Cor aleatória: R=%d G=%d B=%d\n", r, g, b);

  for (int i = LED_FITA2_INICIO; i <= LED_FITA2_FIM; i++)
    strip.setPixelColor(i, strip.Color(r, g, b));

  strip.show();
}

// ===============================================
void apagarTudo() {
  LOG("Apagando todos LEDs.");
  for (int i = 0; i < LED_TOTAL; i++)
    strip.setPixelColor(i, 0);
  strip.show();
}

void resetEtapas() {
  etapaAtual = 0;
  for (int i = 1; i <= num_senhas; i++) {
    etapaCorR[i] = corEquipeR;
    etapaCorG[i] = corEquipeG;
    etapaCorB[i] = corEquipeB;
  }
  aplicarCorEquipe();
}

// ===============================================
// Restaura os LEDs de acordo com a última etapa registrada
// ===============================================
void restaurarEstadoEtapaAtual() {
  LOGF("🔄 Restaurando LEDs da etapa atual (%d/%d)\n", etapaAtual, num_senhas);

  int total = LED_FITA2_FIM - LED_FITA2_INICIO + 1;
  int ledsPorEtapa = total / num_senhas;

  int ledIndex = LED_FITA2_INICIO;

  for (int etapa = 1; etapa <= num_senhas; etapa++) {

    int fimEtapa = ledIndex + ledsPorEtapa - 1;

    if (etapa == num_senhas)
      fimEtapa = LED_FITA2_FIM; // pega sobra

    for (int i = ledIndex; i <= fimEtapa; i++) {

      if (etapa <= etapaAtual) {
        // LEDs de etapas conquistadas → COR DA TENTATIVA QUE CONQUISTOU
        strip.setPixelColor(i, strip.Color(
          etapaCorR[etapa],
          etapaCorG[etapa],
          etapaCorB[etapa]
        ));
      } else {
        // LEDs ainda não conquistados → COR DO COFRE
        strip.setPixelColor(i, strip.Color(
          corEquipeR,
          corEquipeG,
          corEquipeB
        ));
      }
    }

    ledIndex = fimEtapa + 1;
  }

  strip.show();
}
