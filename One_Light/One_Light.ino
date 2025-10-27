




// the setup function runs once when you press reset or power the board
void setup() {
 

  // User Inputs
  pinMode(1, INPUT_PULLUP); // button 
  pinMode(2, INPUT_PULLUP); // switch up 
  pinMode(3, INPUT_PULLUP); // switch down 

  // LED Outputs
  pinMode(6, OUTPUT); // button LED
  pinMode(7, OUTPUT); // switch LED
  pinMode(8, OUTPUT); // wheel LED

  // Potentiometer setup
  // PotentiometerLastValue = analogRead(A0); 
}

// the loop function runs over and over again forever
void loop() {
   //global variable
  int t = 1000; // delay time, decreases with more correct inputs
  int PotentiometerStartValue = 0;
  int PotentiometerLastValue = 0;
  const int potentiometerChange = 20;
    //button input
  if (digitalRead(1) == LOW){
    digitalWrite(6, HIGH);
  } else {
    delay(t);
    digitalWrite(6,LOW);
  }

    //switch input
  if(digitalRead(2) == LOW || digitalRead(3) == LOW){
    digitalWrite(7, HIGH);
  } else {
    delay(t);
    digitalWrite(7, LOW);
  }

  //potentiometer input
  int potentiometerCurrentValue = analogRead(A0);
  if(abs(potentiometerCurrentValue - PotentiometerLastValue) > potentiometerChange) {
    digitalWrite(8, HIGH);
    PotentiometerLastValue = potentiometerCurrentValue; // setting the potentiometer last value to the current value
  } else {
    delay(t);
    digitalWrite(8, LOW); // turn off LED if no input was detected or change wasnt enough
  }

  delay(t);
  }
