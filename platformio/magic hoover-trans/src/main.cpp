/*
███████╗███╗   ███╗██╗  ██╗ ██████╗ ███████╗██╗   ██╗
██╔════╝████╗ ████║██║  ██║██╔═══██╗██╔════╝╚██╗ ██╔╝
███████╗██╔████╔██║███████║██║   ██║███████╗ ╚████╔╝
╚════██║██║╚██╔╝██║██╔══██║██║   ██║╚════██║  ╚██╔╝
███████║██║ ╚═╝ ██║██║  ██║╚██████╔╝███████║   ██║
╚══════╝╚═╝     ╚═╝╚═╝  ╚═╝ ╚═════╝ ╚══════╝   ╚═╝
Projet      : MAGIC HOOVER - TRANSMETTEUR
crée le     : 01/08/2024
Modifié le  : 10/03/2025
par         : David AUVRÉ alias SMHOSY

*** remerciement ***
http://electroniqueamateur.blogspot.com/2018/02/communication-rf-433-mhz-entre-attiny85.html
*/
#include <Arduino.h>

// Librairie manchester pour gérer le module rf 433mhz
#include <Manchester.h>

#include <WinsonLib.h>
/*définition des pins attiny85

   5-|1  8|-VCC
A3/3-|2  7|-A1/2
A2/4-|3  6|-1
 GND-|4  5|-0

*/
#define TX_PIN 1   // broche reliée à l'émetteur 433 MHz
#define WCS_PIN A1 // broche reliée au WCS1800
#define ADR_PIN A2 // ADRESSE 0=0; 1=341; 2=521; 3=614; 4=677; 5=727; 6=765; 7=794

#define datalength 4       // nombre de bytes que contiendra le message (minimum 2)
uint8_t data[datalength];  // variable de données envoyée
unsigned int data_wcs = 0; // variable pour les donnée lu du WCS1800
bool power = false;
uint8_t adr_emeteur = 0; // variable pour les donnée lu du WCS1800

// declaration de la libraie pour le WCS1800
WCS WCS1 = WCS(WCS_PIN, _WCS1800);

void define_adr()
{
  unsigned int analog_adr = analogRead(ADR_PIN);
  if (analog_adr >= 0)
    adr_emeteur = 0;
  if (analog_adr > 300)
    adr_emeteur = 1;
  if (analog_adr > 500)
    adr_emeteur = 2;
  if (analog_adr > 600)
    adr_emeteur = 3;
  if (analog_adr > 650)
    adr_emeteur = 4;
  if (analog_adr > 700)
    adr_emeteur = 5;
  if (analog_adr > 750)
    adr_emeteur = 6;
  if (analog_adr > 780)
    adr_emeteur = 7;
}

void setup()
{

  pinMode(A2, INPUT);
  // configuration du module radio
  man.setupTransmit(TX_PIN, MAN_300);
  // activation du WCS1800
  WCS1.Reset();
  // lecture de l'adresse
  define_adr();
  delay(500);

  data[0] = datalength; // nombre de bytes du message envoyé

  data[1] = adr_emeteur; // pour que le récepteur sache de qui provient le message

  data[2] = 0; // la mesure du potentiomètre (10 bits) est répartie
  data[3] = 0; // sur deux bytes
  man.transmitArray(datalength, data);
  delay(250);
  man.transmitArray(datalength, data);
  delay(250);
  man.transmitArray(datalength, data);
  delay(250);
}

void loop()
{
  data_wcs = floor(1000 * WCS1.A_AC() + 0.5);
  if (data_wcs >= 500 && power == false)
  {
    power = true;
    data[2] = highByte(data_wcs); // la mesure du potentiomètre (10 bits) est répartie
    data[3] = lowByte(data_wcs);  // sur deux bytes
    man.transmitArray(datalength, data);
    delay(250);
    man.transmitArray(datalength, data);
    delay(250);
    man.transmitArray(datalength, data);
    delay(250);
  }
  if (data_wcs <= 499 && power == true)
  {
    power = false;
    data[2] = highByte(data_wcs); // la mesure du potentiomètre (10 bits) est répartie
    data[3] = lowByte(data_wcs);  // sur deux bytes
    man.transmitArray(datalength, data);
    delay(250);
    man.transmitArray(datalength, data);
    delay(250);
    man.transmitArray(datalength, data);
    delay(250);
  }
  delay(500); // on envoi toute les 1 secondes
}