#include <Arduino.h>
#include <LiquidCrystal.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <SimpleDHT.h>
#include <dht.h>

volatile unsigned char *myADCSRA = (unsigned char *)0x7A;
volatile unsigned char *myADCSRB = (unsigned char *)0x7B;
volatile unsigned char *myADMUX = (unsigned char *)0x7C;
volatile unsigned int *myADCDATA = (unsigned int *)0x78;

// Define Port B Register Pointers
volatile unsigned char *portB = (unsigned char *)0x25;
volatile unsigned char *ddrB = (unsigned char *)0x24;
volatile unsigned char *pinB = (unsigned char *)0x23;

volatile unsigned char *portF = (unsigned char *)0x31;
volatile unsigned char *ddrF = (unsigned char *)0x30;
volatile unsigned char *pinF = (unsigned char *)0x2F;

volatile unsigned char *portK = (unsigned char *)0x108;
volatile unsigned char *ddrK = (unsigned char *)0x107;
volatile unsigned char *pinK = (unsigned char *)0x106;

volatile unsigned char *portH = (unsigned char *)0x102;
volatile unsigned char *ddrH = (unsigned char *)0x101;
volatile unsigned char *pinH = (unsigned char *)0x100;

// Prototypes
void adcInit();
unsigned int adcRead(unsigned char adcChannelNum);
void displayWaterLevel(unsigned int waterLevel);

unsigned int waterLevel = 0;
unsigned int tempValue = 0;

LiquidCrystal lcd(12, 11, 5, 4, 3, 2);
dht DHT;
#define dht_apin A0

void setup()
{
  // set up the ADC
  adcInit();
  Serial.begin(9600);

  // set PB7 to output lights
  *ddrB = 0xF0;

  lcd.begin(16, 2);

}

void loop()
{
  // Get the reading from the ADC
  unsigned int waterLevel = adcRead(0);

  // Delay
  //delay(100);

  // reads in the water level and displays it
  displayWaterLevel(waterLevel);


  int chk = DHT.read11(dht_apin); 
  lcd.setCursor(0,0); 
  lcd.print("Temp: ");
  lcd.print(DHT.temperature);
  lcd.print((char)223);
  lcd.print("C");
  lcd.setCursor(0,1);
  lcd.print("Humidity: ");
  lcd.print(DHT.humidity);
  lcd.print("%");
  delay(2000);

}

void adcInit()
{
  // Set up the A register
  // Set bit 7 to 1 to enable the ADC
  *myADCSRA |= 0x80;
  // clear bit 5 to 0 to disable the ADC trigger mode
  *myADCSRA &= 0xDF;
  // clear bit 3 to 0 to disable the ADC interrupt
  *myADCSRA &= 0xF7;
  // clear bits 2-0 to 0 to set prescalar selection to slow reading (Division Factor of 2)
  *myADCSRA &= 0xF8;

  // Setup the B register
  // clear bit 3 to 0 to reset the channel and gain bits
  *myADCSRB &= 0xF7;
  // clear bits 2-0 to 0 to set free running mode
  *myADCSRB &= 0xF8;

  // set up the MUX register
  // clear bit 7 to 0 for AREF, internal Vref turned off
  *myADMUX &= 0x7F;
  // set bit 6 to 1 for AVCC analog reference, external capacitor at AREF pin
  *myADMUX |= 0x40;
  // clear bit 5 to 0 for right adjust result
  *myADMUX &= 0xDF;
  // clear bits 4-0 to 0 to reset the channel and gain bits
  *myADMUX &= 0xE0;
}

unsigned int adcRead(unsigned char adcChannelNum)
{
  // clear the channel selection bits (MUX 4:0)
  *myADMUX &= 0xE0;

  // clear the channel selection bits (MUX 5)
  *myADCSRB &= 0xF7;

  // set the channel number
  if (adcChannelNum > 7)
  {
    // set the channel selection bits, but remove the most significant bit (bit 3)
    adcChannelNum -= 8;

    // Set MUX 5
    *myADCSRB |= 0x08;
  }

  // Set the channel selection bits
  *myADMUX += adcChannelNum;

  // Set bit 6 of ADCSRA to 1 to start a conversion
  *myADCSRA |= 0x40;

  // Wait for the conversion to complete
  while ((*myADCSRA & 0x40) != 0)
    ;

  // return the result in the ADC data register
  return *myADCDATA;
}

void displayWaterLevel(unsigned int waterLevel)
{
  // disabled state = yellow LED
  if (waterLevel <= 1)
  {
    Serial.println("Water Level: EMPTY");
    Serial.println(waterLevel);
    *portB = 0x80;
  }
  // Error State = Red LED
  else if (waterLevel > 2 && waterLevel <= 655)
  {
    Serial.println("Water Level: LOW");
    Serial.println(waterLevel);
    *portB = 0x40;
  }
  // Idle State = Green LED
  else
  {
    Serial.println("Water Level: Okay");
    Serial.println(waterLevel);
    *portB = 0x20;
  }

  // Delay
  delay(1000);
}