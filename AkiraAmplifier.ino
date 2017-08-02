#include <OneButton.h>
#include <Encoder.h>
#include <PT2314.h>
#include <WSWire.h>
#include <EEPROM.h>

#include <LiquidCrystal.h>
PT2314 audio;
LiquidCrystal lcd(7, 8, 9, 10, 11, 12); 
long lastMemWrite;
// ROTARY ENCODER
long oldPosition  = 0;
int encoderDTpin = 2;
int encoderCLKpin = 3;
Encoder myEnc(encoderDTpin, encoderCLKpin);

int buttonPin = 4;
OneButton button0(buttonPin, true);
bool valuesChanged = false;
bool insideMenu = true;
// MENU ARRAY 
// First number in array = amount of menu items in root layer. Same goes for next layer. (Example: first value in array is 4. This means Layer 0 has 4 menu items. Layer 1 has only 2 menu items.)
// First number infront of menu text is the layer number that item points to. (Example: 1_Rotation points to Layer 1)
// Second number infront of menu text is the number of the value connected to that menu item in the values array. (Example: 15Direction points to position 5 in the values array)

String menu[] = {"6","009Volume","000Input","1__EQ","019Loudness","023Balance","028Mute",       // Layer 0
                 "2","104LOW","114HIGH",                                        // Layer 1
                 "3","256Active","2__Item2.2","2__Item2.3",                    // Layer 2
                 "4","4__Watch 1","3__Watch 2","3__Watch 3","3__Watch 4",     // Layer 3
                 "2","4__Item3.1.1","4__Item3.1.2"};                          // Layer 4

// VALUES ARRAY
// TYPES, 1 numeric / 2 character value range
// 1 - type,value,increments,min,max
// 2 - type,starting value in options array,lowest value options array,max value options array
int values[] = { 
                2,0,0,3,         //0-3 Input
                1,0,1,-50,50,     //4-8 EQ-LOW
                1,0,1,0,80, //9-13 VOLUME
                1,0,1,-50,50, //14-18 EQ-HIGH
                2,5,4,5,         //19-22 Loudness
                1,0,1,-50,50, //23-27 Balance
                
                2,4,4,5, //28-32 Mute
                2,2,0,2,         //33-36
                1,600,25,75,600, //37-41
                 
                1,11,25,50,1450, //42-46 WATCH4
                2,2,0,2,         //47-50
                1,600,25,75,600, //51-55
                
                2,5,4,5,         //56-59 WATCH1 ACTIVE
                2,5,4,5,         //60-63 WATCH2 ACTIVE
                2,5,4,5,         //64-67 WATCH3 ACTIVE
                2,5,4,5};        //68-71 WATCH4 ACTIVE
                
                
String options[] = {"AUX","VCD","CD","AC-3",
                    "No","Yes"};                 

// Custom character for LCD.
byte cursor[8] = {
  0b10000,
  0b10000,
  0b01000,
  0b00110,
  0b01000,
  0b10000,
  0b10000,
  0b00000
};

byte watch[8] = {
  0b01110,
  0b01110,
  0b11011,
  0b10001,
  0b10001,
  0b11011,
  0b01110,
  0b01110
};


int currentType = 0;
int value = 0;
int numIncr = 0;
int Min = 0;
int Max = 0;

int currentLayer = 0;
int lastLayer = 99;
int currentLength = menu[0].toInt();
int currentPosition = 0;
int currentSelect = 1;
int currentChange = 0;
String currentPress = "";
float currentlcdLayerFloat = 0;
int currentlcdLayer = 0;
int lastlcdLayer = 0;
int lcdUpdated = 0;
int currentLine = 0;
int cursorLayer = 0;
int cursorPosition = 0;
int valueLength = 0;

/*
int homeScreen = 1;

String currentTime = "00:00:00";
String lastTime = "00:00:00";
String temptime = "00";
String currentDate = "";

int watch1active = values[56] - 3;
int watch2active = values[60] - 3;
int watch3active = values[64] - 3;
int watch4active = values[68] - 3;
int watch1rotations = 0;
int watch2rotations = 0;
int watch3rotations = 0;
int watch4rotations = 0;
int watch1left = values[1];
int watch2left = values[15];
int watch3left = values[29];
int watch4left = values[43];
String temprotationsleft = "";
*/


void setup() {
  Serial.begin(9600);
  pinMode(5,OUTPUT);
  pinMode(2, INPUT_PULLUP);
  pinMode(3, INPUT_PULLUP);
  pinMode(4, INPUT_PULLUP);
  Wire.begin();
  audio.init();
  readEEPROM();
  // Map rotary button to actions single and doubleclick.
  button0.attachClick(singleClick);
  button0.attachDoubleClick(doubleClick);

  lcd.begin (16,2);
  lcd.home();
  lcd.createChar(0, cursor);
  lcd.createChar(1, watch);
  lcd.setCursor (2,0);  
  lcd.print("Starting...");
  
  delay(1000);
  digitalWrite(5,HIGH);
  clearLine(0);

  // Set the select cursor on the first line.
  cursorLayer = 0;
  lcd.setCursor (0,cursorLayer);  
  writeCursor();

}

void loop() {
    
    button0.tick();
    rotary_check();
    lcdMenu();
    if(valuesChanged)
      updatePT2314();
}

void singleClick() {

  // IF current layer number is the same as redirect layer number, we stay on the same layer
  // AND no item is selected in the menu
  // AND the selected item has a redirect to another layer
  if(currentLayer == menu[currentPosition + currentSelect].substring(0,1).toInt() && menu[currentPosition + currentSelect].substring(1,3) != "__" && currentPress == ""){
    
    currentPress = menu[currentPosition + currentSelect].substring(3);
    currentChange = menu[currentPosition + currentSelect].substring(1,3).toInt();
    
    Serial.println("Currentpress: " + currentPress);
    
    currentType = values[currentChange];
    value = values[currentChange + 1];

    // Type select change is numeric
    if(currentType == 1){
      Min = values[currentChange + 3];
      Max = values[currentChange + 4];
      numIncr = values[currentChange + 2];
      
      Serial.println("Currentvalue: " + String(value));
      Serial.println("Increments: " + String(numIncr));
      Serial.println("Min: " + String(Min));
      Serial.println("Max: " + String(Max));

      
      valueLength = String(value).length();
      
    // Type select change is character range
    } else if(currentType == 2){
      Min = values[currentChange + 2];
      Max = values[currentChange + 3];

      Serial.println("Currentvalue: " + options[value]);
      Serial.println("Options: ");
      
      for (int i=Min; i <= Max; i++){
        Serial.println(options[i]);
      }

      valueLength = options[value].length();
    }

    clearSelect();
    cursorPosition = 15 - valueLength;
    lcd.setCursor(cursorPosition,cursorLayer);
    writeCursor();

  // ELSE IF something is selected in the menu, and the button is pressed again. We deselect the item.  
  } else if(currentPress != ""){
    
    currentValues();
    currentPress = "";
    currentChange = 0;

    clearChar(cursorPosition,cursorLayer);
    lcd.setCursor(0,cursorLayer);
    writeCursor();
    
  // IF nothing is selected and nothing will be deselected, move to the redirect layer listed in the first position of the array.  
  } else {
    
    currentLayer = menu[currentPosition + currentSelect].substring(0,1).toInt();
    currentSelect = 1;
    currentlcdLayerFloat = 0;
    currentlcdLayer = 0;
    lastlcdLayer = 0;

    clearSelect();
    cursorLayer = 0;
    lcd.setCursor (0,cursorLayer);  
    writeCursor();
    
  }
}

// Reset to layer 0.
void doubleClick() {
  if(currentLayer == 0){
    /*currentLayer = 0;
    currentSelect = 1;
    currentlcdLayerFloat = 0;
    currentlcdLayer = 0;
    lastlcdLayer = 0;
  
    clearSelect();
    cursorLayer = 0;
    
    lcd.clear();
    lcdUpdated = 0;
    */
    insideMenu = false;
  }
  if(currentLayer != 0){
    currentLayer = 0;
    currentSelect = 1;
    currentlcdLayerFloat = 0;
    currentlcdLayer = 0;
    lastlcdLayer = 0;
  
    clearSelect();
    cursorLayer = 0;
    lcd.setCursor (0,cursorLayer);  
    writeCursor();
  }
}

void rotary_check(){

  // Constantly read the position of the rotary encoder
  long newPosition = myEnc.read() / 4;

  // IF the new position of the encoder is different then the old position
  if (newPosition != oldPosition) {
       
    // IF nothing has been selected in the menu
    if(currentPress == ""){

      if(newPosition > oldPosition && currentSelect < currentLength){
        clearSelect();
        currentSelect++;
        currentlcdLayerFloat = currentlcdLayerFloat + 0.5;
        currentlcdLayer = currentlcdLayerFloat;

        cursorSelect();
        
        lcd.setCursor (0,cursorLayer);  
        writeCursor();
        Serial.println("> " + menu[currentPosition + currentSelect].substring(3));

      } else if (newPosition < oldPosition && currentSelect != 1){
        clearSelect();
        currentSelect--;
        currentlcdLayerFloat = currentlcdLayerFloat - 0.5;
        currentlcdLayer = currentlcdLayerFloat;

        cursorSelect();

        lcd.setCursor (0,cursorLayer);  
        writeCursor();
        Serial.println("> " + menu[currentPosition + currentSelect].substring(3));
  
      }
      
      if(lastlcdLayer != currentlcdLayer){
        lcdUpdated = 0;
      }
      
      lastlcdLayer = currentlcdLayer;      
      oldPosition = newPosition;



    // IF something has been selected in the menu, we are going to change the value of a menu item.
    } else if(currentPress != ""){

      
      lcd.setCursor(cursorPosition + 1,cursorLayer);
      // IF the new position is HIGHER then the old position
      if(newPosition > oldPosition){
        clearChar(cursorPosition,cursorLayer);
        
        // IF the selected item has a change type that is NUMERIC.
        // AND the MAXIMUM of the selected item has not been reached. 
        // We change the value UPWARDS with the icrements listed in the array.
        // SAME applies for next ELSE IF, just for the change type character. And the next ELSE IF we just go down.
        if(currentType == 1 && value < Max){
            value = value + numIncr;
            values[currentChange + 1] = value;
            Serial.println(value);
            valuesChanged = true;
            clearValue();
            valueLength = String(value).length();
            cursorPosition = 15 - valueLength;
            lcd.setCursor(cursorPosition + 1,cursorLayer);
            lcd.print(value);
          }

        if(currentType == 2 && value < Max){
            value++;
            values[currentChange + 1] = value;
            Serial.println(options[value]);
            valuesChanged = true;
            clearValue();
            valueLength = options[value].length();
            cursorPosition = 15 - valueLength;
            lcd.setCursor(cursorPosition + 1,cursorLayer);
            lcd.print(options[value]);
          }
          lcd.setCursor(cursorPosition,cursorLayer);
          writeCursor();
        
      } else {
        clearChar(cursorPosition,cursorLayer);
        
        if(currentType == 1 && value > Min){
          value = value - numIncr;
          values[currentChange + 1] = value;
          Serial.println(value);
          valuesChanged = true;
          clearValue();
          valueLength = String(value).length();
          cursorPosition = 15 - valueLength;
          lcd.setCursor(cursorPosition + 1,cursorLayer);
          lcd.print(value);
        }

        if(currentType == 2 && value > Min){
            value--;
            values[currentChange + 1] = value;
            Serial.println(options[value]);
            valuesChanged = true;
            clearValue();
            valueLength = options[value].length();
            cursorPosition = 15 - valueLength;
            lcd.setCursor(cursorPosition + 1,cursorLayer);
            lcd.print(options[value]);
          }
          lcd.setCursor(cursorPosition,cursorLayer);
          writeCursor();
        
      }

      
        
      oldPosition = newPosition;
    } else {
      oldPosition = newPosition;
    }
    
    
    
  }
}

void lcdMenu(){
  if (lastLayer != currentLayer || lcdUpdated == 0){

    currentLength = menu[0].toInt();
    currentPosition = 0;
    
    for (int i=0; i < currentLayer; i++){
      currentPosition = (menu[currentPosition].toInt() + 1) + currentPosition;
      currentLength = menu[currentPosition].toInt();
    }

    clearMenu();
    int tempPosition = currentPosition;
    
    if(currentlcdLayer != 0){
      
      currentPosition = currentPosition + (currentlcdLayer * 2);
    }
    
    Serial.println("LCD-------------");

    for (int i=1; i <= 2; i++){
      lcd.setCursor (1,i - 1);  
      lcd.print(menu[currentPosition + i].substring(3));
      
      currentType = 0;
      value = values[menu[currentPosition + i].substring(1,3).toInt() + 1];
      String tempvalue = "";
      
      if(menu[currentPosition + i].substring(1,3) != "__" && menu[currentPosition + i].substring(1,3) != ""){
        currentType = values[menu[currentPosition + i].substring(1,3).toInt()];
        if(currentType == 1){
          lcd.setCursor (16 - String(value).length(),i - 1);
          lcd.print(value);
          tempvalue = " "+ String(value);
        }else if(currentType == 2){
          lcd.setCursor (16 - options[value].length(),i - 1);
          lcd.print(options[value]);
          tempvalue = " "+ options[value];
        } 
        
      }
      
      Serial.println(menu[currentPosition + i].substring(3) + tempvalue);
    }
    Serial.println("----------------");
    
    currentPosition = tempPosition;
    lastLayer = currentLayer;
    lcdUpdated = 1;
    
    /**
    Serial.println("Layer " + String(currentLayer + 1));
    Serial.println("Layer length " + String(currentLength));
    Serial.println("Layer position " + String(currentPosition));
    **/
    
  }
}


void currentValues(){

  Serial.println("Currentpress: " + currentPress);
  
  if(currentType == 1){
      Serial.println("Currentvalue: " + String(value));
      Serial.println("Increments: " + String(numIncr));
      Serial.println("Min: " + String(Min));
      Serial.println("Max: " + String(Max));
      
    } else if(currentType == 2){
      Serial.println("Currentvalue: " + options[value]);
      Serial.println("Options: ");
      
      for (int i=Min; i <= Max; i++){
        Serial.println(options[i]);
      }
    }
}

void clearChar(int charPosition, int line){
  lcd.setCursor (charPosition,line);
  lcd.print(" ");
}

void clearLine(int line){
  lcd.setCursor (0,line);
  lcd.print("                ");
}

void clearMenu(){
  lcd.setCursor (1,0);
  lcd.print("               ");
  lcd.setCursor (1,1);
  lcd.print("               ");
}

void clearSelect(){
  lcd.setCursor (0,0);
  lcd.print(" ");
  lcd.setCursor (0,1);
  lcd.print(" ");
}

void cursorSelect(){
  switch (currentSelect){
          case 1:
            cursorLayer = 0;
            break;
          case 2:
            cursorLayer = 1;
            break;
          case 3:
            cursorLayer = 0;
            break;
          case 4:
            cursorLayer = 1;
            break;
          case 5:
            cursorLayer = 0;
            break;
          case 6:
            cursorLayer = 1;
            break;
        }
}

void writeCursor(){
  lcd.write(byte(0));
}

void clearValue(){
  for (int i = 16 - valueLength; i <= 16; i++){
        lcd.setCursor (i,cursorLayer);
        lcd.print(" ");
      } 
}

void updatePT2314() {
  audio.volume(values[10]);
  audio.channel(values[1]); 
  if(values[20] == 4)
    audio.loudnessOff();
  else
    audio.loudnessOn();
  if(values[29] == 4) {
    digitalWrite(5,HIGH);
    audio.muteOff();
  }
  else {
    digitalWrite(5,LOW);
    audio.muteOn();
  }
  audio.bass(values[5] + 50);
  audio.treble(values[15] + 50);
  if(values[24] < 0) {
    audio.attenuation(100 + values[24] * 2, 100);
  }
  else
    audio.attenuation(100, 100 - values[24] * 2);

  if(millis() - lastMemWrite > 1000) {
  EEPROM.write(0, values[10]);
  EEPROM.write(1, values[1]);
  EEPROM.write(2, values[20]);
  EEPROM.write(3, values[29]);
  EEPROM.write(4, values[5] + 50);
  EEPROM.write(5, values[15] + 50);
  EEPROM.write(6, values[24] + 50);
  lastMemWrite = millis();
  }
  valuesChanged = false;
}

void readEEPROM() {
  values[10] =  (int)EEPROM.read(0);
  values[1] =  (int)EEPROM.read(1);
  values[20] =  (int)EEPROM.read(2);
  values[29] =  (int)EEPROM.read(3);
  values[5] =  ((int)EEPROM.read(4)) - 50;
  values[15] =  ((int)EEPROM.read(5)) - 50;
  values[24] =  ((int)EEPROM.read(6)) - 50;
  valuesChanged = true;
}

