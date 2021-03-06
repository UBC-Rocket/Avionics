
/*
 * UBC ROCKET - Print SD Data to Serial
 */

#include <i2c_t3.h>
#include <TimerOne.h>
#include <stdio.h>
#include <SD.h>
#include <SD_t3.h>
#include <SPI.h>


const int LED_PIN = 7;  

File flightData;

#define BUFFER_SIZE 10
#define CS 4 //chip select pin
#define DIN 12 // MISO data into teensy, out of sd pin
#define DOUT 11 // MOSI data out of teensy, into sd pin
#define CLK 13

// make a string for assembling the data to log:
String dataString = "";
    
void setup() {
  Serial.begin(9600);
  
  //Serial.print("Initializing SD card...");
  
  //must initialize teensy pin 10 as output or SD lib does not work
  pinMode(10, OUTPUT);

  //LED ON as start signal
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  
  delay(2000);
  
  //Serial.println("Before SD begin");  
  // see if the card is present and can be initialized:
  if (!SD.begin(CS)) {
    Serial.println("Card failed, or not present");
    return;
  }
  Serial.println("Card successfully initialized");
  Serial.println("---------------");

  /*if(SD.mkdir("/test")){
    Serial.println("Test directory created");
  }*/

  //add the following commented out code in to stop saving data and print
 
  Serial.println("Accessing root directory...");
  File root = SD.open("/");
  Serial.println("---------------");
  
  printDirectory(root, 0);
  
  if(!SD.exists("FLIGHT~2.TXT")){
    Serial.println("File does not exist");
    return;
  }
  
  Serial.println("---------------");
  Serial.println("Reading test file...");
  Serial.println("---------------");
  
  File myfile = SD.open("FLIGHT~2.TXT");
  

  
  // if the file is available, read the file
  if (myfile) 
  {
    while (myfile.available())
    {
      Serial.write(myfile.read());
    }
    myfile.close();
  }  
  // if the file cannot be opened give error report
  else {
    Serial.println("error opening the text file");
  } 

  
  //tried to reset/overwrite file -- did not work
  Serial.println(myfile.size());
  Serial.println("Resetting position...");
  myfile.seek(0);
  Serial.println("New position:");
  Serial.println(myfile.position());

  
  digitalWrite(LED_PIN, LOW);

  return;
  
}

void loop(){
}

void printDirectory(File dir, int numTabs) {
  while (true) {

    File entry =  dir.openNextFile();
    if (! entry) {
      // no more files
      break;
    }
    for (uint8_t i = 0; i < numTabs; i++) {
      Serial.print('\t');
    }
    Serial.print(entry.name());
    if (entry.isDirectory()) {
      Serial.println("/");
      printDirectory(entry, numTabs + 1);
    } else {
      // files have sizes, directories do not
      Serial.print("\t\t");
      Serial.println(entry.size(), DEC);
    }
    entry.close();
  }

}


