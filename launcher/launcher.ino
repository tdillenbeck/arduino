#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"
//#include <Thread.h>

//
// Hardware configuration
//

// Set up nRF24L01 radio on SPI bus plus pins 9 & 10

RF24 radio(9,10);

// sets the role of this unit in hardware.  Connect to GND to be the 'led' board receiver
// Leave open to be the 'remote' transmitter
const int role_pin = A4;

// Pins on the remote for buttons
const uint8_t button_pins[] = { 2,7,8,5 };
const uint8_t num_button_pins = sizeof(button_pins);

// Pins on the LED board for LED's
const uint8_t led_pins[] = { 2,3,4,5 };
const uint8_t num_led_pins = sizeof(led_pins);

const uint8_t fire_time = 200; //ms
const int sleep_time = 5000; //ms

//Thread[] launchThreads;
//launchThreads[0] = Thread();
//launchThreads[1] = Thread();
//launchThreads[2] = Thread();
//launchThreads[3] = Thread();

//
// Topology
//

// Single radio pipe address for the 2 nodes to communicate.
const uint64_t pipe = 0xE8E8F0F0E1LL;

//
// Role management
//
// Set up role.  This sketch uses the same software for all the nodes in this
// system.  Doing so greatly simplifies testing.  The hardware itself specifies
// which node it is.
//
// This is done through the role_pin
//

// The various roles supported by this sketch
typedef enum { role_remote = 1, role_led } role_e;

typedef enum { off = 1, on, sleeping } state;

// The debug-friendly names of those roles
const char* role_friendly_name[] = { "invalid", "Remote", "LED Board"};

// The role of the current running sketch
role_e role;

//
// Payload
//

uint8_t button_states[num_button_pins];
uint8_t button_cooldown[num_button_pins];
state led_states[num_button_pins];
unsigned long sleep_times[num_button_pins];

//// callback for myThread
//void niceCallback(){
//  static bool ledStatus = false;
//  ledStatus = !ledStatus;
//
//  digitalWrite(ledPin, ledStatus);
//
//  Serial.print("COOL! I'm running on: ");
//  Serial.println(millis());
//}

//
// Setup
//

void setup(void)
{
  //
  // Role
  //

  // set up the role pin
  pinMode(role_pin, INPUT);
  digitalWrite(role_pin,HIGH);
  delay(20); // Just to get a solid reading on the role pin

  // read the address pin, establish our role
  if ( digitalRead(role_pin) )
    role = role_remote;
  else
    role = role_led;

  //
  // Print preamble
  //

  Serial.begin(57600);
  printf_begin();
  printf("\n\rRF24/examples/led_remote/\n\r");
  printf("ROLE: %s\n\r",role_friendly_name[role]);

  //
  // Setup and configure rf radio
  //

  radio.begin();

  //
  // Open pipes to other nodes for communication
  //

  // This simple sketch opens a single pipes for these two nodes to communicate
  // back and forth.  One listens on it, the other talks to it.

  if ( role == role_remote )
  {
    radio.openWritingPipe(pipe);
  }
  else
  {
    radio.openReadingPipe(1,pipe);
  }

  //
  // Start listening
  //

  if ( role == role_led )
    radio.startListening();

//    launchThreads[0].onRun(niceCallback);
//    launchThreads[1].onRun(niceCallback);
//    launchThreads[2].onRun(niceCallback);
//    launchThreads[3].onRun(niceCallback);


  //
  // Dump the configuration of the rf unit for debugging
  //

  radio.printDetails();

  //
  // Set up buttons / LED's
  //

  // Set pull-up resistors for all buttons
  if ( role == role_remote )
  {
    int i = num_button_pins;
    while(i--)
    {
      pinMode(button_pins[i],INPUT);
      digitalWrite(button_pins[i],HIGH);
    }
  }

  // Turn LED's OFF until we start getting keys
  if ( role == role_led )
  {
    int i = num_led_pins;
    while(i--)
    {
      pinMode(led_pins[i],OUTPUT);
      led_states[i] = off;
      digitalWrite(led_pins[i],led_states[i]);
      sleep_times[i] = millis();
    }
  }

}

//
// Loop
//

void loop(void)
{
  //
  // Remote role.  If the state of any button has changed, send the whole state of
  // all buttons.
  //

  if ( role == role_remote )
  {
    // Get the current state of buttons, and
    // Test if the current state is different from the last state we sent
    int i = num_button_pins;
    bool different = false;

    //init current states to off
    uint8_t current_button_states[num_button_pins];

    while(i--)
    {
      uint8_t state = ! digitalRead(button_pins[i]);
      if ( state != button_states[i] )
      {
//         if (sleep_times[i] > millis() + sleep_time) {
//           char buffer[3];
//           char* num = itoa(i,buffer,10);
//           printf("We've cooled off launcher ");
//           printf(num);
//           printf("\n\r");
//
//             led_states[i] = off;
//          }

        different = true;
        current_button_states[i] = state;
      }
    }

    // Send the state of the buttons to the LED board
    if ( different )
    {
      printf("Now sending...");
      bool ok = radio.write( button_states, num_button_pins );
      if (ok)
        printf("ok\n\r");
      else
        printf("failed\n\r");
    }

    // Try again in a short while
    delay(20);
  }

  //
  // LED role.  Receive the state of all buttons, and reflect that in the LEDs
  //

  if ( role == role_led )
  {
    // if there is data ready
    if ( radio.available() )
    {
      // Dump the payloads until we've gotten everything
      bool done = false;

      uint8_t old_button_states[num_button_pins];
      memcpy( old_button_states, button_states, num_button_pins );

      while (!done)
      {
        // Fetch the payload, and see if this was the last one.
        radio.read( button_states, num_button_pins );
        if (num_button_pins > 0)
        {
          done = true;
        }
        // Spew it
        printf("Got buttons\n\r");

        // For each button, if the button now on, then toggle the LED
        int i = num_led_pins;
        while(i--)
        {
          /*if (sleep_times[i] > millis() + sleep_time) {
             char buffer[3];
             char* num = itoa(i,buffer,10);
             printf("We've cooled off launcher ");
             printf(num);
             printf("\n\r");

             led_states[i] = off;
          }*/
          if ( button_states[i] != old_button_states[i] & led_states[i] == off)
          {
            fire(led_pins[i], i);
          }
        }
      }
    }
  }
}

void fire(int pin, int i) {
  char buffer[3];
  char* num = itoa(i,buffer,10);
  printf("Firing launcher:  ");
  printf(num);
  printf("\n\r");
  if (led_states[i] = on)
    return;
  digitalWrite(pin,LOW);
  led_states[i] = on;
  delay(fire_time);
  digitalWrite(pin,HIGH);
  delay(sleep_time);
  led_states[i] = off;
  sleep_times[i] = millis();
}

