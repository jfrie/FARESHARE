/********************************************************
  FARESHARE
  Written by Jude Frie
  University of Western Ontario
  jfrie2@uwo.ca
  July 18, 2024

  FED0 is a bare bones pellet dispenser.

  FED was originally developed by Nguyen at al and published in 2016:
  https://www.ncbi.nlm.nih.gov/pubmed/27060385

  This code includes code from:
  *** Adafruit, who made the hardware breakout boards and associated code we used in FED ***

  This project is released under the terms of the GNU GENERAL PUBLIC LICENSE (GPL-3.0 license)
  Copyright (c) 2024 Jude Frie
********************************************************/

#include <SoftwareSerial.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "SparkFun_Qwiic_OpenLog_Arduino_Library.h"
#include <CapacitiveSensor.h>

// define capacitive sensor pins
#define sensePin 2
#define sendPin 3

// A4988 control pins
#define dirPin 4  // Direction
#define stepPin 5 // Step

// LED display setup
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define OLED_RESET -1    // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// set pin for reseting RFID
#define RFIDResetPin 7

// set pin for button press
#define buttonPin 8

// set pins for software serial
#define RX 11
#define TX 12

// set RFID tag length
#define tagLength 13

// create instance of logger
OpenLog logger;

// known RFID tags: replace, add, or remove tag IDs as required.
// It is currently configured for 4 rats and a tag for priming the pump tubing.
//******************************************************************************************** try adding additional tags and see if effects volume
char tag1[tagLength] = "F83D103CBC87"; // rat 1 tag, comment when testing
char tag2[tagLength] = "C23D103CBC87"; // rat 2 tag, comment when testing
char tag3[tagLength] = "043D103CBC87"; // rat 3 tag
char tag4[tagLength] = "223D103CBC87"; // rat 4 tag

//char tag5[tagLength] = ""; // next rat tag 

//char tag1[tagLength] = "000C9522D962"; // tag for testing, comment when not testing

char tagString[tagLength]; // new tag place holder

// Total drinking volumes for each rat. Add or remove for the # of rats being used.
float rat_1_total_volume = 0;
float rat_2_total_volume = 0;
float rat_3_total_volume = 0;
float rat_4_total_volume = 0;
// float rat_5_total_volume = 0;

// Total number of licks for each rat. Add or remove for the # of rats being used.
int rat_1_total_licks = 0;
int rat_2_total_licks = 0;
int rat_3_total_licks = 0;
int rat_4_total_licks = 0;
// int rat_5_total_licks = 0;

int time_allowed = 2000;
int lick_reg_cap = 1800;
int lick_end_cap = 400;


// Initializing variables to 0
int licks = 0;
long cap_value = 0;
long cap_value_init = 0;
float bout_volume = 0;
unsigned long bout_start = 0;
unsigned long bout_end = 0;
int tick = 0;
unsigned long start = 0;
byte buttonState = 0;

// flow_scale and motor_scale may need to be adjusted for volume measurement and rate to be accurate
float flow_scale = 0.2707092583;
float flow_rate = 0.00744 * flow_scale;

// Instantiate software serial
SoftwareSerial serial(RX, TX);

// Create capacitive sensor object
CapacitiveSensor cap_val = CapacitiveSensor(sendPin, sensePin);

void setup()
{

    pinMode(buttonPin, INPUT_PULLUP); // set button pin as input with a pull-up resistance
    

    //logger file setup
    Wire.begin();
    logger.begin();

    // setup file for rat 1
    logger.append("rat1_drinking_data.csv");
    logger.print("Start Time (ms)");
    logger.print(",");
    logger.print("End Time (ms)");
    logger.print(",");
    logger.print("Bout Volume (mL)");
    logger.print(",");
    logger.println("Bout Licks");
    logger.syncFile();

    // setup file for rat 2
    logger.append("rat2_drinking_data.csv");
    logger.print("Start Time (ms)");
    logger.print(",");
    logger.print("End Time (ms)");
    logger.print(",");
    logger.print("Bout Volume (mL)");
    logger.print(",");
    logger.println("Bout Licks");
    logger.syncFile();

    // setup file for rat 3
    logger.append("rat3_drinking_data.csv");
    logger.print("Start Time (ms)");
    logger.print(",");
    logger.print("End Time (ms)");
    logger.print(",");
    logger.print("Bout Volume (mL)");
    logger.print(",");
    logger.println("Bout Licks");
    logger.syncFile();

    // setup file for rat 4
    logger.append("rat4_drinking_data.csv");
    logger.print("Start Time (ms)");
    logger.print(",");
    logger.print("End Time (ms)");
    logger.print(",");
    logger.print("Bout Volume (mL)");
    logger.print(",");
    logger.println("Bout Licks");
    logger.syncFile();

    // Additional rats can be added here
    // logger.append("rat5_drinking_data.csv");
    // logger.print("Start Time (ms)");
    // logger.print(",");
    // logger.print("End Time (ms)");
    // logger.print(",");
    // logger.print("Bout Volume (mL)");
    // logger.print(",");
    // logger.println("Bout Licks");
    // logger.syncFile();

    // Setup motor control pins as Outputs
    pinMode(stepPin, OUTPUT);
    pinMode(dirPin, OUTPUT);

    // set motor clockwise
    digitalWrite(dirPin, HIGH);

    // start serials
    serial.begin(9600);

    // LED display settings and initialization
    display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
    display.clearDisplay();
    updateDisplayRat1();
    updateDisplayRat2();
    updateDisplayRat3();
    updateDisplayRat4();
    display.display();

    // set reset pin high as default (keeps RFID on)
    pinMode(RFIDResetPin, OUTPUT);
    digitalWrite(RFIDResetPin, HIGH);
}

void loop()
{
    // if priming button is pushed, activate motor and update display
    buttonState = digitalRead(buttonPin);
    if (buttonState == LOW)
    {
        for (int i=0;i<10;i++){
            activate_motor();
        }   
    }

    readTag();         // read tag
    checkTag(tagString); // check if match
    clearTag(tagString); // clear tag
    resetReader();     // reset the RFID reader
    
    // Update Display
    updateDisplayRat1();
    updateDisplayRat2();
    updateDisplayRat3();
    updateDisplayRat4();
}

//Check tag against known tags
void checkTag(char tag[])
{
    // When no tag is present, bout is over. Reinitialize licks and bout volume to 0.
    if (strlen(tag) == 0)
    {
        bout_volume = 0;
        licks = 0;
        return;
    }

    // rat 1
    //********************************************************
    if (compareTag(tag, tag1))
    { // if senses tag 1

        // timer for last tag read
        start = millis();

        // record bout start time
        bout_start = millis();

        // reset bout end to bout start
        bout_end = bout_start;

        // While the tag present is being sensed or less than 2000ms has passed since tag present
        while ((compareTag(tag, tag1)) || (((millis() - start) < time_allowed))) 
        { 

            // reset last read timer if tag is read
            if (compareTag(tag, tag1))
            {
                start = millis();
            }

            // prepare RFID for next read
            clearTag(tagString);
            resetReader();

            // read lickometer
            cap_value = cap_val.capacitiveSensor(100);
            
            // if a lick is registered
            if (cap_value > lick_reg_cap)
            {
              // wait until the tongue is retracted
              while (cap_value > lick_end_cap){ 
                cap_value = cap_val.capacitiveSensor(100);
              }

              // count licks
              licks++;
              rat_1_total_licks++; 

              // update bout_end
              bout_end = millis();

              // deliver small volume to straw
              activate_motor();

              // count volume
              bout_volume = bout_volume + flow_rate;
              rat_1_total_volume = rat_1_total_volume + flow_rate;
              
              // show current total volume and licks for rat on display
              updateDisplayRat1();
            }
            readTag();
        }

        // if a bout has occured and is finished, save the results
        if (licks > 0)
        {
            logger.append("rat1_drinking_data.csv");
            logger.print(bout_start);
            logger.print(",");
            logger.print(bout_end);
            logger.print(",");
            logger.print(bout_volume, 4);
            logger.print(",");
            logger.println(licks);
            logger.syncFile();
        }

        //********************************************************

        // rat 2
        //********************************************************
    }
    else if (compareTag(tag, tag2))
    {
        start = millis();
        bout_start = millis();
        bout_end = bout_start;
        while ((compareTag(tag, tag2)) || (((millis() - start) < time_allowed)))
        {
            if (compareTag(tag, tag2))
            {
                start = millis();
            }
            clearTag(tagString);
            resetReader();
            cap_value = cap_val.capacitiveSensor(100);
            if (cap_value > lick_reg_cap)
            {
              while (cap_value > lick_end_cap){
                cap_value = cap_val.capacitiveSensor(100);
              }
              licks++;
              rat_2_total_licks++; 
              bout_end = millis();
              activate_motor();
              bout_volume = bout_volume + flow_rate;
              rat_2_total_volume = rat_2_total_volume + flow_rate;
              updateDisplayRat2();
            }
            readTag();
        }
        if (licks > 0)
        {
            logger.append("rat2_drinking_data.csv");
            logger.print(bout_start);
            logger.print(",");
            logger.print(bout_end);
            logger.print(",");
            logger.print(bout_volume, 4);
            logger.print(",");
            logger.println(licks);
            logger.syncFile();
        }
        //********************************************************

        // rat 3
        //********************************************************
    }
    else if (compareTag(tag, tag3))
    {
        start = millis();
        bout_start = millis();
        bout_end = bout_start;
        while ((compareTag(tag, tag3)) || (((millis() - start) < time_allowed)))
        {
            if (compareTag(tag, tag3))
            {
                start = millis();
            }
            clearTag(tagString);
            resetReader();
            cap_value = cap_val.capacitiveSensor(100);
            if (cap_value > lick_reg_cap)
            {
              while (cap_value > lick_end_cap){
                cap_value = cap_val.capacitiveSensor(100);
              }
              licks++;
              rat_3_total_licks++; 
              bout_end = millis();
              activate_motor();
              bout_volume = bout_volume + flow_rate;
              rat_3_total_volume = rat_3_total_volume + flow_rate;
              updateDisplayRat3();
            }
            readTag();
        }
        if (licks > 0)
        {
            logger.append("rat3_drinking_data.csv");
            logger.print(bout_start);
            logger.print(",");
            logger.print(bout_end);
            logger.print(",");
            logger.print(bout_volume, 4);
            logger.print(",");
            logger.println(licks);
            logger.syncFile();
        }
        //********************************************************

        // rat 4
        //********************************************************
    }
    else if (compareTag(tag, tag4))
    {
        start = millis();
        bout_start = millis();
        bout_end = bout_start;
        while ((compareTag(tag, tag4)) || (((millis() - start) < time_allowed)))
        {
            if (compareTag(tag, tag4))
            {
                start = millis();
            }
            clearTag(tagString);
            resetReader();
            cap_value = cap_val.capacitiveSensor(100);
            Serial.println(cap_value);
            if (cap_value > lick_reg_cap)
            {
              while (cap_value > lick_end_cap){
                cap_value = cap_val.capacitiveSensor(100);
                Serial.println(cap_value);
              }
              licks++;
              rat_4_total_licks++; 
              bout_end = millis();
              activate_motor();
              bout_volume = bout_volume + flow_rate;
              rat_4_total_volume = rat_4_total_volume + flow_rate;
              updateDisplayRat4();
            }
            readTag();
        }
        if (licks > 0)
        {
            logger.append("rat4_drinking_data.csv");
            logger.print(bout_start);
            logger.print(",");
            logger.print(bout_end);
            logger.print(",");
            logger.print(bout_volume, 4);
            logger.print(",");
            logger.println(licks);
            logger.syncFile();
        }
        //********************************************************

        // Additional rat
        //********************************************************
        // } else if (compareTag(tag, tag5)) {
        //...
        //  }
        //********************************************************

        // for unknown tag
    }
    else
    {
        display.clearDisplay();
        display.setCursor(0, 0);
        display.println(F("Unknown:"));
        display.print(tag);
        display.display();
        delay(2000);
    }
    //********************************************************
    
}

// reset RFID reader so that it checks for a new tag
void resetReader()
{
        digitalWrite(RFIDResetPin, LOW);
        digitalWrite(RFIDResetPin, HIGH);
}

// clears the current tag being read by the RFID reader
void clearTag(char one[])
{
    for (int i = 0; i < strlen(one); i++)
    {
        one[i] = 0;
    }
}

// compares the tag currently being read with each of the known tags
boolean compareTag(char one[], char two[])
{
    if (strlen(one) == 0)
        return false;
    for (int i = 0; i < 12; i++)
    {
        if (one[i] != two[i])
            return false;
    }
    return true;
}

// read the current tag
void readTag()
{
    int index = 0;
    boolean reading = false;
    while (serial.available())
    {
        int readByte = serial.read(); //read next available byte
        if (readByte == 2)
            reading = true; // begining of tag
        if (readByte == 3)
            reading = false; // end of tag
        if (reading && readByte != 2 && readByte != 3 && readByte != 10 && readByte != 13 && index < 12)
        {
            //store the tag
            tagString[index] = readByte;
            index++;
        }
        if (index == 12)
            tagString[12] = '\0';
    }
}

// Updates the display for rat 1 only
void updateDisplayRat1()
{
    display.setCursor(0, 0);
    display.print(F("R1:"));
    display.print(rat_1_total_volume, 2);
    display.print(F("mL"));
    display.print(F(","));
    display.print(rat_1_total_licks);
    display.print(F("Lk     "));
    display.display();
}

// Updates the display for rat 2 only
void updateDisplayRat2()
{
    display.setCursor(0, 8);
    display.print(F("R2:"));
    display.print(rat_2_total_volume, 2);
    display.print(F("mL"));
    display.print(F(","));
    display.print(rat_2_total_licks);
    display.println(F("Lk"));
    display.display();
}

// Updates the display for rat 3 only
void updateDisplayRat3()
{
    display.setCursor(0, 16);
    display.print(F("R3:"));
    display.print(rat_3_total_volume, 2);
    display.print(F("mL"));
    display.print(F(","));
    display.print(rat_3_total_licks);
    display.println(F("Lk"));
    display.display();
}

// Updates the display for rat 4 only
void updateDisplayRat4()
{
    display.setCursor(0, 24);
    display.print(F("R4:"));
    display.print(rat_4_total_volume, 2);
    display.print(F("mL"));
    display.print(F(","));
    display.print(rat_4_total_licks);
    display.println(F("Lk"));
    display.display();
}

// Turn the motor
void activate_motor()
{
  for (int i=0; i<4; i++) {
    digitalWrite(stepPin, HIGH);
    delayMicroseconds(500);
    digitalWrite(stepPin, LOW);
    delayMicroseconds(5000);
  }
}

