/* =========================================================
   Projet  : Enregistreur SLT (ESP32-S3 + DS1302 + Wi-Fi + LittleFS)
   Auteur  : [A.PRIVALOV]
   Date    : [24/09/2025]
   Version : [1.4]

   🛠️ Bibliothèques à installer :
   ---------------------------------------------------------
   - ESP32 Board Support ("XIAO_ESP32S3")
   - WiFi (inclus)
   - WebServer (inclus)
   - FS (inclus)
   - LittleFS_esp32
   - Preferences (inclus)
   - Ds1302 (Henning Karlsen ou équivalent)
   ========================================================= */

#include <WiFi.h>
#include <WebServer.h>
#include <Ds1302.h>
#include <Preferences.h>
#include <FS.h>
#include <LittleFS.h>

// =========================================================
// === CONFIG UTILISATEUR ==================================
const unsigned long WIFI_DURATION_MINUTES = 60; // durée WiFi actif après boot

// Broches RTC DS1302
#define RTC_CLK 2
#define RTC_DAT 1
#define RTC_RST 3

// Broches entrées impulsions
#define PIN_V1 9
#define PIN_V2 8

// =========================================================
// === WIFI ================================================
const char* ssid = "Enregistreur_SLT";
const char* password = "Mainta2020*";

// =========================================================
// === OBJETS ==============================================
WebServer server(80);
Ds1302 rtc(RTC_RST, RTC_CLK, RTC_DAT);
Preferences prefs;

// Compteurs
int compteur_V1 = 0, compteur_V2 = 0;
unsigned long tempsDebutZero_V1 = 0, tempsDebutZero_V2 = 0;
bool attenteZero_V1 = false, attenteZero_V2 = false;
bool doitRemonter_V1 = false, doitRemonter_V2 = false;
bool ignorerPremier_V1 = false, ignorerPremier_V2 = false;

// Anti-rebond
unsigned long dernierCptV1 = 0;
unsigned long dernierCptV2 = 0;
const unsigned long tempoComptage = 500; // 500ms anti-rebond

// Gestion reset journalier
int resetHour = 0, resetMinute = 0, resetSecond = 0;
static bool resetFait = false; // flag de reset par jour
int lastResetDay = -1;

// Gestion WiFi
unsigned long wifiStartTime = 0;
bool wifiActif = false;

// Correction progressive RTC (ESP32 plus lent)
unsigned long lastCorrectionMillis = 0;
const long correctionParSeconde_us = -4166; // retirer ~15s/h

// =========================================================
// === OUTILS TEMPS ========================================

// Fonction pour savoir si une année est bissextile
bool estBissextile(int annee) {
  return ((annee % 4 == 0 && annee % 100 != 0) || (annee % 400 == 0));
}

// Nombre de jours dans un mois donné
int joursDansMois(int mois, int annee) {
  switch (mois) {
    case 1: case 3: case 5: case 7: case 8: case 10: case 12: return 31;
    case 4: case 6: case 9: case 11: return 30;
    case 2: return estBissextile(annee) ? 29 : 28;
    default: return 30;
  }
}

// Calcul de la date de la veille
void dateVeille(int &jour, int &mois, int &annee) {
  jour--;
  if (jour == 0) {
    mois--;
    if (mois == 0) { mois = 12; annee--; }
    jour = joursDansMois(mois, annee);
  }
}

// =========================================================
// === STYLE CSS ===========================================
String commonCSS() {
  return R"rawliteral(
    <style>
      body {
        margin: 0; padding: 20px;
        font-family: monospace;
        background: #2b6777; color: #ffffff;
        text-align: center;
      }
      h1 { font-size: 2em; margin-bottom: 20px; }
      .info, #clock { font-size: 1.6em; margin: 10px 0; }
      .btn {
        padding: 10px 15px;
        background: #f2f2f2; color: #000;
        border: none; font-size: 1em;
        margin: 5px; cursor: pointer;
        border-radius: 5px;
      }
      .btn:hover { background: #cccccc; }
      .btn-container {
        display: flex;
        flex-wrap: wrap;
        justify-content: center;
        gap: 8px;
      }
      pre { margin-top: 30px; font-size: 0.8em; }
    </style>
  )rawliteral";
}

// =========================================================
// === PAGES HTML ==========================================
String htmlPage() {
  String html = R"rawliteral(
  <!DOCTYPE html><html><head>
    <meta charset='utf-8'>
    <meta name='viewport' content='width=device-width, initial-scale=1.0'>
    <title>Enregistreur SLT</title>
  )rawliteral";

  html += commonCSS();

  html += R"rawliteral(
    <script>
      function updateData() {
        fetch('/data')
          .then(res => res.json())
          .then(data => {
            document.getElementById('clock').innerText = data.horloge;
            document.getElementById('v1').innerText = "V1 : " + data.v1;
            document.getElementById('v2').innerText = "V2 : " + data.v2;
            document.getElementById('reset').innerText = "Reset à : " + data.reset;
          });
      }
      setInterval(updateData, 1000);
      function resetLogs() {
        if (confirm("Réinitialiser compteurs + effacer logs ?")) {
          fetch('/reset').then(() => updateData());
        }
      }
    </script></head><body onload="updateData()">
    <h1>Enregistreur SLT</h1>
    <div id="clock">--:--:--</div>
    <div class="info" id="v1">V1 : ...</div>
    <div class="info" id="v2">V2 : ...</div>
    <div class="info" id="reset">Reset à : ...</div>
    <div class="btn-container">
      <button class="btn" onclick="resetLogs()">🔄 Reset Logs</button>
      <a href="/manual"><button class="btn">🕑 Régler l'heure</button></a>
      <a href="/reset-time"><button class="btn">🔄 Régler reset</button></a>
      <a href="/logs.txt"><button class="btn">📄 Journalier</button></a>
      <a href="/logs.csv"><button class="btn">💾</button></a>
      <a href="/full_logs.txt"><button class="btn">📄 Full Logs</button></a>
      <a href="/full_logs.csv"><button class="btn">💾</button></a>
    </div>
    <pre>
                       :::       ::::::::      
                   :+:       :+:     :+:    
           +:+  +:+               +:+     
        +#+     +:+             +#+          
       +#+#+#+#+#+#+#+       +#+             
  #+#         #+#    
          ###        #############     
    </pre>
    <h2>Wifi restera actif 1h après ON/OFF</h2>
  </body></html>)rawliteral";

  return html;
}

String htmlManualPage() {
  String html = "<!DOCTYPE html><html><head><meta charset='utf-8'><title>Régler l'heure</title>";
  html += commonCSS();
  html += "</head><body><h1>Régler l'heure</h1>";
  html += "<form action='/set' method='POST'>"
          "JJ: <input type='number' name='jj' min='1' max='31' required> / "
          "MM: <input type='number' name='mm' min='1' max='12' required> / "
          "AAAA: <input type='number' name='aa' min='2000' max='2099' required><br>"
          "HH: <input type='number' name='hh' min='0' max='23' required> : "
          "MIN: <input type='number' name='min' min='0' max='59' required> : "
          "SS: <input type='number' name='ss' min='0' max='59' required><br>"
          "<input class='btn' type='submit' value='Définir'>"
          "</form><a href='/'><button class='btn'>← Retour</button></a></body></html>";
  return html;
}

String htmlResetTimePage() {
  String html = "<!DOCTYPE html><html><head><meta charset='utf-8'><title>Régler reset</title>";
  html += commonCSS();
  html += "</head><body><h1>Régler heure du reset</h1>";
  html += "<form action='/set-reset' method='POST'>"
          "HH: <input type='number' name='hh' min='0' max='23' required> : "
          "MIN: <input type='number' name='min' min='0' max='59' required> : "
          "SS: <input type='number' name='ss' min='0' max='59' required><br>"
          "<input class='btn' type='submit' value='Définir'>"
          "</form><a href='/'><button class='btn'>← Retour</button></a></body></html>";
  return html;
}

// =========================================================
// === HANDLERS SERVEUR ====================================
void handleRoot() { server.send(200, "text/html", htmlPage()); }
void handleManual() { server.send(200, "text/html", htmlManualPage()); }
void handleResetTime() { server.send(200, "text/html", htmlResetTimePage()); }

void handleSetTime() {
  int jj = server.arg("jj").toInt();
  int mm = server.arg("mm").toInt();
  int aa = server.arg("aa").toInt();
  int hh = server.arg("hh").toInt();
  int mn = server.arg("min").toInt();
  int ss = server.arg("ss").toInt();

  Ds1302::DateTime dt = { .year=aa-2000, .month=mm, .day=jj,
                          .hour=hh, .minute=mn, .second=ss, .dow=1 };
  rtc.setDateTime(&dt);
  server.sendHeader("Location","/",true); server.send(302,"OK");
}

void handleSetReset() {
  resetHour = server.arg("hh").toInt();
  resetMinute = server.arg("min").toInt();
  resetSecond = server.arg("ss").toInt();
  prefs.putInt("resetHour", resetHour);
  prefs.putInt("resetMinute", resetMinute);
  prefs.putInt("resetSecond", resetSecond);
  server.sendHeader("Location","/",true); server.send(302,"OK");
}

void handleResetLogs() {
  compteur_V1 = compteur_V2 = 0;
  prefs.putInt("v1",0); prefs.putInt("v2",0);
  LittleFS.remove("/logs.txt");
  LittleFS.remove("/logs.csv");
  LittleFS.remove("/full_logs.txt");
  LittleFS.remove("/full_logs.csv");
  server.send(200,"text/plain","Reset complet");
}

void handleData() {
  Ds1302::DateTime now; rtc.getDateTime(&now);
  char horloge[32], resetStr[16];
  sprintf(horloge,"%02d/%02d/%04d %02d:%02d:%02d", now.day,now.month,2000+now.year,
          now.hour,now.minute,now.second);
  sprintf(resetStr,"%02d:%02d:%02d", resetHour,resetMinute,resetSecond);
  String json="{";
  json+="\"horloge\":\""+String(horloge)+"\",";
  json+="\"v1\":"+String(compteur_V1)+",";
  json+="\"v2\":"+String(compteur_V2)+",";
  json+="\"reset\":\""+String(resetStr)+"\"}";
  server.send(200,"application/json",json);
}

void handleTXT() {
  if (!LittleFS.exists("/logs.txt")) { server.send(200,"text/plain","Aucun log"); return; }
  File f=LittleFS.open("/logs.txt","r"); server.streamFile(f,"text/plain"); f.close();
}
void handleCSV() {
  if (!LittleFS.exists("/logs.csv")) { server.send(200,"text/csv","Aucun log"); return; }
  File f=LittleFS.open("/logs.csv","r"); server.streamFile(f,"text/csv"); f.close();
}
void handleFullTXT() {
  if (!LittleFS.exists("/full_logs.txt")) { server.send(200,"text/plain","Aucun log"); return; }
  File f=LittleFS.open("/full_logs.txt","r"); server.streamFile(f,"text/plain"); f.close();
}
void handleFullCSV() {
  if (!LittleFS.exists("/full_logs.csv")) { server.send(200,"text/csv","Aucun log"); return; }
  File f=LittleFS.open("/full_logs.csv","r"); server.streamFile(f,"text/csv"); f.close();
}

// =========================================================
// === LOG FULL ============================================
void logFull(Ds1302::DateTime now, int v1, int v2) {
  bool headerNeeded1 = !LittleFS.exists("/full_logs.txt");
  bool headerNeeded2 = !LittleFS.exists("/full_logs.csv");
  File f1=LittleFS.open("/full_logs.txt","a"); 
  File f2=LittleFS.open("/full_logs.csv","a");
  if (headerNeeded1 && f1) f1.printf("%-10s %-8s %-6s %-6s\n","DATE","HEURE","V1","V2");
  if (headerNeeded2 && f2) f2.print("DATE;HEURE;V1;V2\n");
  char heureStr[9]; sprintf(heureStr,"%02d:%02d:%02d", now.hour,now.minute,now.second);
  if (f1){ f1.printf("%02d/%02d/%04d %-8s %-6d %-6d\n",
    now.day,now.month,2000+now.year,heureStr,v1,v2); f1.close();}
  if (f2){ f2.printf("%02d/%02d/%04d;%02d:%02d:%02d;%d;%d\n",
    now.day,now.month,2000+now.year,now.hour,now.minute,now.second,v1,v2); f2.close();}
}

// =========================================================
// === WIFI ================================================
void activerWiFi() {
  WiFi.mode(WIFI_AP); WiFi.softAP(ssid,password);
  Serial.println("WiFi actif, IP : "+WiFi.softAPIP().toString());
  wifiStartTime = millis(); wifiActif = true; server.begin();
}
void couperWiFi() {
  Serial.println("WiFi coupé"); server.stop(); WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF); wifiActif=false;
}

// =========================================================
// === SETUP ===============================================
void setup() {
  Serial.begin(115200); rtc.init();
  pinMode(PIN_V1,INPUT_PULLUP); pinMode(PIN_V2,INPUT_PULLUP);
  prefs.begin("compteurs",false);
  compteur_V1 = prefs.getInt("v1",0); compteur_V2 = prefs.getInt("v2",0);
  resetHour=prefs.getInt("resetHour",0); resetMinute=prefs.getInt("resetMinute",0); resetSecond=prefs.getInt("resetSecond",0);
  LittleFS.begin(true);
  activerWiFi();

  Ds1302::DateTime now; rtc.getDateTime(&now);
  lastResetDay = now.day;

  // Routes serveur
  server.on("/",handleRoot);
  server.on("/manual",handleManual);
  server.on("/set",HTTP_POST,handleSetTime);
  server.on("/reset-time",handleResetTime);
  server.on("/set-reset",HTTP_POST,handleSetReset);
  server.on("/reset",handleResetLogs);
  server.on("/data",handleData);
  server.on("/logs.txt",handleTXT);
  server.on("/logs.csv",handleCSV);
  server.on("/full_logs.txt",handleFullTXT);
  server.on("/full_logs.csv",handleFullCSV);
}

// =========================================================
// === LOOP ================================================
void loop() {
  Ds1302::DateTime now; rtc.getDateTime(&now);

  // --- WiFi auto OFF ---
  if (wifiActif && millis()-wifiStartTime > WIFI_DURATION_MINUTES*60UL*1000UL) couperWiFi();
  if (wifiActif) server.handleClient();

  // --- Correction progressive RTC ---
  if (millis() - lastCorrectionMillis >= 1000) { // toutes les secondes
    lastCorrectionMillis = millis();
    struct timeval tv;
    gettimeofday(&tv, NULL);
    tv.tv_usec += correctionParSeconde_us;
    if (tv.tv_usec >= 1000000) {
      tv.tv_sec += tv.tv_usec / 1000000;
      tv.tv_usec = tv.tv_usec % 1000000;
    }
    if (tv.tv_usec < 0) {
      tv.tv_sec -= 1;
      tv.tv_usec += 1000000;
    }
    settimeofday(&tv, NULL);
  }

  // --- Reset journalier robuste ---
  if (now.hour==resetHour && now.minute==resetMinute && abs(now.second-resetSecond) <= 3) {
    if (!resetFait) {
      bool header=!LittleFS.exists("/logs.txt");
      int j = now.day, m = now.month, a = 2000+now.year;
      dateVeille(j,m,a); // on prend la veille
      File f1=LittleFS.open("/logs.txt","a"); File f2=LittleFS.open("/logs.csv","a");
      if (f1) {
        if (header) f1.printf("%-10s %-6s %-6s\n","DATE","V1","V2");
        f1.printf("%02d/%02d/%04d   %-6d %-6d\n", j, m, a, compteur_V1, compteur_V2);
        f1.close();
      }
      if (f2) {
        if (header) f2.print("DATE;V1;V2\n");
        f2.printf("%02d/%02d/%04d;%d;%d\n", j, m, a, compteur_V1, compteur_V2);
        f2.close();
      }
      compteur_V1=compteur_V2=0; prefs.putInt("v1",0); prefs.putInt("v2",0);
      resetFait = true;
    }
  } else {
    resetFait = false;
  }

  // --- Comptage V1 ---
  bool etat_V1=digitalRead(PIN_V1);
  if (etat_V1==LOW) {
    if (!attenteZero_V1 && !doitRemonter_V1) { tempsDebutZero_V1=millis(); attenteZero_V1=true; }
    else if (attenteZero_V1 && millis()-tempsDebutZero_V1>=1) {
      if (!ignorerPremier_V1 && (millis()-dernierCptV1>tempoComptage)) {
        compteur_V1++; prefs.putInt("v1",compteur_V1); dernierCptV1=millis();
        logFull(now, compteur_V1, compteur_V2);
      }
      ignorerPremier_V1=false; attenteZero_V1=false; doitRemonter_V1=true;
    }
  } else doitRemonter_V1=false;

  // --- Comptage V2 ---
  bool etat_V2=digitalRead(PIN_V2);
  if (etat_V2==LOW) {
    if (!attenteZero_V2 && !doitRemonter_V2) { tempsDebutZero_V2=millis(); attenteZero_V2=true; }
    else if (attenteZero_V2 && millis()-tempsDebutZero_V2>=1) {
      if (!ignorerPremier_V2 && (millis()-dernierCptV2>tempoComptage)) {
        compteur_V2++; prefs.putInt("v2",compteur_V2); dernierCptV2=millis();
        logFull(now, compteur_V1, compteur_V2);
      }
      ignorerPremier_V2=false; attenteZero_V2=false; doitRemonter_V2=true;
    }
  } else doitRemonter_V2=false;

  delay(2); // petit tempo CPU
}
