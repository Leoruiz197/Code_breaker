// ===============================================
// Processa comandos do cofre
// ===============================================
void comandosCofre(String payload) {
  LOGF("🔎 Processando comando: %s\n", payload.c_str());

  StaticJsonDocument<400> doc;
  if (deserializeJson(doc, payload)) {
    LOG("Erro JSON no comando");
    return;
  }

  if (!doc.containsKey("comando")) {
    LOG("JSON sem campo 'comando'");
    return;
  }

  // === Comando simples ===
  if (doc["comando"].is<const char*>()) {
    String cmd = doc["comando"].as<String>();
    cmd.toLowerCase();

    LOGF("Executando comando simples: %s\n", cmd.c_str());

    if (cmd == "abrir") abrirPortaSuave();
    else if (cmd == "fechar") fecharPortaSuave();
    else if (cmd == "luz") ligarLuzInterna();
    else if (cmd == "apagar") apagarTudo();
    else if (cmd == "tranca_direita") abrirServo1();
    else if (cmd == "tranca_esquerda") fecharServo1();
    else if (cmd == "correta") {
      int r = doc["cor"]["R"] | corEquipeR;
      int g = doc["cor"]["G"] | corEquipeG;
      int b = doc["cor"]["B"] | corEquipeB;
      tentativaCorreta(r, g, b);
    }

    return;
  }

  // === Comando complexo ===
  JsonObject cmd = doc["comando"];

  bool tentando = cmd["tentando"] | false;
  int etapa = cmd["etapa"] | 1;

  int r = cmd["cor"]["R"] | corEquipeR;
  int g = cmd["cor"]["G"] | corEquipeG;
  int b = cmd["cor"]["B"] | corEquipeB;

  if (!tentando) {
    LOG("❌ Tentativa incorreta detectada");
    tentandoAbrir();
  } else {
    LOGF("✅ Registrando etapa %d com cor RGB(%d,%d,%d)\n", etapa, r, g, b);
    tentativaCorretaEtapa(etapa, r, g, b);
  }
}


// ===============================================
// Animação de tentativa incorreta
// ===============================================
void tentandoAbrir() {
  LOG("Aplicando efeito de tentativa incorreta...");

  for (int i = 0; i < 2; i++) {
    ledRed();
    abrirServo1();
    delay(150);

    ledOff();
    fecharServo1();
    delay(150);
  }
  restaurarEstadoEtapaAtual();
}
