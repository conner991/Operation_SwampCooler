//#include <Arduino.h>
#include <LiquidCrystal.h>
//#include <Adafruit_Sensor.h>
#include <Wire.h>
//#include <SimpleDHT.h>
#include <dht.h>
#include <DS3231.h>

volatile unsigned char *myADCSRA = (unsigned char *)0x7A;
volatile unsigned char *myADCSRB = (unsigned char *)0x7B;
volatile unsigned char *myADMUX = (unsigned char *)0x7C;
volatile unsigned int *myADCDATA = (unsigned int *)0x78;

// Define Port Register Pointers
volatile unsigned char *portB = (unsigned char *)0x25;
volatile unsigned char *ddrB = (unsigned char *)0x24;
volatile unsigned char *pinB = (unsigned char *)0x23;

volatile unsigned char *portF = (unsigned char *)0x31;
volatile unsigned char *ddrF = (unsigned char *)0x30;
volatile unsigned char *pinF = (unsigned char *)0x2F;

volatile unsigned char *portE = (unsigned char *)0x2E;
volatile unsigned char *ddrE = (unsigned char *)0x2D;
volatile unsigned char *pinE = (unsigned char *)0x2C;

volatile unsigned char *portG = (unsigned char *)0x34;
volatile unsigned char *ddrG = (unsigned char *)0x33;
volatile unsigned char *pinG = (unsigned char *)0x32;

// Prototypes
void adcInit();
unsigned int adcRead(unsigned char adcChannelNum);
void displayWaterLevel(unsigned int waterLevel);
static bool measureTempHumid();
void tempFan();
void TempLCD();
void RTC(); //RTC

// Global Variables
unsigned int waterLevel = 0;
unsigned int tempValue = 0;
bool fan_on = false;

#define ENABLE 5
#define DIRA 3
#define DIRB 4

LiquidCrystal lcd(22, 23, 24, 25, 26, 27); //Activates lcd
dht DHT;
#define dht_apin 8

DS3231 clock;
RTCDateTime dt;

void setup()
{

  //  // Set both pins 5 and 3 to output (for fan)
  //  *ddrE |= 0x28;
  //  // Set pin 4 to output (for fan)
  //  *ddrG |= 0x20;
  //
  //  // one way fan direction and enable off
  //  *portE |= 0x08;
  //
  //  // Set pin 4 low
  //  *portG &= 0xDF;

  pinMode(ENABLE, OUTPUT);
  pinMode(DIRA, OUTPUT);
  pinMode(DIRB, OUTPUT);

  digitalWrite(DIRA, HIGH); //one way
  digitalWrite(DIRB, LOW);
  digitalWrite(ENABLE, LOW); // enable off

  adcInit();// set up the ADC
  Serial.begin(9600);

  lcd.begin(16, 2);

  // set PB7 to output lights
  *ddrB = 0xF0;

  lcd.begin(16, 2);

  //pushbotton input
  pinMode(38, INPUT);

  //RTC Clock
  clock.begin();
  clock.setDateTime(__DATE__, __TIME__);
}

void loop()
{
  static int buttonState = digitalRead(38);
  buttonState += digitalRead(38);
  delay(1000);
  if (buttonState % 2 == 1) {
    Serial.println("\nSystem is enabled");
    unsigned int waterLevel = adcRead(0);
    displayWaterLevel(waterLevel);
    if (DHT.temperature < 24 && waterLevel > 100) {
      Serial.println("Idle State");
      *portB = 0x20;
      //unsigned int waterLevel = adcRead(0); // Get the reading from the ADC
      displayWaterLevel(waterLevel); // reads in the water level and displays it//
      digitalWrite(ENABLE, LOW); //Fan off
      TempLCD();
    }
    else if (waterLevel <= 100) {
      Serial.println("\nError State");
      *portB = 0x40; //Turns on Red LED
      Serial.println("Water Level: LOW");
      Serial.println(waterLevel);
      *portB = 0x40; //Turns on Red LED
      digitalWrite(ENABLE, LOW); //Fan off
      lcd.setCursor(0, 0);
      lcd.print("Water level: ");
      lcd.setCursor(0, 1);
      lcd.print("LOW             ");
    }
    else if (DHT.temperature > 0 && waterLevel > 100) {
      Serial.println("\nRunning State");
      *portB = 0x10; //Turns on Blue LED
      tempFan(); //Turns on fan
      displayWaterLevel(waterLevel);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Temp: ");
      lcd.print(DHT.temperature);
      lcd.print((char)223);
      lcd.print("C");
      lcd.setCursor(0, 1);
      lcd.print("Humidity: ");
      lcd.print(DHT.humidity);
      lcd.print("%");
      delay(1000);
    }

  }
  else {
    Serial.println("\nSystem is disabled");
    *portB = 0x80; //Lights up yellow LED
    lcd.clear(); //Clears lcd screen
    digitalWrite(ENABLE, LOW); //Turns off fan
  }
  delay(1000);
}

void tempFan()
{
  if (DHT.temperature > 23.00)
  {
    digitalWrite(ENABLE, HIGH);
    // set pin 5 high, turn on fan
    //*portE |= 0x08;
    RTC();
    if (!fan_on)
    {
      Serial.println("High temperature - turn on fan");
      fan_on = true;
    }
  }
  else
  {
    digitalWrite(ENABLE, LOW);
    RTC();
    // set pin 5 low, turn off fan
    //*portE &= 0xF7;
    if (fan_on)
    {
      Serial.println("Low temperature - turn off fan");
      fan_on = false;
    }
  }
}

void adcInit()
{
  // Set up the A register
  *myADCSRA |= 0x80; // Set bit 7 to 1 to enable the ADC
  *myADCSRA &= 0xDF; // clear bit 5 to 0 to disable the ADC trigger mode
  *myADCSRA &= 0xF7; // clear bit 3 to 0 to disable the ADC interrupt
  *myADCSRA &= 0xF8; // clear bits 2-0 to 0 to set prescalar selection to slow reading (Division Factor of 2)

  // Setup the B register
  *myADCSRB &= 0xF7; // clear bit 3 to 0 to reset the channel and gain bits
  *myADCSRB &= 0xF8; // clear bits 2-0 to 0 to set free running mode

  // set up the MUX register
  *myADMUX &= 0x7F; // clear bit 7 to 0 for AREF, internal Vref turned off
  *myADMUX |= 0x40; // set bit 6 to 1 for AVCC analog reference, external capacitor at AREF pin
  *myADMUX &= 0xDF; // clear bit 5 to 0 for right adjust result
  *myADMUX &= 0xE0; // clear bits 4-0 to 0 to reset the channel and gain bits
}

unsigned int adcRead(unsigned char adcChannelNum)
{
  *myADMUX &= 0xE0;      // clear the channel selection bits (MUX 4:0)
  *myADCSRB &= 0xF7;     // clear the channel selection bits (MUX 5)
  if (adcChannelNum > 7) // set the channel number
  {
    adcChannelNum -= 8; // set the channel selection bits, but remove the most significant bit (bit 3)
    *myADCSRB |= 0x08;  // Set MUX 5
  }

  *myADMUX += adcChannelNum;      // Set the channel selection bits
  *myADCSRA |= 0x40;              // Set bit 6 of ADCSRA to 1 to start a conversion
  while ((*myADCSRA & 0x40) != 0) // Wait for the conversion to complete
    ;

  return *myADCDATA; // return the result in the ADC data register
}

void displayWaterLevel(unsigned int waterLevel)
{
  // Disabled State = Red LED
  if (waterLevel >= 0 && waterLevel <= 240)
  {
    Serial.println("Water Level: LOW");
    Serial.println(waterLevel);
    *portB = 0x40;
    digitalWrite(ENABLE, LOW);
    lcd.setCursor(0, 0);
    lcd.print("Water level: ");
    lcd.setCursor(0, 1);
    lcd.print("LOW             ");
  }
  // Idle State = Green LED
  else
  {
    Serial.println("Water Level: Okay");
    Serial.println(waterLevel);
    *portB = 0x20;
    int chk = DHT.read11(dht_apin);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Temp: ");
    lcd.print(DHT.temperature);
    lcd.print((char)223);
    lcd.print("C");
    lcd.setCursor(0, 1);
    lcd.print("Humidity: ");
    lcd.print(DHT.humidity);
    lcd.print("%");
    delay(1000);
  }

  // Delay
  delay(1000);
}

void TempLCD()
{
  int chk = DHT.read11(dht_apin);
  lcd.setCursor(0, 0);
  lcd.print("Temp: ");
  lcd.print(DHT.temperature);
  lcd.print((char)223);
  lcd.print("C");
  lcd.setCursor(0, 1);
  lcd.print("Humidity: ");
  lcd.print(DHT.humidity);
  lcd.print("%");
  delay(1000);
}

void RTC()
{
  dt = clock.getDateTime();

  // For leading zero look to DS3231_dateformat example

  Serial.print("Current Time: ");
  Serial.print(dt.year);
  Serial.print("-");
  Serial.print(dt.month);
  Serial.print("-");
  Serial.print(dt.day);
  Serial.print(" ");
  Serial.print(dt.hour);
  Serial.print(":");
  Serial.print(dt.minute);
  Serial.print(":");
  Serial.print(dt.second);
  Serial.println("");

  delay(1000);
}
