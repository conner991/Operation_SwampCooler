#include <Arduino.h>
#include <LiquidCrystal.h>
#include <Wire.h>
#include <dht.h>
#include <DS3231.h>

// ADC pointers
volatile unsigned char *myADCSRA = (unsigned char *)0x7A;
volatile unsigned char *myADCSRB = (unsigned char *)0x7B;
volatile unsigned char *myADMUX = (unsigned char *)0x7C;
volatile unsigned int *myADCDATA = (unsigned int *)0x78;

// State LED pointers
volatile unsigned char *portB = (unsigned char *)0x25;
volatile unsigned char *ddrB = (unsigned char *)0x24;
volatile unsigned char *pinB = (unsigned char *)0x23;

// Vent Movement LED pointers
volatile unsigned char *portH = (unsigned char *)0x102;
volatile unsigned char *ddrH = (unsigned char *)0x101;
volatile unsigned char *pinH = (unsigned char *)0x100;

// Power Button pointers
volatile unsigned char *portD = (unsigned char *)0x2B;
volatile unsigned char *ddrD = (unsigned char *)0x2A;
volatile unsigned char *pinD = (unsigned char *)0x29;

// Fan pointers
volatile unsigned char *portE = (unsigned char *)0x2E;
volatile unsigned char *ddrE = (unsigned char *)0x2D;
volatile unsigned char *pinE = (unsigned char *)0x2C;

// Fan pointers
volatile unsigned char *portG = (unsigned char *)0x34;
volatile unsigned char *ddrG = (unsigned char *)0x33;
volatile unsigned char *pinG = (unsigned char *)0x32;

// Prototypes
void adcInit();
unsigned int adcRead(unsigned char adcChannelNum);
void disabledState();
void displayWaterLevel(unsigned int waterLevel);
void enabledState();
void errorState();
void idleState();
static bool measureTempHumid();
void realTimeClockOff(); //realTimeClock
void realTimeClockOn();  //realTimeClock
void runningState();
void tempFan();
void tempLCD();
void ventLeftRight();

// Global Variables
unsigned int waterLevel = 0;
unsigned int tempValue = 0;
bool fan_on = false;
bool buttonEnabled = false;

#define ENABLE 5
#define DIRA 3
#define DIRB 4

LiquidCrystal lcd(22, 23, 24, 25, 26, 27); //Activates lcd
dht DHT;
#define dht_apin 8

DS3231 clock;
RTCDateTime dt;

//////////////////////////
// setup Function
//////////////////////////
void setup()
{

  //set PD7 for input
  *ddrD &= 0x7F;
  //enable pull up resistor on PD7
  *portD |= 0x80;

  // set PB7 to output LED lights
  *ddrB = 0xF0;
  *ddrH = 0x18;

  // Fan Set Up
  // Set both pins 5 and 3 to output (for fan)
  *ddrE = 0x28;
  // Set pin 4 to output (for fan)
  *ddrG = 0x20;
  // one way fan direction and enable off
  *portE = 0x08;
  // Set pin 4 low
  *portG = 0xDF;

  // initialize the ADC
  adcInit();
  Serial.begin(9600);

  // Initialize the LCD Display
  lcd.begin(16, 2);

  //realTimeClock Clock
  clock.begin();
  clock.setDateTime(__DATE__, __TIME__);
}

//////////////////////////
// loop Function
//////////////////////////
void loop()
{
  // If PD7 reads in a 0 (button pressed)
  if (!(*pinD & 0x80))
  {
    // for loop for debouncing
    for (volatile unsigned int i = 0; i < 1000; i++)
      ;

    if (!(*pinD & 0x80))
    {

      if (!buttonEnabled)
      {
        buttonEnabled = true;
      }
      else
      {
        buttonEnabled = false;
      }

      // Wait for input signal to finish
      while (!(*pinD & 0x80))
        ;
    }
  }

  if (buttonEnabled)
  {
    enabledState();
  }
  else
  {
    disabledState();
  }

  delay(1000);
}

//////////////////////////
// adcInit Function
//////////////////////////
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

//////////////////////////
// adcRead Function
//////////////////////////
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

//////////////////////////
// enabledState Function
//////////////////////////
void enabledState()
{
  Serial.println("\nSystem is enabled");
  unsigned int waterLevel = adcRead(1);

  if (DHT.temperature <= 20.00 && waterLevel > 100)
  {
    idleState();
  }
  else if (DHT.temperature >= 21.00 && waterLevel > 100)
  {
    runningState();
  }
  else if (waterLevel <= 100)
  {
    errorState();
  }

  else
  {
    //Does nothing
  }
}

//////////////////////////
// disabledState Function
//////////////////////////
void disabledState()
{
  Serial.println("\nSystem is disabled");
  *portB = 0x80; //Lights up yellow LED
  lcd.clear();   //Clears lcd screen
  // set pin 5 low, turn off fan
  *portE = 0xF7;
}

//////////////////////////
// idleState Function
//////////////////////////
void idleState()
{
  unsigned int waterLevel = adcRead(1);
  Serial.println("Idle State");
  // Turn on Green LED
  *portB = 0x20;
  displayWaterLevel(waterLevel); // reads in the water level and displays it//
  // set pin 5 low, turn off fan
  *portE = 0xF7;
  tempLCD();
  ventLeftRight();
}

//////////////////////////
// runningState Function
//////////////////////////
void runningState()
{
  unsigned int waterLevel = adcRead(1);
  Serial.println("Running State");
  *portB = 0x10; //Turns on Blue LED
  tempFan();     //Turns on fan
  displayWaterLevel(waterLevel);
  ventLeftRight();
  tempLCD();
}

//////////////////////////
// errorState Function
//////////////////////////
void errorState()
{
  unsigned int waterLevel = adcRead(1);
  Serial.println("Error State");
  *portB = 0x40; //Turns on Red LED
  Serial.println("Water Level: LOW");
  *portB = 0x40; //Turns on Red LED
  // set pin 5 low, turn off fan
  *portE = 0xF7;
  if (fan_on)
  {
    realTimeClockOff();
    fan_on = false;
  }
  ventLeftRight();
  lcd.setCursor(0, 0);
  lcd.print("ERROR           ");
  lcd.setCursor(0, 1);
  lcd.print("Water level: LOW");
  delay(1000);
  tempLCD();
}

//////////////////////////
// realTimeClockOff Function
//////////////////////////
void realTimeClockOff()
{
  dt = clock.getDateTime();

  Serial.print("The motor turned off at time: ");
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
}

//////////////////////////
// realTimeClockOn Function
//////////////////////////
void realTimeClockOn()
{
  dt = clock.getDateTime();

  Serial.print("The motor turned on at time: ");
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
}

//////////////////////////
// displayWaterLevel Function
//////////////////////////
void displayWaterLevel(unsigned int waterLevel)
{
  // Disabled State = Red LED
  if (waterLevel >= 0 && waterLevel <= 100)
  {
    Serial.println("Water Level: LOW");
    // set pin 5 low, turn off fan
    *portE = 0xF7;
    lcd.setCursor(0, 0);
    lcd.print("Water level: ");
    lcd.setCursor(0, 1);
    lcd.print("LOW             ");
  }
  // Idle State = Green LED
  else
  {
    Serial.println("Water Level: Okay");
  }
}

//////////////////////////
// tempFan Function
//////////////////////////
void tempFan()
{
  if (DHT.temperature >= 21.00)
  {
    // set pin 5 high, turn on fan
    *portE |= 0x08;
    if (!fan_on)
    {
      fan_on = true;
      realTimeClockOn();
    }
  }
  else
  {
    // set pin 5 low, turn off fan
    *portE &= 0xF7;
    if (fan_on)
    {
      fan_on = false;
      realTimeClockOff();
    }
  }
}

//////////////////////////
// tempLCD Function
//////////////////////////
void tempLCD()
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
}

//////////////////////////
// ventLeftRight Function
//////////////////////////
void ventLeftRight()
{
  // read the input on analog pin A0:
  int analogValue = adcRead(6);
  // Rescale to potentiometer's voltage (from 0V to 5V):
  float voltage = floatMap(analogValue, 0, 1023, 0, 5);

  // print out the value you read
  if (((voltage * 270) / 5) < 20)
  {
    Serial.print("Vent is not moving.\n");
  }
  else if (((voltage * 270) / 5) > 20 && ((voltage * 270) / 5) < 40)
  {
    Serial.print("Vent is moving to the left.\n");
    *portH |= 0x08;
    delay(500);
    *portH &= 0xF7;
  }
  else if (((voltage * 270) / 5) > 50 && ((voltage * 270) / 5) < 80)
  {
    Serial.print("Vent is moving to the right.\n");
    *portH |= 0x10;
    delay(500);
    *portH &= 0xEF;
  }
  else
  {
    //does nothhing
  }
}

//////////////////////////
// floatMap Function
//////////////////////////
float floatMap(float x, float in_min, float in_max, float out_min, float out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}