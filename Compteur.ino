#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP8266WiFi.h>
#include <NTPClient.h> //Il ne s'agit pas du NTPClient de base mais du Taranais NTP Client https://github.com/taranais/NTPClient
#include <WiFiUdp.h>
#include "configuration.h"
 
#define SCREEN_WIDTH 128    // OLED display width, in pixels
#define SCREEN_HEIGHT 64    // OLED display height, in pixels
#define OLED_RESET -1       // Reset pin # (or -1 if sharing Arduino reset pin)
#define SENSOR  12
#define REAL_LED_PIN 16
 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

//Debug stuff
bool  debug = false;
bool debug_conditions = false;
bool debug_FTP = false;

//Récupération des informations de configuration (fichier configuration.h)
const char *ssid = WIFI_SSID;  
const char *pass = WIFI_PASS;
const char* host = FTP_HOST;
const int port = FTP_PORT;
const char* userName = FTP_USERNAME;
const char* password = FTP_PASSWORD;
char fileName[] = FTP_FILENAME;
char dirName[] = FTP_DIRNAME; 


//Delay stuff
unsigned long interval_affichage = 1000L; // Temps entre chaque raffraichissement du calcul et de l'affichage en ms MODIFIER CETTE VALEUR CHANGERA LE CALCUL DU DEBIT
unsigned long interval_upload = 6000L; // Temps entre chaque upload sur le FTP (en cas de débit > 0) en ms
unsigned long interval_sansdebit = 9000L; // Temps entre chaque upload sur le FTP (en cas de débit nul) en ms
unsigned long interval_ntp = 3600000L; // Temps entre deux synchronosation avec le serveur NTP pour l'horodatage en ms (entre deux synchronisation il utilise millis())
unsigned long interval_connexion = 600000L; // Temps avant d'essayer de se reconnecter en ms

//Captor stuff
// Basé sur capteur débit : F = 6*Q-8
bool analog_captor = false; // Mettre false si pas de capteur en analogique.
const int analogInPin = A0; // ESP8266 Analog Pin ADC0 = A0
bool calibration = true; // True = donne le nombre de pulsation
 
//NTP stuff
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 7200, interval_ntp);

 
uint64_t previousMillis = 0;
uint64_t previous_connexion = 0;
uint64_t previous_upload = 0;
boolean ledState = LOW;
volatile byte pulseCount;
byte pulse1Sec = 0;
float flowRate;
unsigned long flowMilliLitres;
uint64_t totalMilliLitres;
uint64_t pulseCount_calibration = 0;
float flowLitres;
float totalLitres;
uint64_t Millilitres_since_last_upload =0;
bool FTP=false;
void IRAM_ATTR pulseCounter()
{
  pulseCount++;
}



//############################################
//DO NOT CHANGE THESE
char outBuf[128];
int outCount;
WiFiClient client; //client pour se connecter
WiFiClient dclient; //client pour transférer les données


void efail() //fermeture connexion FTP (NE PAS MODIFIER)
{
  byte thisByte = 0;

  client.println(F("QUIT"));

  while(!client.available()) delay(1);

  while(client.available())
  { 
    thisByte = client.read();   
    Serial.write(thisByte);
  }

  client.stop();
  Serial.println(F("Command disconnected - efail"));
}


byte eRcv() //test connexion FTP (NE PAS MODIFIER)
{
  byte respCode;
  byte thisByte;

  while(!client.available()) delay(1);

  respCode = client.peek();

  outCount = 0;

  while(client.available())
  { 
    thisByte = client.read();   
    Serial.write(thisByte);

    if(outCount < 127)
    {
      outBuf[outCount] = thisByte;
      outCount++;     
      outBuf[outCount] = 0;
      if (debug) Serial.print(F("modification outBuf"));
      if (debug) Serial.println(outCount);
    }
  }

  if(respCode >= '4')
  {
    efail();
    return 0; 
  }

  return 1;
}



void affichertext(const __FlashStringHelper * text, int text_size=1) //fonction permettant d'afficher du texte sur l'écran de l'esp (pas tout le temps utilisé)
{
  display.setTextSize(text_size);             // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE);        // Draw white text
  display.setCursor(0,0);             // Start at top-left corner
  display.println(text);
  display.display();

  return;
}


bool uploadFTP() //Fonction se connectant et uploadant sur le FTP
{
  
  if (debug) Serial.println("entree upload");
  timeClient.update(); //update de l'horodatage
  display.clearDisplay();
  affichertext(F("Upload sur le FTP\n"),1);
  display.display();
  
  if (client.connect(host,port))
  {
    Serial.println(F("FTP connected"));
  }
  else
  {
    Serial.println(F("FTP connection failed"));
    display.print(F("\nErreur FTP"));
    display.display();
    delay(2000);
    return false;
  }

  
  if(!eRcv()) return false;

  client.print("USER ");
  client.println(userName);
 
  if(!eRcv()) return false;
 
  client.print("PASS ");
  client.println(password);
 
  if(!eRcv()) return false;
 
  client.println("SYST");

  if(!eRcv()) return false;

  client.println("Type I");
  
  if(!eRcv()) return false;

  client.println("PASV");

  if(!eRcv()) return false;
  
  
  
  char *tStr = strtok(outBuf,"(,");
  int array_pasv[6];

  if (debug) Serial.println("l260");
  for ( int i = 0; i < 6; i++) {
    if (debug) Serial.println(i);
    tStr = strtok(NULL,"(,");
    if(tStr == NULL)
    {
      Serial.println(F("Bad PASV Answer"));
      return false;
    }
    if (debug) Serial.println(i+100);
    array_pasv[i] = atoi(tStr);
    if (debug) Serial.println(i+200);
  }

  if (debug) Serial.println("sortie initialisation FTP");

  if (debug) Serial.println("timeclient upload");
  
  if (debug) Serial.println("l148");
  
  if (debug) Serial.println("debug 1");
  unsigned int hiPort,loPort;
  hiPort=array_pasv[4]<<8;
  loPort=array_pasv[5]&255;
  Serial.print(F("Data port: "));
  hiPort = hiPort|loPort;
  Serial.println(hiPort);
  if (debug) Serial.println("debug 2");
  if(dclient.connect(host, hiPort)){
    display.clearDisplay();
    affichertext(F("Client UPLOAD\n écriture"),1);
    display.display();
  }
  else{
    affichertext(F("\nErreur client\n UPLOAD"),1);
    if (debug) Serial.println("Erreur client\n UPLOAD");
    display.display();
    delay(1000);
    client.stop();
    return false;
  }
  if (debug) Serial.println("debug 3");
  
  client.print("CWD ");
  client.println(dirName);
  if(!eRcv())
  {
    dclient.stop();
    return false;
  }
  client.print("SIZE ");
  client.println(fileName);
  byte byte_taille_fichier;
  char taille_fichier[64];
  int l_taille_fichier = 0;
 
  int attemptNumber = 0;
  while(!client.available())
  {
    if (attemptNumber++ > 50000) return false;
    delay(1);
  }
 
  while(client.available())
  { 
    byte_taille_fichier = client.read();   
    Serial.write(byte_taille_fichier);
    l_taille_fichier+=snprintf(taille_fichier + l_taille_fichier,64 - l_taille_fichier,"%c",byte_taille_fichier);
  }
  l_taille_fichier += snprintf(taille_fichier + l_taille_fichier, 64 - l_taille_fichier, "\n");
  char * retour_FTP = strtok(taille_fichier, " ");
  if (atoi(retour_FTP) != 550)
  {
      retour_FTP = strtok(NULL, " ");
      Serial.print(F("Ecriture a l'emplacement : "));
      Serial.println(retour_FTP);
      client.print("REST ");
      client.println(retour_FTP);
  }
  else Serial.println(F("Pas de fichier, creation de celui-ci"));
  client.print("STOR ");
  client.println(fileName);
  if(!eRcv())
  {
    dclient.stop();
    return false;
  }
  affichertext(F("\n En cours"));
  display.display();
  Serial.println(F("Writing"));
  if (debug) Serial.println("debug 4");
  char horodatage[64];
  snprintf(horodatage, 64, "%s", timeClient.getFormattedDate().c_str());
  char * pch = strtok(horodatage,"-T:Z");
  char date[64];
  int position = 0;

  while (pch!=NULL && position < 64)
  {
    position += snprintf(date + position, 64 - position, pch);
    pch = strtok(NULL, "-T:Z");
  }
  
  char data[64];
  int length = 0;
  if (analog_captor) 
  {
    if (calibration) length = snprintf(data, 64, "%s %f %f %llu %d %llu\n", date, totalLitres, flowLitres, extendedMillis(), analogRead(analogInPin), pulseCount_calibration);
    else length = snprintf(data, 64, "%s %f %f %llu %d\n", date, totalLitres, flowLitres, extendedMillis(), analogRead(analogInPin));
  }
  else
  {
    if (calibration) length = snprintf(data, 64, "%s %f %f %llu %llu\n", date, totalLitres, flowLitres, extendedMillis(), pulseCount_calibration);
    else length = snprintf(data, 64, "%s %f %f %llu\n", date, totalLitres, flowLitres, extendedMillis());
  }
  Serial.print(F("Ecriture d'une ligne sur le fichier FTP, longueur : "));
  Serial.println(length);
  Serial.println(data);
  dclient.write(data, length);
  dclient.stop();
  Serial.println(F("Data disconnected"));
  display.clearDisplay();
  affichertext(F("Fin upload"));
  display.display();
  if (debug) Serial.println("debug 5");
  client.println();
  if(!eRcv()) return false;
  if (debug_FTP)
  {
    client.print("SIZE ");
    client.println(fileName);
    Serial.println(F("Nouvelle taille du fichier :"));
    if(!eRcv()) return false;
  }
  client.println("QUIT");

  if(!eRcv()) return false;
  client.stop();
  Serial.println(F("Command disconnected"));
  return true;
}


void connexionFTP(void) //Fonction de connexion au FTP sans upload (mais création du dossier) -> utilisée qu'au démarrage de l'ESP
{
  display.clearDisplay();
  affichertext(F("Connexion au serveur FTP\n"),1);
  display.print(host);
  display.print(":");
  display.print(port);
  display.display();
  delay(1000);
  if (client.connect(host,port))
  {
    Serial.println(F("FTP connected"));
    display.print(F("\nFTP : OK"));
    display.display();
    delay(2000);
  }
  else
  {
    Serial.println(F("FTP connection failed"));
    display.print(F("\nErreur FTP"));
    display.display();
    delay(2000);
    return;
  }

  
  if(!eRcv()) return ;

  client.print("USER ");
  client.println(userName);
 
  if(!eRcv()) return ;
 
  client.print("PASS ");
  client.println(password);
 
  if(!eRcv()) return ;
 
  client.println("SYST");

  if(!eRcv()) return ;

  client.println("Type I");

  if(!eRcv()) return ;

  client.println("PASV");

  if(!eRcv()) return ;
  
  
  
  char *tStr = strtok(outBuf,"(,");

  if (debug) Serial.println("l260");
  for ( int i = 0; i < 6; i++) {
    if (debug) Serial.println(i);
    tStr = strtok(NULL,"(,");
    if(tStr == NULL)
    {
      Serial.println(F("Bad PASV Answer"));
      return;
    }
    if (debug) Serial.println(i+100);
    if (debug) Serial.println(i+200);
  }
  client.println("MLSD");
  client.print("MKD ");
  client.println(dirName);
  if (debug) Serial.println("sortie initialisation FTP");
  FTP = true;
  client.stop();
  return;
}

uint64_t extendedMillis() //gestion du fait que millis repasse à 0 au bout de 50j. Donne le nombre de ms depuis le démarrage de l'ESP
{
    static unsigned long previous_millis_50 = 0;
    static uint64_t offset = 0;
    unsigned long current_millis_50 = millis();

    if (current_millis_50 < previous_millis_50)
        offset += ULONG_MAX;
    previous_millis_50 = current_millis_50;

    uint64_t retval = current_millis_50; // Important de d'abord convertir en 64bits avant de faire des opérations dessus
    retval += offset;
    return retval;
}

void lancementWifi(void) //Fonction de connexion au WIFI
{
   // We start by connecting to a WiFi network
  /* Explicitly set the ESP8266 to be a WiFi-client, otherwise, it by default,
     would try to act as both a client and an access-point and could cause
     network-issues with your other WiFi-devices on your WiFi-network. */
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  affichertext(F("Connexion au Wifi"),1);
  display.print(ssid);
  display.display();
  delay(500);
  WiFi.begin(ssid, pass);
  for (int i=0; i<20; i++) 
  {
    if (WiFi.status() == WL_CONNECTED)
      break;
    else
    {
      delay(500);
      display.print(".");
      display.display();
    }
  }
  
  display.clearDisplay();

  if (WiFi.status() == WL_CONNECTED)
  {
    affichertext(F("Wifi : OK\n"),1);
    display.print(WiFi.macAddress());
    display.display();
    delay(2000);
  }
  else
  {
    affichertext(F("Wifi: Echec connexion"),1);
    display.display();
    delay(2000);
  }

  Serial.println(WiFi.macAddress()); // renvoi sur le port série l'adresse MAC de l'ESP
  return;
}


void calcul_debit() //Fonction de calcul du débit 
{
  pulse1Sec = pulseCount; // récupération du nombre de pulsation
  if (calibration) pulseCount_calibration += pulse1Sec;
  float t = (extendedMillis() - previousMillis)/1000.;
  previousMillis = extendedMillis();
  if (pulse1Sec == 0)
  {
    flowRate = 0; //débit nul
    flowMilliLitres = 0;
    flowLitres = 0;
  }
  else
  {
    pulseCount -= pulse1Sec;
    flowRate = (pulse1Sec/t+8.0)/6; // Calcul du débit
    flowMilliLitres = (pulse1Sec+8*t)*1000/360; //Volume en mL depuis dernier comptage
    flowLitres = (pulse1Sec+8.0*t)/360; //Volume en L depuis dernier comptage
  }
  
  totalMilliLitres += flowMilliLitres; //Volume total en mL depuis dernier boot
  totalLitres = totalMilliLitres/1000.; //Volume total en L depuis dernier boot
  Millilitres_since_last_upload += flowMilliLitres; //Permet de voir si ça vaut le coup d'uploader


  if (debug) Serial.println("fin calcul débit");
  return;
}

void affichage_debit(void) //Fonction de mise à jour de l'affichage
{
    
    display.clearDisplay();
    // Print the flow rate for this second in litres / minute
    Serial.print("Flow rate: ");
    Serial.print(float(flowRate));  // Print the integer part of the variable
    Serial.print("L/min");
    Serial.print("\t");       // Print tab space
 
    
    display.setCursor(10,0);  //oled display
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.print(F("Water flow meter\n"));
    if (WiFi.status() == WL_CONNECTED)
      display.print(F("WIFI OK"));
    else display.print(F("WIFI NOK"));
    if(!FTP)
      display.print(F(" FTP NOK"));
    else display.print(F(" FTP OK"));

    display.setCursor(0,20);  //oled display
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.print("R:");
    display.print(float(flowRate));
    display.setCursor(100,28);  //oled display
    display.setTextSize(1);
    display.print("L/M");
 
    // Print the cumulative total of litres flowed since starting
    Serial.print("Output Liquid Quantity: ");
    Serial.print(totalLitres);
    Serial.println("L");
 
    display.setCursor(0,45);  //oled display
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.print("V:");
    display.print(totalLitres);
    display.setCursor(100,53);  //oled display
    display.setTextSize(1);
    display.print("L");
    display.display();
    if (debug) Serial.println(" fin affichage");
  return;
}

void setup()
{
  Serial.begin(115200);
  delay(100);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); //initialize with the I2C addr 0x3C (128x64)
  //initialiasation de l'écran
  display.clearDisplay();
  display.setCursor(10,0);  //oled display
  display.setTextSize(1);
  display.setTextColor(WHITE);

  affichertext(F("Lancement du \n compteur de \n debit"));
  delay(2000);
  display.clearDisplay();

   //début de la séquence de lancement.
  lancementWifi();

  connexionFTP();
  

  pinMode(REAL_LED_PIN, OUTPUT);
  pinMode(SENSOR, INPUT_PULLUP);
 
  pulseCount = 0;
  flowRate = 0.0;
  flowMilliLitres = 0;
  totalMilliLitres = 0;
  previousMillis = 0;
 

  // Démarrage du client NTP - Start NTP client
  timeClient.begin();
  timeClient.update();



  attachInterrupt(digitalPinToInterrupt(SENSOR), pulseCounter, FALLING);


}
 
void loop()
{
  if ( extendedMillis() - previousMillis > interval_affichage) //On ne va mettre à jour l'affichage et le calcul que toutes les secondes
  {
    calcul_debit();
    affichage_debit();
    if (extendedMillis() - previous_upload > interval_upload && Millilitres_since_last_upload > 0 && FTP) // test si il y a eu du débit sinon ça ne sert à rien d'uploader.
    {
      if (debug_conditions) Serial.println("FTP ok - Débit ok - upload");
      
      FTP=uploadFTP();
      if (FTP) 
      {
        previous_upload = extendedMillis();
        Millilitres_since_last_upload = 0;
        pulseCount_calibration = 0;
      }
    }
    if (extendedMillis() - previous_connexion > interval_connexion ) //toutes les 10 minutes on regarde si il y a toujours le WIFI sinon on se reconnecte
    {
      if (debug_conditions) Serial.println("Tentative reconnexion");
      if (WiFi.status() != WL_CONNECTED) lancementWifi();
      if (!FTP)
      {
        if (debug_conditions) Serial.println("reconnexion FTP");
        FTP=uploadFTP();
        if (FTP) 
        {
          previous_upload = extendedMillis();
          Millilitres_since_last_upload = 0;
          pulseCount_calibration = 0;
        }
      }
      previous_connexion = extendedMillis();
    }
    if (extendedMillis() - previous_upload > interval_sansdebit && FTP ) // si on n'a pas eu de débit on va quand même forcer l'upload
    {
      if (debug_conditions) Serial.println("FTP ok - Débit 0 - upload");
      FTP=uploadFTP();
      if (FTP) 
      {
        previous_upload = extendedMillis();
        Millilitres_since_last_upload = 0;
        pulseCount_calibration = 0;
      }
    }
    if (debug) Serial.println("fin d'une boucle");
  }
  
}
