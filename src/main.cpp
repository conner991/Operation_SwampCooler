#include <Arduino.h>
#include <LiquidCrystal.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <SimpleDHT.h>

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

// Global Variables
unsigned int waterLevel = 0;
unsigned int tempValue = 0;
LiquidCrystal lcd(3, 4, 5, 6, 7, 8);
const int pinDHT11 = 2;
SimpleDHT11 dht11; //instantiates the SimpleDHT11 Object class to variable dht11
float temperatureC = 0.0;
float temperatureF = 0.0;
float humidity = 0.0;

void setup()
{
  // set up the ADC
  adcInit();
  Serial.begin(9600);
  lcd.begin(16, 2);

  // set PB7 to output lights
  *ddrB = 0xF0;
}

void loop()
{

  //Local data array
  byte data[40] = {0};
  if (dht11.read2(pinDHT11, &temperatureC, &humidity, data))
  {
    Serial.print("Read DHT11 failed");
    return;
  }

  Serial.print("Sample RAW Bits: ");
  for (int i = 0; i < 40; i++)
  {
    Serial.print((int)data[i]);
    if (i > 0 && ((i + 1) % 4) == 0)
    {
      Serial.print(' ');
    }
  }

  Serial.println("");
  //°F = °C x 9/5 + 32.
  temperatureF = temperatureC * 9.0 / 5.0 + 32.0;
  Serial.print("Sample OK: ");
  Serial.print(temperatureC);
  Serial.print("°C, ");
  Serial.print(temperatureF);
  Serial.print("°F, ");
  Serial.print(humidity);
  Serial.println("%");

  // DHT11 sampling rate is 1HZ.
  delay(1000);

  //   // Get the reading from the ADC
  //   unsigned int waterLevel = adcRead(0);
  //
  //   // Delay
  //   //delay(100);
  //
  //   // reads in the water level and displays it
  //   displayWaterLevel(waterLevel);
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