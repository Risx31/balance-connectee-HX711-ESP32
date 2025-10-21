# Balance connectée (ESP32 + HX711)

Projet de balance connectée basée sur une carte ESP32 et un amplificateur HX711.
Elle mesure la masse en temps réel, effectue la tare automatique ou manuelle, calcule les incertitudes de mesure et envoie les résultats via MQTT à un dashboard Node-RED.

---

# Objectif du projet
Concevoir une chaîne complète de mesure connectée permettant la vérification métrologique et le suivi en temps réel d’une balance de précision.

---

# Matériel utilisé
- ESP32 (uPesy WROOM ou équivalent)
- Module HX711 + cellule de charge 1 kg
- Plateau en plexiglas
- Masses étalons F1 (1 à 200g)
- Broker MQTT 
- Interface Node-RED

---

# Fonctionnalités principales
- Lecture du capteur HX711
- Tare automatique au démarrage + manuelle via bouton BOOT + Tare possible depuis Node-Red
- Calcul et affichage de la masse moyenne
- Estimation de l’incertitude combinée possible dans deux modes : Vérification ou Etalonnage :
  - u_f : répétabilité  
  - u_j : justesse  
  - u_r : résolution  
  - u_m et U(k=2)
- Envoi JSON MQTT → Node-RED

Exemple de trame JSON :
```json
{
  "masse": 123.456,
  "um": 0.006,
  "incertitude": 0.012,
  "mode": "constat"
}
