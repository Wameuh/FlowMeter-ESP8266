Capteur de débit d'eau avec upload de l'information sur server FTP par un ESP8266
----------------------------------------------------------------------------------
Mesure le débit et envoi la données en ajoutant une ligne sur un fichier par FTP.

Inspirations
------------
Partie gestion capteur réalisée sur inspiration de :
https://how2electronics.com/iot-water-flow-meter-using-esp8266-water-flow-sensor/#IoT_Water_Flow_Meter_using_ESP8266_Water_Flow_Sensor

Partie FTP réalisée sur inspiration de :
https://www.rudiswiki.de/wiki9/WiFiFTPServer

----------------------------------------------------------------------------------


Librairies
----------
Nécessite les librairies arduino suivantes :
- Adafruit SSD1306 (pour l'écran) v2.4.4
- Adafruit GFX (pour l'écran) v1.10.7
- Adafruit BusIO (pour l'écran) v1.7.3
- Taranais NTPClient (pas celui de base) -> https://github.com/taranais/NTPClient (pour l'horodatage) (3.1.0)
----------------------------------------------------------------------------------


Matériel
--------
Programme prévu pour :
- ESP8266 avec écran 1.3pouces OLED. A la base il est avec Micronpython mais non utilisé ici. (alimentation 5V)
	https://www.makerfabs.com/makepython-esp8266.html
- Capteur de débit d'eau à effet hall.
	https://www.gotronic.fr/art-capteur-de-debit-yf-b2-31350.htm.
	Son alimentation est branchée en parallèle de celle de l'ESP (sur VCCin et GND). La sortie est connectée au GPIO12.
------------------------------------------------------------------------------------


Installation logiciel
----------------
Le programme peut être uploadé en utilisant le logiciel Arduino IDE.
1. Téléchargez et installez et lancez le programme : https://www.arduino.cc/en/software (avec les drivers notamment)

2. Dans Fichier>Préférences ajouter dans "URL de gestionnaire de cartes supplémentaires" :
	https://arduino.esp8266.com/stable/package_esp8266com_index.json
	
3. Dans Outils>Types de cartes>Gestionnaire de cartes :
	- Indiquez ESP8266 dans la barre de recherche
	- Sélectionnez la dernière version et installé le pack "esp8266 by ESP8266 Community" (programme réalisé sur la version 2.7.4)

4. Sélectionnez Outils>Types de cartes>ESP8266 Boards>Generic ESP8266 Module

5. Cliquez sur Outils>Gérer les bibliothèques :
	- Indiquez SSD1306 dans la barre de recherche
	- Installez la dernière version de la librairie Adafruit SSD1306
	- Indiquez GFX dans la barre de recherche
	- Installez la dernière version de la librairie Adafruit GFX Library
	- Indiquez Bus dans la barre de recherche
	- Installez la dernière version de la librairie Adafruit BusIO

6. Téléchargez la librairie Taranais NTPClient sur https://github.com/taranais/NTPClient
	- Cliquez sur Code>Download ZIP
	- Renommez le fichier pour qu'il n'y ait pas de caractères spéciaux dans le nom

7. Revenez sur Arduino IDE, cliquez sur Croquis>Inclure une bibliothèque>Ajouter la bibliothèque .ZIP...
	- choisissez votre fichier .ZIP téléchargé au 6-

8. Cliquez sur Fichier>Exemples>01.Basics>BareMinimum
9. Copiez/collez le code dans la fenêtre qui est apparue
10. Sélectionnez le port dans Outils>Port> (ça peut être COM1 par exemple)
11. Configurez le programme :
	- WIFI stuff
	- FTP stuff
	- File stuff
	- Delay stuff
	- Captor stuff
	- NTP stuff (vous pouvez modifier le serveur NTP et le nombre qui suit pour mettre un décalage entre l'heure UTC et l'heure désirée)

12. Cliquez sur Croquis>Vérifier/Compiler

13. S'il n'y a pas d'erreur cliquez sur Croquis>Téléverser

Et voilà le programme est dans l'ESP8266.
------------------------------------------------------------------------------------


Fonctionnement du programme
---------------------------
Chaque pulsation du capteur va interrompre le programme pour incrémenter le compteur (Volume(L) = Nbr pulsation / (coefficient*60)).

Au lancement, il se connecte en Wifi,essaie la connexion FTP et commence le service NTP.

Toutes les secondes(interval_affichage), il regarde le nombre d'impulsion, met à jour le compteur et l'affichage.

Toutes les minutes(interval_upload), si la dernière connexion FTP a marché et s'il y a eu du débit depuis le dernier upload envoi en FTP l'horodatage le volume total le débit de la dernière seconde (en L/m) et le nombre de milisecondes depuis le dernier démarrage (+valeur de l'entrée analogique si le booléen est vrai dans les paramétrages) en l'ajoutant sur une ligne dans un fichier YYYYMMDDHHMMSS VVVVVV DDDDD SSSSSS.

Exemple : 

20210428110408 3.881304 0.347800 73030

20210428110429 7.377808 0.284900 94063

20210428110453 7.640508 0.007400 118088

20210428110524 12.232213 0.000000 149176

Toutes les 10 minutes(interval_connexion), il vérifie qu'il y a toujours le WIFI sinon essaie de se reconnecter. S'il il y a eu un echec au niveau de la connexion FTP il va essayer de se reconnecter et uploader la donnée : horodatage le volume total le débit de la dernière seconde (en L/m) et le nombre de milisecondes depuis le dernier démarrage

Toutes les heures, s'il n'y a pas eu d'upload et que le FTP marche, il va forcer un upload (cas où il n'y a pas de débit).

------------------------------------------------------------------------------------


Précisions
----------
La fonction extendedMillis() permet d'éviter un rollover de millis() au bout de 50j.

Au rédemarrage de l'ESP le volume total est perdu, ceci peut être vu par le nombre de milisecondes dans le fichier FTP.

La gestion des erreur FTP est très limitée (car ma compréhension du code utilisé reste limitée)



------------------------------------------------------------------------------------
Réalisé par Wameuh avec l'aide de Jollie_fin


