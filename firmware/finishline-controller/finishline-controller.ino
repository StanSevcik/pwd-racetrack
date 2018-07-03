//
// Racetrack Firmware
//

#include "FastLED.h"
#include "PWDLaneDisplay.h"
#include "PWDData.h"
#include "PWDProtocol.h"

#define LED_PIN 13
#define FAST true
#define SLOW false

#define TM1637_DATA 24
#define TM1637_CLOCK 26
#define NEOPIXEL_PIN 28
#define DEMO_PIN 30
#define LASER_PIN 32

#define LDR_PIN A7
#define LDR_THRESHOLD 400

#define LANES 4

// needs to be global here, as it is shared across all lane objects
CRGB leds[LANES*7];

// instantiate the <n> lanes
PWDLaneDisplay lane[LANES] = {
  PWDLaneDisplay( 0, TM1637_CLOCK, TM1637_DATA, leds, CRGB( 255, 255, 255) ),  
  PWDLaneDisplay( 1, TM1637_CLOCK, TM1637_DATA, leds, CRGB(  30, 220, 255) ),  
  PWDLaneDisplay( 2, TM1637_CLOCK, TM1637_DATA, leds, CRGB( 255,  70,   0) ),  
  PWDLaneDisplay( 3, TM1637_CLOCK, TM1637_DATA, leds, CRGB( 180,  50, 100) ),  
};

// instantiate three serial communication channels
PWDProtocol combr( Serial );

// global data structures
PWDHeat heat;

// counter for loop stats
unsigned long c;
unsigned long last_elapsed;
unsigned long last_millis;

unsigned long elapsed;
unsigned long start;

int ldr;
int lane_status;
int finishers;
bool race_on;
bool update_rank;
// rank contains the lane numbers in the order they finish
int rank[LANES] = {0, 0, 0, 0};
// place contains the finishing place in lane order
int place[LANES] = {0, 0, 0, 0};


// we have to unselect all lanes first, otherwise two lanes might 
// temporarily end up both selected. Shouldn't be a big deal, but still...
// n == l equals false for all lanes other than the desired one
void select_lane( int l ) {
  for( int n=0; n<LANES; n++ ) {
    lane[n].select( n == l );
  }
  lane[l].select( true );
}


void blink( bool fast )
{
  for( int i=0; i<3; i++ ) {
    digitalWrite( LED_PIN, HIGH );
    delay( fast ? 250 : 750 );
    digitalWrite( LED_PIN, LOW );
    delay( fast ? 250 : 750 );
  }
}


void setup() {

  pinMode( LED_PIN, OUTPUT );
  blink( FAST );

  //Serial.begin( 57600 );
  //while ( ! Serial );
  //Serial.println( "Racetrack starting." );


// testing

  combr.begin();
  combr.sendAck( 13, 51 );

  combr.sendCarDetection( 0, "156F78DA2D6582" );
  combr.sendCarDetection( 1, "156F78DA2D6582" );
  combr.sendCarDetection( 2, "156F78DA2D6582" );
  combr.sendCarDetection( 3, "156F78DA2D6582" );

  const char* myrfid = "DEADFOODBADBAD";
  const char* myowner = "Michael Rommel";

  PWDLane myLane0;
  myLane0.rfid = myrfid;
  myLane0.owner = myowner;
  myLane0.matno = 1234567;
  myLane0.serno = 113;
  myLane0.time = 0;
  PWDLane myLane1;
  myLane1.rfid = myrfid;
  myLane1.owner = myowner;
  myLane1.matno = 12121212;
  myLane1.serno = 2222;
  myLane1.time = 0;
  PWDLane myLane2;
  myLane2.rfid = myrfid;
  myLane2.owner = myowner;
  myLane2.matno = 34343434;
  myLane2.serno = 3333;
  myLane2.time = 0;
  PWDLane myLane3;
  myLane3.rfid = myrfid;
  myLane3.owner = myowner;
  myLane3.matno = 56565656;
  myLane3.serno = 4444;
  myLane3.time = 0;

  heat.status = 0;
  heat.heatno = 15;
  heat.lanes[0] = &myLane0;
  heat.lanes[1] = &myLane1;
  heat.lanes[2] = &myLane2;
  heat.lanes[3] = &myLane3;

  combr.sendCarDetection( 3, &myLane0, true );
  combr.sendCarDetection( 0, &myLane0, false );

  combr.sendCompleteOrProgress( MSG_COMPLETE, &heat );

  heat.status = 2;
  heat.lanes[2]->time = 3501;

  combr.sendCompleteOrProgress( MSG_PROGRESS, &heat );

  heat.status = 3;
  heat.lanes[0]->time = 2001;
  heat.lanes[1]->time = 3001;
  heat.lanes[3]->time = 4001;

  combr.sendCompleteOrProgress( MSG_PROGRESS, &heat );

  blink( SLOW );
  delay( 5000 );

// testing

  // set all pin modes
  pinMode( DEMO_PIN, INPUT_PULLUP );
  pinMode( LASER_PIN, OUTPUT );
  pinMode( LDR_PIN, INPUT );
  pinMode( NEOPIXEL_PIN, OUTPUT );

  // initialize the LED library (strip type, pin number, variable,
  // number of LEDs
  FastLED.addLeds<NEOPIXEL, NEOPIXEL_PIN>(leds, LANES * 7);

  // clear displays
  for( int n=0; n<LANES; n++ ) {
    select_lane( n );
    lane[n].begin();
    lane[n].showNumber( n );
    lane[n].setBigDigit( PWDLaneDisplay::DIGIT_OFF );
  }
  FastLED.show();

  delay( 5000 );

  // start statistics
  last_millis = millis();
  
  start = millis();
  race_on = true;
  lane_status = 0;
  finishers = 0;
  digitalWrite( LASER_PIN, HIGH );

  Serial.println( "Entering loop." );

}

void loop() {
  elapsed = (millis() - start);
  // just for testing, skip numbers >10seconds
  // TODO: need to add support for decimal points to the library
  if ( elapsed > 9999 ) elapsed = elapsed % 10000;
  // loop counter
  c++;
  update_rank = false;


  // check serial only if the race is off, to save loop cycle time
  if( ! race_on ) {
    if( Serial.available() ) {
      switch( Serial.read() ) {
        case 'g':
          Serial.println( "Got g, starting race" );
          digitalWrite( LASER_PIN, HIGH );
          race_on = true;
          start = millis();
          lane_status = 0;
          finishers = 0;
          // blank the big displays
          for( int n=0; n<LANES; n++ ) {
            lane[n].setBigDigit( PWDLaneDisplay::DIGIT_OFF );
          }
          update_rank = true;
          break;
        // for testing get the lane that finishes next from serial
        // no checks if the lane already finished...
        case '0':
          Serial.println( "Got 0" );
          lane_status = lane_status | 1;
          finishers++;
          rank[finishers-1] = 0;
          place[0] = finishers;
          update_rank = true; 
          break;
        case '1':
          Serial.println( "Got 1" );
          lane_status = lane_status | 2;
          finishers++;
          rank[finishers-1] = 1;
          place[1] = finishers;
          update_rank = true; 
          break;
        case '2':
          Serial.println( "Got 2" );
          lane_status = lane_status | 4;
          finishers++;
          rank[finishers-1] = 2;
          place[2] = finishers;
          update_rank = true; 
          break;
        case '3':
          Serial.println( "Got 3" );
          lane_status = lane_status | 8;
          finishers++;
          rank[finishers-1] = 3;
          place[3] = finishers;
          update_rank = true; 
          break;
      }
    }

    Serial.println( "Reading demo pin" );
    int demo = digitalRead( DEMO_PIN );
    Serial.print( "Pin is: " );
    Serial.println( demo );
    if( ! demo && ! race_on ) {
      Serial.println( "Starting race." );
      digitalWrite( LASER_PIN, HIGH );
      race_on = true;
      start = millis();
      lane_status = 0;
      finishers = 0;
      // blank the big displays
      for( int n=0; n<LANES; n++ ) {
        lane[n].setBigDigit( PWDLaneDisplay::DIGIT_OFF );
      }
      update_rank = true;
    }
  }
  
  // display times on all counters, which have not finished
  for( int n=0; n<4; n++) {
    if( race_on ) {
      // if the lane has not finished or is the one that finished in this 
      // loop iteration, then update the millis display
      if( ( ! (lane_status & (1 << n)) ) || ( (n==rank[finishers-1]) && update_rank ) ) {
        // select lane chip
        select_lane( n );
        lane[n].showNumber( elapsed );
        // normally we would do that also continuously, so simulate this here
        ldr = analogRead( A7 );
        //Serial.print( "LDR: " );
        //Serial.println( ldr );
        if( ldr > LDR_THRESHOLD ) {
          // detected laser beam break
          // TODO: add array for multiple consecutive breaks from original sketch.
          //       may actually not be needed as the loop is around 18ms
          // set bit for this lane
          lane_status = lane_status | (1 << n);
          // increase number of finishers (1-4)
          finishers++;
          // remember the lane for the n-th finisher
          rank[finishers-1] = n;
          // remember the place for this lane
          place[n] = finishers;
          update_rank = true; 
        }
      }
      
      // if we have to update the rank display and this is the lane we have to update
      if( ( n==rank[finishers-1]) && update_rank ) {
        //Serial.print( "update milli displays for lane " );
        //Serial.println( n );
        lane[n].setBigDigit( place[n] );
      }
      
    } else {
      
      // test for setup
      Serial.println( "Race is off." );
      // select lane chip
      select_lane( n );
      ldr = analogRead( A7 ) / 10;
      lane[n].showNumber( ldr );
      delay(50);

    }

  }

  // unselect last lane to achieve a stable state for the LDR line
  lane[3].select( false );
  
  // only push updates to the LEDs, if necessary, as it blocks serial interrupts
  if( update_rank ) {
    FastLED.show();
    update_rank = false;
  }

  // every now and then print statistics
  if( (millis() - last_millis) > 8000 ) {
    // get counter diff divided by elapsed millis
    unsigned long millis_per_loop = ( millis() - last_millis ) / c ;
    last_millis = millis();
    c = 0;
    Serial.print( "Milliseconds per loop iteration: " );
    Serial.println( millis_per_loop );
  }

  if( finishers == LANES ) {
    // this enables reading serial again, to re-start a race
    race_on = false;
    // no need to be so responsive now
    delay( 200 );
  }

}

// vim:si:sw=2
