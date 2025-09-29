# Enregistreur SLT (ESP32-S3 + DS1302 + Wi-Fi + LittleFS)

## 1. Présentation du projet
Ce projet implémente un enregistreur d’impulsions sur ESP32-S3, avec sauvegarde dans la mémoire flash interne via **LittleFS**, gestion des compteurs persistants et possibilité d’export des journaux au format texte et CSV via une interface web.  
Un module **RTC DS1302** est utilisé afin de garantir la précision horaire.

---

## 2. Matériel requis
- Carte **Seeed Studio XIAO ESP32-S3**  
- Module **RTC DS1302** avec pile de sauvegarde  
- Deux **optocoupleurs** connectés sur les entrées impulsions (GPIO9 et GPIO8)  
- Connexion **USB-C** pour l’alimentation et le téléversement  

---

## 3. Installation de l’environnement

### 3.1 Arduino IDE
Télécharger et installer la dernière version de l’IDE Arduino :  
👉 [https://www.arduino.cc/en/software](https://www.arduino.cc/en/software)

### 3.2 Ajout du support ESP32
1. Ouvrir **Arduino IDE** → *Fichier* → *Préférences*.  
2. Dans « URL de gestionnaire de cartes supplémentaires », ajouter :  
   ```
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
   ```
3. Aller dans *Outils* → *Type de carte* → *Gestionnaire de cartes*.  
4. Rechercher et installer : **esp32 by Espressif Systems** (≥ 2.0.11).  
5. Sélectionner la carte :  
   ```
   XIAO_ESP32S3
   ```

---

## 4. Bibliothèques nécessaires
À installer via le **Gestionnaire de bibliothèques Arduino** :

- **WiFi** (inclus avec ESP32)  
- **WebServer** (inclus avec ESP32)  
- **Preferences** (inclus avec ESP32)  
- **FS** (inclus avec ESP32)  
- **LittleFS_esp32** (auteur : lorol / espressif)  
- **Ds1302** (auteur : Henning Karlsen ou équivalent)  

---

## 5. Configuration de la carte
Dans le menu **Outils** :

- Carte : `XIAO_ESP32S3`  
- Upload Speed : `921600`  
- Flash Size : `8MB`  
- Partition Scheme :  
  ```
  Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)
  ```
  (ou toute partition compatible avec LittleFS)  

---

## 6. Téléversement
1. Brancher la carte en **USB-C**.  
2. Sélectionner le **port série** correspondant dans *Outils → Port*.  
3. Compiler et téléverser le fichier :  
   ```
   sketch_apr2a.ino
   ```

---

## 7. Utilisation
Après démarrage, l’ESP32 crée un **point d’accès Wi-Fi** :

- **SSID** : `Enregistreur_SLT`  
- **Mot de passe** : `A définir`  
- **Adresse IP par défaut** : `192.168.4.1`  

### Interface web
Accessible depuis un navigateur à l’adresse :  
```
http://192.168.4.1
```

### Fonctionnalités disponibles
- Réglage manuel de l’heure (RTC).  
- Réglage de l’heure de reset journalier des compteurs.  
- Consultation et téléchargement des journaux journaliers et complets (**TXT/CSV**).  
- Remise à zéro manuelle des compteurs et suppression des journaux.  

---

## 8. Notes techniques
- Les journaux sont enregistrés dans **LittleFS** (mémoire flash interne de l’ESP32).  
- Les compteurs sont stockés dans la **NVS (Preferences)** pour persistance après redémarrage.  
- Une **correction temporelle progressive** est appliquée afin de compenser la dérive de l’horloge interne.  
- Fichiers disponibles :  
  - `logs.txt` et `logs.csv` → journaux journaliers  
  - `full_logs.txt` et `full_logs.csv` → historique complet  

---

## 9. Dépannage
- **Si les journaux journaliers n’apparaissent pas :**  
  Vérifier que l’heure de reset a bien été définie via l’interface web.  
- **Si l’ESP32 ne démarre pas le Wi-Fi :**  
  Redémarrer la carte. Le Wi-Fi reste actif pendant une durée définie (par défaut : 60 minutes).  
- **Si les journaux dépassent la capacité mémoire :**  
  Supprimer les fichiers via le bouton *Reset Logs*.  

---
