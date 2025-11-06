// ===============================================
// Monta tópico MQTT com subtopico opcional
// ===============================================
String montarTopico(String cofreTopico, String subTopico) {
  String t = "fiap/" + cofreTopico;
  if (subTopico.length() > 0) t += "/" + subTopico;
  LOGF("Tópico montado: %s\n", t.c_str());
  return t;
}
