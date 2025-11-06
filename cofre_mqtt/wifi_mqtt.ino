// ===============================================
// Conecta ao broker MQTT
// ===============================================
bool connectMQTT() {
  client.setServer(broker, 1883);
  client.setCallback(callback);

  for (byte tentativa = 1; tentativa <= 5; tentativa++) {
    LOGF("Conectando ao broker %s (tentativa %d/5)...\n", broker, tentativa);

    String client_id = "Cofre-" + String(WiFi.macAddress());

    if (client.connect(client_id.c_str())) {
      LOG("✅ Conectado ao broker MQTT!");

      client.subscribe(topic1.c_str());
      client.subscribe(topic2.c_str());

      LOGF("📡 Inscrito em: %s e %s\n", topic1.c_str(), topic2.c_str());

      client.publish(topic1.c_str(), "{\"status\":\"ESP32 conectado\"}");
      return true;
    }

    LOGF("❌ Falha MQTT (%d). Tentando novamente...\n", client.state());
    delay(2000);
  }

  LOG("❌ Não conectou ao broker.");
  return false;
}


// ===============================================
// Callback MQTT
// ===============================================
void callback(char *topic, byte *payload, unsigned int length) {
  String msg = "";
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];

  LOGF("\n📩 Recebido no tópico: %s\n", topic);
  LOGF("Conteúdo: %s\n", msg.c_str());

  if (String(topic) == topic1) comandosCofre(msg);
  else if (String(topic) == topic2) atualizarConfiguracoes(msg);
}
