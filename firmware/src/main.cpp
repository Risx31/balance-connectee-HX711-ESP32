#include "Arduino.h"        // Bibliothèque de base pour l’ESP32 (fonctions Arduino)
#include "PubSubClient.h"   // Gère la communication MQTT (publish/subscribe)
#include "WiFi.h"           // Permet la connexion réseau (WiFi classique)
#include "esp_wpa2.h"       // Spécifique à la connexion Eduroam (authentification EAP)
#include "HX711.h"          // Bibliothèque du capteur de poids HX711 (ADC 24 bits)




//Configuration du protocole MQTT


// Adresse IP du broker de l’IUT
const char *mqtt_broker = "172.23.28.132"; //char (character) : langage C, stocke un caractère, ici l'adresse IP constante
const int mqtt_port = 1883;


// Topics utilisés pour la communication JSON
const char *topic_pub = "balance/masse"; // Publication des mesures
const char *topic_tare = "balance/tare"; // Réception d'une commande de tare
const char *topic_mode = "balance/mode"; // Réception du mode métrologique ("certificat" ou "constat")


// Déclaration des objets pour gérer la communication
WiFiClient espClient;          // Objet client WiFi
PubSubClient client(espClient); // Client MQTT basé sur la connexion WiFi




//Configuration du Wi-Fi Eduroam


#define EAP_IDENTITY "titouan.gardy-lognon@etu.univ-amu.fr" // Identifiant Eduroam
#define EAP_PASSWORD "**********"                           // Mot de passe Eduroam
#define EAP_USERNAME "titouan.gardy-lognon@etu.univ-amu.fr" // Nom d’utilisateur complet
const char *ssid = "eduroam";                               // Nom du réseau utilisé




//Configuration du capteur et du bouton tare


#define LOADCELL_DOUT_PIN 19  // Pin DATA du HX711
#define LOADCELL_SCK_PIN 21   // Pin CLOCK du HX711
#define TARE_BUTTON_PIN 0     // Pin du bouton BOOT utilisée pour la tare manuelle


HX711 scale;                  // Création d’un objet HX711
long tare_offset = 0;         // Offset de tare mémorisé




//Définition Métrologie


String mode_incert = "certificat"; // Mode d’évaluation de l’incertitude (modifiable par Node-Red)
float uj, uf, ur, um, U;           // Noms des incertitudes
float uj2, uf2, ur2;               // Carrés des incertitudes
float resolution = 0.001;          // Résolution de la balance (1 mg)
float u_etalon = 0.001;            // Incertitude-type de l’étalon
float EMT = 0.001;                 // Erreur maximale tolérée pour le constat




//Fonction de tare


void effectuerTare() {
  Serial.println("Tare en cours...");
  scale.tare();                    // Réinitialise la valeur moyenne à zéro
  tare_offset = scale.read_average(20); // Sauvegarde de la valeur moyenne (pour affichage/debug)
  Serial.println("Tare effectuée");
}


//MQTT (réception des messages)


void callback(char *topic, byte *payload, unsigned int length) { //nom du topic; contenu du message (tableau d'octets); taille du message reçu
  payload[length] = '\0';              // Ajoute le caractère de fin de chaîne du C et C++
  String message = String((char *)payload); // Convertit le tableau d'octets "payload" en chaîne string lisible


  // Commande de tare reçue, deux conditions : message vient bien du topic “balance/tare” et commande envoyée par Node-Red active
  if (String(topic) == topic_tare && message == "1") {
    effectuerTare(); //si condition satisfaite
  }


  // Changement de mode d’incertitude ("certificat" ou "constat")
  if (String(topic) == topic_mode) {                                    //message vient du topic "balance/mode"
    if (message == "certificat" || message == "constat") {             //Deux cas acceptés (II = OU logique)
      mode_incert = message;                                          //enregistre le mode sélectionné dans une variable globale mode_incert
      Serial.printf("🧾 Mode incertitude changé : %s\n", mode_incert.c_str());    //%s = string ; mode_incert.c_str() convertit la variable String en chaîne C (char *) pour être compatible avec printf
    }
  }
}


//Reconnexion automatique au broker MQTT


void reconnect() {
  while (!client.connected()) {                         //Tant que le client MQTT n’est pas connecté on exécute le bloc à l’intérieur
    String cid = "esp32-balance-" + WiFi.macAddress(); // ID unique basé sur la MAC
    Serial.print("Connexion MQTT...");
    if (client.connect(cid.c_str())) {                  // Tentative de connexion
      Serial.println(" ✅");
      client.subscribe(topic_tare); // S’abonne aux commandes de tare
      client.subscribe(topic_mode); // S’abonne aux changements de mode
    } else {
      Serial.print("Échec MQTT (code ");
      Serial.print(client.state());
      Serial.println(")");
      delay(2000); // Attente avant de réessayer
    }
  }
}


//SETUP


void setup() {
  Serial.begin(115200); // Démarrage de la communication série
  delay(500);
  Serial.println("\n=== Initialisation de la balance HX711 ===");


  // Configuration des broches
  pinMode(TARE_BUTTON_PIN, INPUT_PULLUP);
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);


  // Attente que le HX711 soit prêt
  Serial.print("Attente HX711...");
  while (!scale.is_ready()) {
    delay(100);
    Serial.print(".");
  }
  Serial.println(" ✅");


  scale.set_scale(1079.0); // Facteur d’étalonnage (déterminé expérimentalement)
  effectuerTare();         // Tare automatique au démarrage


  // --- Connexion au réseau WiFi Eduroam ---
  WiFi.disconnect(true);                    //coupe toute connexion Wi-Fi existante pour en établir une nouvelle
  WiFi.begin(ssid, WPA2_AUTH_PEAP, EAP_IDENTITY, EAP_USERNAME, EAP_PASSWORD);     //connexion au réseau Eduroam
  Serial.print("Connexion à Eduroam...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);           //Progression de la
    Serial.print(".");   //    connexion
  }
  Serial.println("\n✅ Wi-Fi connecté !");
  Serial.print("IP : ");
  Serial.println(WiFi.localIP());


  // --- Configuration MQTT ---
  client.setServer(mqtt_broker, mqtt_port);
  client.setCallback(callback);
  reconnect();


  Serial.println("Balance prête");
}




//Boucle principale : mesure et envoi




void loop() {
  if (!client.connected()) reconnect(); // Vérifie la connexion MQTT
  client.loop();                        // Gère les messages entrants


  // --- Tare manuelle (bouton BOOT) ---
  if (digitalRead(TARE_BUTTON_PIN) == LOW) {      //quand bouton appuyé, pin lit LOW
    effectuerTare();
    delay(1000);
  }


  // --- Lecture et traitement des mesures ---
  if (scale.is_ready()) {
    const int N = 10;       // Nombre de mesures pour la moyenne
    float somme = 0;
    float mesures[N];


    // Moyenne sur N mesures pour réduire le bruit
    for (int i = 0; i < N; i++) {     //répète N fois avec N le nombre de mesures à effectuer
      mesures[i] = scale.get_units(1); //(1) indique qu’on fait la moyenne sur une seule lecture brute (valeur instantanée)
      somme += mesures[i]; //on ajoute la valeur mesurée à la somme totale
      delay(20);
    }


    float moyenne = somme / N;




    // CALCUL DES INCERTITUDES MÉTROLOGIQUES


    // --- Écart-type expérimental (fidélité)
    float variance = 0;
    for (int i = 0; i < N; i++) variance += pow(mesures[i] - moyenne, 2);   // pour chaque mesure: calcul écart à la moyenne, met au carré, ajoute le résultat à la somme variance
    variance /= (N - 1);                                                   //formule variance experimentale (Bessel)
    float ecart_type = sqrt(variance);                                    //racine carrée de la variance pour obtenir l’écart-type


    // --- u_f : incertitude de répétabilité (fidélité)
    uf = ecart_type / sqrt(N);
    uf2 = pow(uf, 2);


    // --- u_r : incertitude liée à la résolution de la balance
    ur = resolution / (2 * sqrt(3));
    ur2 = pow(ur, 2);


    // --- u_j : incertitude-type globale selon la plage et le mode sélectionné
    if (mode_incert == "certificat") {


      // Valeurs extraites du certificat d’étalonnage
      if (moyenne <= 1) uj = 0.0069551;
      else if (moyenne <= 2) uj = 0.00695548/2;
      else if (moyenne <= 5) uj = 0.00695756/2;
      else if (moyenne <= 10) uj = 0.00696478/2;
      else if (moyenne <= 20) uj = 0.00699289/2;
      else if (moyenne <= 50) uj = 0.00718447/2;
      else if (moyenne <= 100) uj = 0.00782788/2;
      else uj = 0.01001084;
    } else { // Mode constat de vérification


      // Valeurs issues du constat de vérification
      if (moyenne <= 1) uj = 0.05783966;
      else if (moyenne <= 2) uj = 0.06936926;
      else if (moyenne <= 5) uj = 0.08667238;
      else if (moyenne <= 10) uj = 0.11552254;
      else if (moyenne <= 20) uj = 0.1443799;
      else if (moyenne <= 50) uj = 0.17324231;
      else if (moyenne <= 100) uj = 0.28870167;
      else uj = 0.57737192;
    }
    uj2 = pow(uj, 2);


    // --- Calcul final de u_m et U (incertitude élargie)
    if (mode_incert == "certificat") {
      // u_m = √(u_f² + u_j² + u_etalon² + u_r²)
      um = sqrt(uf2 + uj2 + pow(u_etalon, 2) + ur2);
    } else {
      // u_m = √(u_f² + u_j² + (EMT/√3)² + u_r²)
      um = sqrt(uf2 + uj2 + pow((EMT / sqrt(3)), 2) + ur2);
    }
    U = 2 * um; // Facteur de couverture k=2 (niveau de confiance ≈ 95%)


    // --- Affichage console pour vérification
    Serial.println("=== Calcul incertitude ===");
    Serial.printf("Mode : %s\n", mode_incert.c_str());
    Serial.printf("Masse = %.3f g\n", moyenne);
    Serial.printf("u_f = %.6f | u_j = %.3f | u_r = %.6f\n", uf, uj, ur);  //%.3f = trois chiffres après la virgule
    Serial.printf("u_m = %.6f g | U(k=2) = %.6f g\n", um, U);
    Serial.println("===========================");


    // --- Construction du message JSON à envoyer vers Node-RED
    String payload = "{\"masse\": " + String(moyenne, 3) +
                     ", \"incertitude\": " + String(um, 6) +
                     ", \"mode\": \"" + mode_incert + "\"}";
    client.publish(topic_pub, payload.c_str()); // Publication MQTT
  } else {
    Serial.println("⚠️ HX711 non prêt !");
  }


  delay(500); // Fréquence d’échantillonnage (2 mesures par seconde)
}



