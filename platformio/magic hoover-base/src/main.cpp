/*
███████╗███╗   ███╗██╗  ██╗ ██████╗ ███████╗██╗   ██╗
██╔════╝████╗ ████║██║  ██║██╔═══██╗██╔════╝╚██╗ ██╔╝
███████╗██╔████╔██║███████║██║   ██║███████╗ ╚████╔╝
╚════██║██║╚██╔╝██║██╔══██║██║   ██║╚════██║  ╚██╔╝
███████║██║ ╚═╝ ██║██║  ██║╚██████╔╝███████║   ██║
╚══════╝╚═╝     ╚═╝╚═╝  ╚═╝ ╚═════╝ ╚══════╝   ╚═╝
Projet      : MAGIC HOOVER - RECEPTEUR
crée le     : 10/03/2025
Modifié le  : 15/03/2025
par         : David AUVRÉ alias SMHOSY

*** remerciement ***
http://electroniqueamateur.blogspot.com/2018/02/communication-rf-433-mhz-entre-attiny85.html
*/

#include <Arduino.h>

// defifinition du mode de debug
#define DEBUG false
#define Serial \
  if (DEBUG)   \
  Serial

#include <Manchester.h>

#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

// definition des pins
#define RX_PIN 4 // là où est branché le récepteur 433 MHz
#define LED_PIN 16
#define REL_PIN 5

// variable du buffer de réception
#define BUFFER_SIZE 22
uint8_t buffer[BUFFER_SIZE];

// Variable de l'IP fixe
IPAddress local_IP(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

// Création de l'objet AsyncWebServer sur le port 80
AsyncWebServer server(80);

// Variable d'état general
const uint8_t nb_trans = 8;
uint8_t state_trans[nb_trans];
uint8_t connected_trans[nb_trans];        // intensité relever Milli amperes
uint16_t trigger_on[nb_trans];            // valeur en Milli amperes de declenchement
const uint16_t defaut_trigger_on = 500;   // valeur par defaut du trigger_on soit 500Ma
uint16_t trigger_off[nb_trans];           // valeur en milli secondes pour arret
const uint16_t defaut_trigger_off = 3000; // valeur par defaut du trigger_off soit 3 secondes
uint64_t time_stop = 0;                   // enregistrement de l'arret avec la temporisation
uint8_t state_rel = 0;                    // enregistrement de l'arret avec la temporisation
uint8_t last_state_rel = 0;
uint8_t tension = 230; // tension par defaut du rèseau

// definition du fichier de configuration
const char *filename_config = "/config.json";

/* Fonction d'enregistrement de la config  */
void saveConfig()
{
  File file = LittleFS.open(filename_config, "w");
  Serial.print(F("Ouvertuture du fichier de configuration -> "));
  if (!file)
  {
    Serial.println(F("[ECHEC]"));
    return;
  }
  Serial.println(F("[OK]"));

  JsonDocument config;             // Attribution temporaire JsonDocument
  config["ref_voltage"] = tension; // en enregistre la tension du reseau
  JsonArray triggerOn = config["triggerOn"].to<JsonArray>();
  JsonArray triggerOff = config["triggerOff"].to<JsonArray>();
  JsonArray connected = config["connected"].to<JsonArray>();

  for (int i = 0; i < nb_trans; i++) // on enregistre les trigger definie
  {
    triggerOn.add(trigger_on[i]);
    triggerOff.add(trigger_off[i]);
    connected.add(connected_trans[i]);
  }

  Serial.print(F("Ecriture du fichier de configuration -> "));
  if (serializeJson(config, file) == 0)
  {
    Serial.println(F("[ECHEC]"));
    return;
  }
  Serial.println(F("[OK]"));
}
/*****************************************************************************/

/* Fonction de chargement de la config  */
void loadConfig()
{
  // on initialise l'etat des module
  for (int i = 0; i < nb_trans; i++)
  {
    state_trans[i] = 0;
  }
  if (LittleFS.exists(filename_config)) // on verifie que le fichier est present
  {
    File file = LittleFS.open(filename_config, "r");
    Serial.print(F("Ouvertuture du fichier de configuration -> "));
    if (!file)
    {
      Serial.println(F("[ECHEC]"));
      return;
    }
    Serial.println(F("[OK]"));

    JsonDocument config; // Attribution temporaire JsonDocument

    DeserializationError error = deserializeJson(config, file); // extration et verification d'erreur

    Serial.print(F("Extraction des données de configuration -> "));
    if (error)
    {
      Serial.println(F("[ECHEC]"));
      return;
    }
    Serial.println(F("[OK]"));

    tension = config["ref_voltage"];
    for (int i = 0; i < nb_trans; i++) // on enregistre les trigger definie
    {
      trigger_on[i] = config["triggerOn"][i];
      trigger_off[i] = config["triggerOff"][i];
      connected_trans[i] = 0;
    }
  }
  else // si pas de fichier de config on charge les valeurs par defaut
  {

    Serial.print(F("Fichier de configuration absent -> [CONFIG PAR DEFAUT] \n"));
    for (int i = 0; i < nb_trans; i++)
    {
      trigger_on[i] = defaut_trigger_on;
      trigger_off[i] = defaut_trigger_off;
      connected_trans[i] = 0;
    }
    saveConfig();
  }
}
/*****************************************************************************/

void setup()
{
  // Déclaration des pins
  pinMode(LED_PIN, OUTPUT);
  pinMode(REL_PIN, OUTPUT);
  digitalWrite(LED_PIN, 0);

  // mode console pour le debug
  Serial.begin(115200);
  Serial.print(F("\n\r\n\r"));
  delay(500);
  Serial.print(F("*** MAGIC HOOVER ***\n"));
  Serial.print(F("Module : Recepteur\n"));
  delay(1000);

  // initialisation la reception 433mhz
  Serial.print(F("Initialisation du module 433 Mhz -> "));
  man.setupReceive(RX_PIN, MAN_300);
  man.beginReceiveArray(BUFFER_SIZE, buffer);
  Serial.println(F("[OK]"));

  // initialisation de littlefs
  Serial.print(F("Initialisation l'espace de stockage LittleFS -> "));
  if (!LittleFS.begin())
  {
    Serial.println(F("[ECHEC]"));
    return;
  }
  Serial.println(F("[OK]"));

  // Configuration du point d'access
  Serial.print(F("Configuration du point d'access -> "));
  if (!WiFi.softAPConfig(local_IP, gateway, subnet))
  {
    Serial.println(F("[ECHEC]"));
    return;
  }
  Serial.println(F("[OK]"));

  // Démarrage du point d'access
  Serial.print(F("Demarrage du point d'access -> "));
  if (!WiFi.softAP("SMHOSY-TOOLS"))
  {
    Serial.println(F("[ECHEC]"));
    return;
  }
  Serial.println(F("[OK]"));
  // on charge la config
  loadConfig();

  server.on("/", HTTP_ANY, [](AsyncWebServerRequest *request)
            {
              request->send(LittleFS, "/index.html");
              Serial.println("\n*** nouveau client Connecté ***\n"); });

  /***   Requete element connecté   ***/
  server.on("/connect", HTTP_GET, [](AsyncWebServerRequest *request)
            { 
              String reponse = "{";
              reponse +="\"connected\":[";
              for (int i = 0; i < nb_trans; i++){
                reponse += connected_trans[i];
                reponse += ",";
              }
              // on supprime la derniere virgule
              reponse = reponse.substring(0, reponse.length() - 1);
              // on ferme le tableau
              reponse += "]}";
              request->send(200, "application/json", reponse); });

  /****************************************/

  /***   Requete enregistrement des config ****/
  server.on("/save", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              // action reset on redemarre la base
              if(request->hasParam("action") && request->getParam("action")->value() == "reset") {
                ESP.restart();
              }
              // on enregistre la tension
              if (request->hasParam("ref_voltage")) {
                tension = request->getParam("ref_voltage")->value().toInt();
              }
              // on enregistre les valeur Ma et Msec
              if (request->hasParam("id")) {
                trigger_on[request->getParam("id")->value().toInt()] = request->getParam("trigger_on")->value().toInt();
                trigger_off[request->getParam("id")->value().toInt()] = request->getParam("trigger_off")->value().toInt();
              }
                saveConfig();
request->send(200, "application/json","[{\"result\": 1 }]" ); });

  /********************************************/

  // On renvoie toute les requetes web vers le SPIFFS
  server.serveStatic("/", LittleFS, "/");

  // Démarrage du serveur WEB
  Serial.print(F("Demarrage du serveur WEB -> "));
  server.begin();
  Serial.println(F("[OK]"));
  delay(100);
  // affichage de l'adresse IP
  Serial.print(F("IP du point d'acces = "));
  Serial.println(WiFi.softAPIP());

  digitalWrite(LED_PIN, 1);
}

void loop()
{
  // on verifie la recption sur le module 433 Mhz
  if (man.receiveComplete())
  {
    digitalWrite(LED_PIN, 0);
    int adr_trans = buffer[1];
    int valeur = (buffer[2] << 8) + buffer[3]; // on combine les deux bytes pour former la valeur analogique mesurée
    // on enregistre la precence du module
    connected_trans[adr_trans] = 1;

    if (valeur >= trigger_on[adr_trans])
    { // seuil de détection atteint
      Serial.print(F("Démarrage détecté sur le module -> "));
      Serial.println(adr_trans);
      state_trans[adr_trans] = 1;
    }
    else
    {

      Serial.print(F("Arret détecté sur le module -> "));
      Serial.println(adr_trans);
      state_trans[adr_trans] = 0;
      // on enregistre l'arret du relais sur timer
      time_stop = millis() + trigger_off[adr_trans];
    }
    // prépare le buffer pour la prochaine reception
    man.beginReceiveArray(BUFFER_SIZE, buffer);
    digitalWrite(LED_PIN, 1);
  }

  // verifie l'état des transmetteurs
  state_rel = 0;
  for (int i = 0; i < nb_trans; i++)
  {
    state_rel = state_rel + state_trans[i];
  }
  // on agit suivant l'état
  if (state_rel >= 1)
  {
    // on evite la redondance
    if (last_state_rel == 0)
    {
      Serial.println(F("Démarrage de l'aspirateur"));
      last_state_rel = 1;
      digitalWrite(REL_PIN, 1);
    }
  }
  else
  {
    if (millis() >= time_stop && last_state_rel == 1)
    {
      Serial.println(F("Arret de l'aspirateur"));
      last_state_rel = 0;
      digitalWrite(REL_PIN, 0);
    }
  }

  delay(10);
}