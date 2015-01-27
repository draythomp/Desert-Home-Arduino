#include <TimerThree.h>
// pick this library up at http://www.arduino.cc/playground/Code/Timer1
// scroll down the page to the Timer3 entry.

unsigned long resetTimer = 0;
unsigned long timerRate = 2000000;  // this is the MICRO seconds to wait between interrupts
int resetWait = 10000;              // this variable is in MILLI seconds for how long to wait
                                    // before resetting

void(* resetFunc) (void) = 0; //declare reset function @ address 0

void resetMe(){  // There are certain compiler items that this little trick
  resetFunc();   // can over come.  Like putting this in a callback.
}

void checkActive(){
  // this routine gets called every timerRate (see above)
  // and here I check resetWait to see if the device has been 
  // hung up for resetWait number of milliseconds
  if(millis() - resetTimer > resetWait){
    Serial.println("Fake Watchdog Timer Reset..........");
    resetMe();
  }
  Serial.print("Timer3 at "); // uncomment these two lines to watch what's
  Serial.println(millis());   // going on
}

void setup()
{
  int errorCount = 0;
  Serial.begin(57600);
  Serial.println("Initializing..");
}

boolean firsttime = true;

void loop(){
  if(firsttime == true){
    Serial.print("First time through loop, set up timer");
    firsttime = false;
    resetTimer = millis();
    // set timer3 to expire in 2 seconds.  So, every 2 seconds the checkActive routine
    // will get called and it will check the resetTimer variable to see how long in 
    // milliseconds it has been since the variable was last updated.
    Timer3.initialize(timerRate);  // This sets timer3 to expire in 2 seconds
    Timer3.attachInterrupt(checkActive);
    // Note that this could have been in the setup() routine above, but
    // I put it here because sometimes I have a LOT of stuff to do during
    // the setup() routine.
  }
  // to test this, comment out the line below and the board should reboot.
  resetTimer = millis(); // This keeps resetting the timer
}

