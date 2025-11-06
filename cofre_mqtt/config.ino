// ===============================================
// Atualiza configurações via MQTT
// ===============================================
void atualizarConfiguracoes(String payload) {
  LOGF("⚙️ Atualizando configurações: %s\n", payload.c_str());

  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, payload)) {
    LOG("Erro JSON em atualização de config");
    return;
  }

  bool alterou = false;

  if (doc.containsKey("angulo-max")) {
    anguloFechado = doc["angulo-max"];
    LOGF("Novo ângulo fechado: %d\n", anguloFechado);
    alterou = true;
  }

  if (doc.containsKey("num_senhas")) {
    num_senhas = doc["num_senhas"];
    LOGF("Novo num_senhas: %d\n", num_senhas);
    alterou = true;
  }

  if (doc.containsKey("cor_equipe")) {
    corEquipeR = doc["cor_equipe"]["cor"]["R"];
    corEquipeG = doc["cor_equipe"]["cor"]["G"];
    corEquipeB = doc["cor_equipe"]["cor"]["B"];

    LOGF("Nova cor da equipe → R=%d G=%d B=%d\n", corEquipeR, corEquipeG, corEquipeB);

    aplicarCorEquipe();
    alterou = true;
  }

  if (alterou) {
    prefs.begin("config", false);
    prefs.putInt("anguloFechado", anguloFechado);
    prefs.putInt("num_senhas", num_senhas);
    prefs.putInt("corR", corEquipeR);
    prefs.putInt("corG", corEquipeG);
    prefs.putInt("corB", corEquipeB);
    prefs.end();
    LOG("✅ Configurações salvas na flash");
  }
}
