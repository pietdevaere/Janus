
/* Detects patterns of pushes and triggers a motor to unlock
   it if the pattern is correct.
   
   Original Code by Steve Hoefer http://grathio.com
   Version 0.1.10.20.10
   Licensed under Creative Commons Attribution-Noncommercial-Share Alike 3.0
   http://creativecommons.org/licenses/by-nc-sa/3.0/us/
   (In short: Do what you want, just be sure to include this line and the four above it, and don't sell it or use it in anything you sell without contacting me.)
   Modifications by Piet De Vaere

   Digital Pin 2: IN Switch to enter a new code.  Short this to enter programming mode.
   Digital Pin 3: OUT Relay to open door connected here. (Or a motor controller or a solenoid or other unlocking mechanisim.)
   Digital Pin 4: OUT Red LED. 
   Digital Pin 5: OUT Green LED. 
   Digital pin 6: IN doorBel.
   Digtial pin 7: OUT Buzzer.
   Future: Digital pin 8: IN Party mode switch
   Furute: Digital pin 9: gateopener enable switch
   secret knocks are stored in the EEPROM at adress 0 to maximumPushes-1
   
   Update: Nov 09 09: Fixed red/green LED error in the comments. Code is unchanged. 
   Update: Nov 20 09: Updated handling of programming button to make it more intuitive, give better feedback.
   Update: Jan 20 10: Removed the "pinMode(doorbell, OUTPUT);" line since it makes no sense and doesn't do anything.
   -- Modifications Below are by Piet De Vaere
   Update: Mar 21 12: changed the code to work with a switch instead of a piezo buzzer
   Update: Mar 21 12: Store secretKnock in EEPROM, removed the default secret knock since it makes no sense anymore
   
 */
 
#include <EEPROM.h> 
#include "pitches.h"
// Pin definitions
const int doorbell = 9;         // Piezo sensor on pin 0.
const int buzzer = 7;            // pin the buzzer is conected to
const int programSwitch = 6;       // If this is high we program a new code.
const int doorUnlock = 8;           // Gear motor used to turn the lock.
const int redLED = 5;              // Status LED
const int greenLED = 4;            // Status LED
 
// Tuning constants.  Could be made vars and hoooked to potentiometers for soft configuration, etc.
const int rejectValue = 25;        // If an individual push is off by this percentage of a push we don't unlock..
const int averageRejectValue = 15; // If the average timing of the pushes is off by this percent we don't unlock.
const int debounceTime = 300;     // milliseconds we allow a push to fade before we listen for another one. (Debounce timer.)
const int UnlockTime = 1000;      // milliseconds that we pull up the relay to unlock door.
const int maximumPushes = 20;       // Maximum number of pushes to listen for.
const int pushComplete = 1200;     // Longest time to wait for a push before we assume that it's finished.


// Variables.
int secretCode[maximumPushes];  // Initial setup is in EEPROM
int pushReadings[maximumPushes];   // When someone pushes this array fills with delays between pushes.
int doorbellValue = 0;           // Last reading of the push sensor.
int programButtonPressed = false;   // Flag so we remember the programming button setting at the end of the cycle.

void setup() {
  for (int i = 0; i < maximumPushes; i++){ // Read the secretCode from the EEPROM
    secretCode[i] = EEPROM.read(i);
  }
  pinMode(doorUnlock, OUTPUT);
  pinMode(redLED, OUTPUT);
  pinMode(greenLED, OUTPUT);
  pinMode(buzzer, OUTPUT);
  pinMode(programSwitch, INPUT);
  
  Serial.begin(9600);               			// Uncomment the Serial.bla lines for debugging.
  Serial.println("Program start.");  			// but feel free to comment them out after it's working right.
  
  digitalWrite(greenLED, HIGH);      // Green LED on, everything is go.
}

void loop() {
  // Listen for any push at all.
  doorbellValue = digitalRead(doorbell);
  int posibleCodeMillis = millis();
  if (digitalRead(programSwitch)==HIGH){  // is the program button pressed?
    programButtonPressed = true;          // Yes, so lets save that state
    digitalWrite(redLED, HIGH);           // and turn on the red light too so we know we're programming.
  } else {
    programButtonPressed = false;
    digitalWrite(redLED, LOW);
  }
  
  if (doorbellValue == HIGH){
    listenToSecretPush();
  }
} 

// Records the timing of pushes.
void listenToSecretPush(){
  Serial.println("push starting");   

  int i = 0;
  // First lets reset the listening array.
  for (i=0;i<maximumPushes;i++){
    pushReadings[i]=0;
  }
  
  int currentPushNumber=0;         			// Incrementer for the array.
  int startTime=millis();           			// Reference for when this push started.
  int now=millis();
  
  digitalWrite(greenLED, LOW);      			// we blink the LED for a bit as a visual indicator of the push.
  if (programButtonPressed==true){
     digitalWrite(redLED, LOW);                         // and the red one too if we're programming a new push.
  }
  delay(debounceTime);                       	        // wait for this peak to fade before we listen to the next one.
  digitalWrite(greenLED, HIGH);  
  if (programButtonPressed==true){
     digitalWrite(redLED, HIGH);                        
  }
  do {
    //listen for the next push or wait for it to timeout. 
    doorbellValue = digitalRead(doorbell);
    if (doorbellValue == HIGH){                   //got another push...
      //record the delay time.
      Serial.println("push.");
      now=millis();
      pushReadings[currentPushNumber] = now-startTime;
      currentPushNumber ++;                             //increment the counter
      startTime=now;          
      // and reset our timer for the next push
      digitalWrite(greenLED, LOW);  
      if (programButtonPressed==true){
        digitalWrite(redLED, LOW);                       // and the red one too if we're programming a new code.
      }
      delay(debounceTime);                              // again, a little delay to let the push decay.
      digitalWrite(greenLED, HIGH);
      if (programButtonPressed==true){
        digitalWrite(redLED, HIGH);                         
      }
    }

    now=millis();
    
    //did we timeout or run out of pushes?
  } while ((now-startTime < pushComplete) && (currentPushNumber < maximumPushes));
  
  //we've got our push recorded, lets see if it's valid
  if (programButtonPressed==false){             // only if we're not in progrmaing mode.
    if (validatePush() == true){
      triggerDoorUnlock(); 
    } else {
      Serial.println("Secret push failed, ringing doorbell");
      ringDoorbell();
      digitalWrite(greenLED, LOW);  		// We didn't unlock, so blink the red LED as visual feedback.
      for (i=0;i<4;i++){					
        digitalWrite(redLED, HIGH);
        delay(100);
        digitalWrite(redLED, LOW);
        delay(100);
      }
      digitalWrite(greenLED, HIGH);
    }
  } else { // if we're in programming mode we still validate the lock, we just don't do anything with the lock
    validatePush();
    // and we blink the green and red alternately to show that program is complete.
    for (int i = 0; i < maximumPushes; i++){  // Write the new secretCode to the EEPROM
      EEPROM.write(i, secretCode[i]);
    }
    Serial.println("New lock stored.");
    digitalWrite(redLED, LOW);
    digitalWrite(greenLED, HIGH);
    for (i=0;i<3;i++){
      delay(100);
      digitalWrite(redLED, HIGH);
      digitalWrite(greenLED, LOW);
      delay(100);
      digitalWrite(redLED, LOW);
      digitalWrite(greenLED, HIGH);      
    }
  }
}


// Runs the motor (or whatever) to unlock the door.
void triggerDoorUnlock(){
  Serial.println("Door unlocked!");
  int i=0;
  
  // turn the motor on for a bit.
  digitalWrite(doorUnlock, HIGH);
  digitalWrite(greenLED, HIGH);            // And the green LED too.
  
  delay (UnlockTime);                    // Wait a bit.
  
  digitalWrite(doorUnlock, LOW);            // Turn the motor off.
  
  // Blink the green LED a few times for more visual feedback.
  for (i=0; i < 5; i++){   
      digitalWrite(greenLED, LOW);
      delay(100);
      digitalWrite(greenLED, HIGH);
      delay(100);
  }
   
}

// Sees if our push matches the secret.
// returns true if it's a good push, false if it's not.
// todo: break it into smaller functions for readability.
boolean validatePush(){
  int i=0;
 
  // simplest check first: Did we get the right number of pushes?
  int currentPushCount = 0;
  int secretPushCount = 0;
  int maxPushInterval = 0;          			// We use this later to normalize the times.
  
  for (i=0;i<maximumPushes;i++){
    if (pushReadings[i] > 0){
      currentPushCount++;
    }
    if (secretCode[i] > 0){  					//todo: precalculate this.
      secretPushCount++;
    }
    
    if (pushReadings[i] > maxPushInterval){ 	// collect normalization data while we're looping.
      maxPushInterval = pushReadings[i];
    }
  }
  
  // If we're recording a new push, save the info and get out of here.
  if (programButtonPressed==true){
      for (i=0;i<maximumPushes;i++){ // normalize the times
        secretCode[i]= map(pushReadings[i],0, maxPushInterval, 0, 100); 
      }
      // And flash the lights in the recorded pattern to let us know it's been programmed.
      digitalWrite(greenLED, LOW);
      digitalWrite(redLED, LOW);
      delay(1000);
      digitalWrite(greenLED, HIGH);
      digitalWrite(redLED, HIGH);
      delay(50);
      for (i = 0; i < maximumPushes ; i++){
        digitalWrite(greenLED, LOW);
        digitalWrite(redLED, LOW);  
        // only turn it on if there's a delay
        if (secretCode[i] > 0){                                   
          delay( map(secretCode[i],0, 100, 0, maxPushInterval)); // Expand the time back out to what it was.  Roughly. 
          digitalWrite(greenLED, HIGH);
          digitalWrite(redLED, HIGH);
        }
        delay(50);
      }
	  return false; 	// We don't unlock the door when we are recording a new push.
  }
  
  if (currentPushCount != secretPushCount){
    return false; 
  }
  
  /*  Now we compare the relative intervals of our pushes, not the absolute time between them.
      (ie: if you do the same pattern slow or fast it should still open the door.)
      This makes it less picky, which while making it less secure can also make it
      less of a pain to use if you're tempo is a little slow or fast. 
  */
  int totaltimeDifferences=0;
  int timeDiff=0;
  for (i=0;i<maximumPushes;i++){ // Normalize the times
    pushReadings[i]= map(pushReadings[i],0, maxPushInterval, 0, 100);      
    timeDiff = abs(pushReadings[i]-secretCode[i]);
    if (timeDiff > rejectValue){ // Individual value too far out of whack
      return false;
    }
    totaltimeDifferences += timeDiff;
  }
  // It can also fail if the whole thing is too inaccurate.
  if (totaltimeDifferences/secretPushCount>averageRejectValue){
    return false; 
  }
  
  return true;
  
}

void ringDoorbell(){
  digitalWrite(buzzer, HIGH);
  delay(1000);
  digitalWrite(buzzer, LOW);
}
