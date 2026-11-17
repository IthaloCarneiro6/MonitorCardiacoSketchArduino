#define PIN_AUDIO        25
#define PIN_BTN_ANT       4
#define PIN_BTN_PROX     16
#define PIN_BTN_MUDO     17
#define PIN_BTN_ANALISAR 18
#define PIN_LED_BATIDA   23
#define PIN_LED_ALARME   22

struct Condicao {
  const char* nome;
  const char* codigo;
  uint16_t bpm;
  uint8_t  irregular;
  bool     pvc;
  uint8_t  alarme;
};

const Condicao CONDICOES[] = {
  {"Ritmo Sinusal Normal",      "NSR",   75,  0, false, 0},
  {"Bradicardia Sinusal",       "BRAD",  42,  0, false, 1},
  {"Taquicardia Sinusal",       "TAQ",  135,  0, false, 1},
  {"Fibrilacao Atrial",         "AFIB", 110, 35, false, 1},
  {"Extrassistole Ventricular", "PVC",   78,  0, true,  1},
  {"Taquicardia Ventricular",   "VT",   180,  0, false, 2},
  {"Fibrilacao Ventricular",    "VF",     0,  0, false, 3},
  {"Assistolia",                "ASY",    0,  0, false, 3},
};
const uint8_t N_COND = sizeof(CONDICOES) / sizeof(CONDICOES[0]);

const uint16_t TOM_NORMAL = 440;
const uint16_t TOM_PVC    = 330;
const uint16_t DUR_BIP    = 40;

const uint16_t ALARME_MEDIO[] = {1047, 1319, 1568};
const uint16_t ALARME_ALTO[]  = {988, 784, 988};

const uint16_t SIRENE_MIN = 650;
const uint16_t SIRENE_MAX = 1350;
const uint16_t MEIO_CICLO_SIRENE = 500;
const uint8_t  PASSO_SIRENE = 10;

const uint16_t DUR_LED = 80;
const uint16_t PISCA_ALARME_SIRENE = 125;

const unsigned long TIMEOUT_PC = 3000;
const unsigned long DUR_GRAVACAO = 15000;

uint8_t condAtual = 0;
uint8_t nivelAlarme = 0;
bool mudo = false;
bool bipeMudo = false;
bool modoPC = false;

unsigned long proxBatida = 0;
uint16_t contBatidas = 1;
unsigned long proxAlarme = 0;
unsigned long proxPulso = 0;
uint8_t pulsosRestantes = 0;
uint8_t pulsoIdx = 0;
unsigned long ultimoBeatPC = 0;
unsigned long fimBip = 0;
unsigned long fimRajada = 0;
unsigned long ledDesligaEm = 0;
unsigned long proxTremor = 0;

bool sireneAtiva = false;
unsigned long inicioSirene = 0;
unsigned long proxPassoSirene = 0;

bool gravando = false;
unsigned long fimGravacao = 0;

String bufferSerial;

bool antProx = false, antAnt = false, antMudo = false, antAnalisar = false;
unsigned long tProx = 0, tAnt = 0, tMudo = 0, tAnalisar = 0;

void tocar(uint16_t freq, uint16_t dur) {
  tone(PIN_AUDIO, freq, dur);
}

void piscarLED(uint16_t dur) {
  digitalWrite(PIN_LED_BATIDA, HIGH);
  ledDesligaEm = millis() + dur;
}

bool alarmeTocando() {
  return sireneAtiva || pulsosRestantes > 0 || (long)(millis() - fimRajada) < 0;
}

void tocarBatida(bool ventricular) {
  piscarLED(DUR_LED);
  if (bipeMudo || alarmeTocando()) return;
  tocar(ventricular ? TOM_PVC : TOM_NORMAL, DUR_BIP);
  fimBip = millis() + DUR_BIP;
}

void pararSirene() {
  if (sireneAtiva) {
    noTone(PIN_AUDIO);
    sireneAtiva = false;
  }
}

void tocarSirene() {
  unsigned long agora = millis();
  if (!sireneAtiva) {
    sireneAtiva = true;
    inicioSirene = agora;
    proxPassoSirene = agora;
  }
  if ((long)(agora - proxPassoSirene) < 0) return;
  proxPassoSirene = agora + PASSO_SIRENE;

  unsigned long fase = (agora - inicioSirene) % (2UL * MEIO_CICLO_SIRENE);
  uint32_t faixa = SIRENE_MAX - SIRENE_MIN;
  uint16_t freq;
  if (fase < MEIO_CICLO_SIRENE) freq = SIRENE_MIN + faixa * fase / MEIO_CICLO_SIRENE;
  else                          freq = SIRENE_MAX - faixa * (fase - MEIO_CICLO_SIRENE) / MEIO_CICLO_SIRENE;
  tone(PIN_AUDIO, freq);
}

void chirpConfirmacao() {
  tocar(1200, 40); delay(50);
  tocar(1600, 40); delay(50);
}

void somResultado() {
  tocar(1568, 70); delay(90);
  tocar(2093, 120); delay(140);
}

void cancelarGravacao() {
  if (!gravando) return;
  gravando = false;
  Serial.println("REC:CANCEL");
}

void selecionarCondicao(uint8_t i, bool avisarPC) {
  if (i >= N_COND) return;
  cancelarGravacao();
  pararSirene();
  condAtual = i;
  nivelAlarme = 0;
  mudo = false;
  bipeMudo = false;
  contBatidas = 1;
  pulsosRestantes = 0;
  fimRajada = 0;
  fimBip = 0;
  chirpConfirmacao();
  proxBatida = millis() + 400;
  proxAlarme = millis() + 600;
  if (avisarPC) {
    Serial.print("COND:");
    Serial.print(i);
    Serial.print(':');
    Serial.print(CONDICOES[i].codigo);
    Serial.print(':');
    Serial.println(CONDICOES[i].nome);
  }
}

void alternarMudo() {
  if (!mudo) {
    mudo = true;
  } else if (!bipeMudo) {
    bipeMudo = true;
  } else {
    mudo = false;
    bipeMudo = false;
  }
  pulsosRestantes = 0;
  Serial.print("MUTE:");
  Serial.println(bipeMudo ? 2 : (mudo ? 1 : 0));
}

void iniciarGravacao() {
  if (gravando) return;
  pararSirene();
  pulsosRestantes = 0;
  tocar(784, 80);  delay(100);
  tocar(1175, 80); delay(100);
  tocar(1568, 120); delay(140);
  gravando = true;
  fimGravacao = millis() + DUR_GRAVACAO;
  Serial.println("REC:START");
}

void verificarGravacao() {
  if (gravando && millis() >= fimGravacao) {
    gravando = false;
    proxAlarme = millis();
    Serial.println("REC:END");
  }
}

void aplicarResultado(String cmd) {
  int sep = cmd.lastIndexOf(':');
  if (sep < 0) return;
  gravando = false;
  pararSirene();
  pulsosRestantes = 0;
  nivelAlarme = constrain(cmd.substring(sep + 1).toInt(), 0, 3);
  mudo = false;
  somResultado();
  proxAlarme = millis() + 300;
}

unsigned long intervaloRR(const Condicao& c) {
  unsigned long rr = 60000UL / c.bpm;
  if (c.irregular) {
    long v = (long)rr * c.irregular / 100;
    rr += random(-v, v + 1);
  }
  return rr;
}

void gerarBatimentoLocal() {
  const Condicao& c = CONDICOES[condAtual];
  if (modoPC || c.bpm == 0) return;

  unsigned long agora = millis();
  if ((long)(agora - proxBatida) < 0) return;

  unsigned long rr = intervaloRR(c);
  bool ehPVC = c.pvc && (contBatidas % 4 == 0);
  tocarBatida(ehPVC);

  contBatidas++;
  bool proxEhPVC = c.pvc && (contBatidas % 4 == 0);
  if (proxEhPVC)  proxBatida = agora + rr * 6 / 10;
  else if (ehPVC) proxBatida = agora + rr * 14 / 10;
  else            proxBatida = agora + rr;
}

void tremularFV() {
  if (strcmp(CONDICOES[condAtual].codigo, "VF") != 0) return;
  unsigned long agora = millis();
  if ((long)(agora - proxTremor) < 0) return;
  piscarLED(random(15, 50));
  proxTremor = agora + random(60, 220);
}

void tratarAlarme() {
  if (nivelAlarme == 0 || mudo || gravando) {
    pulsosRestantes = 0;
    pararSirene();
    return;
  }
  if (nivelAlarme == 3) {
    tocarSirene();
    return;
  }
  pararSirene();

  bool alto = (nivelAlarme == 2);
  const uint16_t* seq = alto ? ALARME_ALTO : ALARME_MEDIO;
  uint16_t dur   = alto ? 150 : 180;
  uint16_t pausa = alto ? 100 : 70;
  unsigned long agora = millis();

  if (pulsosRestantes == 0 && (long)(agora - proxAlarme) >= 0) {
    pulsosRestantes = 3;
    pulsoIdx = 0;
    proxPulso = ((long)(fimBip - agora) > 0) ? fimBip + 30 : agora;
    proxAlarme = agora + (alto ? 2500 : 4000);
  }
  if (pulsosRestantes && (long)(agora - proxPulso) >= 0) {
    tocar(seq[pulsoIdx], dur);
    pulsoIdx++;
    pulsosRestantes--;
    proxPulso = agora + dur + pausa;
    fimRajada = proxPulso + 150;
  }
}

void atualizarLEDs() {
  if (ledDesligaEm && (long)(millis() - ledDesligaEm) >= 0) {
    digitalWrite(PIN_LED_BATIDA, LOW);
    ledDesligaEm = 0;
  }

  bool alarmeAceso;
  if (nivelAlarme == 0 || mudo) alarmeAceso = false;
  else if (nivelAlarme == 3)    alarmeAceso = (millis() / PISCA_ALARME_SIRENE) % 2;
  else                          alarmeAceso = true;
  digitalWrite(PIN_LED_ALARME, alarmeAceso ? HIGH : LOW);
}

void processarComando(String cmd) {
  cmd.trim();
  if (cmd.startsWith("COND:")) {
    selecionarCondicao(cmd.substring(5).toInt(), false);
  } else if (cmd.startsWith("BEAT")) {
    ultimoBeatPC = millis();
    tocarBatida(cmd.endsWith(":V"));
  } else if (cmd.startsWith("ALARM:")) {
    nivelAlarme = constrain(cmd.substring(6).toInt(), 0, 3);
  } else if (cmd.startsWith("RESULT:")) {
    aplicarResultado(cmd);
  } else if (cmd == "REC") {
    iniciarGravacao();
  } else if (cmd == "MUTE") {
    alternarMudo();
  } else if (cmd == "PING") {
    Serial.println("PONG");
  }
}

void lerSerial() {
  while (Serial.available()) {
    char ch = Serial.read();
    if (ch == '\n') {
      processarComando(bufferSerial);
      bufferSerial = "";
    } else if (ch != '\r' && bufferSerial.length() < 64) {
      bufferSerial += ch;
    }
  }
}

bool botaoApertado(uint8_t pino, bool& anterior, unsigned long& t) {
  bool apertado = (digitalRead(pino) == LOW);
  if (apertado && !anterior && millis() - t > 200) {
    anterior = true;
    t = millis();
    return true;
  }
  if (!apertado) anterior = false;
  return false;
}

void lerBotoes() {
  if (botaoApertado(PIN_BTN_PROX, antProx, tProx)) selecionarCondicao((condAtual + 1) % N_COND, true);
  if (botaoApertado(PIN_BTN_ANT,  antAnt,  tAnt))  selecionarCondicao((condAtual + N_COND - 1) % N_COND, true);
  if (botaoApertado(PIN_BTN_MUDO, antMudo, tMudo)) alternarMudo();
  if (botaoApertado(PIN_BTN_ANALISAR, antAnalisar, tAnalisar)) iniciarGravacao();
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_BTN_PROX, INPUT_PULLUP);
  pinMode(PIN_BTN_ANT,  INPUT_PULLUP);
  pinMode(PIN_BTN_MUDO, INPUT_PULLUP);
  pinMode(PIN_BTN_ANALISAR, INPUT_PULLUP);
  pinMode(PIN_LED_BATIDA, OUTPUT);
  pinMode(PIN_LED_ALARME, OUTPUT);
  pinMode(PIN_AUDIO, OUTPUT);
  randomSeed(esp_random());

  selecionarCondicao(0, true);
}

void loop() {
  lerSerial();
  lerBotoes();

  bool agoraPC = ultimoBeatPC && (millis() - ultimoBeatPC < TIMEOUT_PC);
  if (agoraPC != modoPC) {
    modoPC = agoraPC;
    Serial.println(modoPC ? "MODE:PC" : "MODE:LOC");
  }

  verificarGravacao();
  tremularFV();
  gerarBatimentoLocal();
  tratarAlarme();
  atualizarLEDs();
}
