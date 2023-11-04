#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

// Array of glow times (seconds) mapped for each 8bit ADC value
const uint8_t glow_data[256] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,6,6,6,6,6,6,6,6,6,6,6,6,7,7,7,7,7,7,7,7,8,8,8,8,8,8,8,9,9,9,9,9,10,10,10,10,11,11,11,11,12,12,12,13,13,14,14,15,15,16,16,17,17,18,19,20,20,21,22,23,25,26,27,29,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30};

// Below two are volatile since they can be modified in an ISR
volatile uint8_t crank = 0;
volatile uint32_t timer0_millis;

// Define an ISR for the crank signal
ISR( PCINT0_vect ) {
  // Set the crank flag to 1
  crank = 1;
}

ISR( TIMER0_COMPA_vect ) {
  // Increment our timer every time this ISR runs.
  timer0_millis++;
}

void init_ADC()
{
  // Below is modified from this extremely helpful page on ADC:
  // https://www.marcelpost.com/wiki/index.php/ATtiny85_ADC

  /* this function initialises the ADC 

        ADC Prescaler Notes:
	--------------------

	   ADC Prescaler needs to be set so that the ADC input frequency is between 50 - 200kHz.
  
           For more information, see table 17.5 "ADC Prescaler Selections" in 
           chapter 17.13.2 "ADCSRA – ADC Control and Status Register A"
          (pages 140 and 141 on the complete ATtiny25/45/85 datasheet, Rev. 2586M–AVR–07/10)

           Valid prescaler values for various clock speeds
	
	     Clock   Available prescaler values
           ---------------------------------------
             1 MHz   8 (125kHz), 16 (62.5kHz)
             4 MHz   32 (125kHz), 64 (62.5kHz)
             8 MHz   64 (125kHz), 128 (62.5kHz)
            16 MHz   128 (125kHz)

           Below example set prescaler to 128 for mcu running at 8MHz
           (check the datasheet for the proper bit values to set the prescaler)
  */

  // 8-bit resolution
  // set ADLAR to 1 to enable the Left-shift result (only bits ADC9..ADC2 are available)
  // then, only reading ADCH is sufficient for 8-bit results (256 values)

  ADMUX =
            (1 << ADLAR) |     // left shift result
            (0 << REFS1) |     // Sets ref. voltage to VCC, bit 1
            (0 << REFS0) |     // Sets ref. voltage to VCC, bit 0
            (0 << MUX3)  |     // use ADC3 for input (PB3), MUX bit 3
            (0 << MUX2)  |     // use ADC3 for input (PB3), MUX bit 2
            (1 << MUX1)  |     // use ADC3 for input (PB3), MUX bit 1
            (1 << MUX0);       // use ADC3 for input (PB3), MUX bit 0

  ADCSRA = 
            (1 << ADEN)  |     // Enable ADC 
            (1 << ADPS2) |     // set prescaler to 64, bit 2 
            (1 << ADPS1) |     // set prescaler to 64, bit 1 
            (0 << ADPS0);      // set prescaler to 64, bit 0  

  DIDR0 |= (1 << ADC2D); //disable di on ADC pin
}

// Rather than import the whole arduino framework for one timer, we'll just setup a simple millis function.
// Since we're using the internal oscillator, we need to account for the clock error.
// This is done by adjusting the OCR0A value.
void init_millis() {
  TCCR0A = (1<<WGM01); // CTC mode
  TCCR0B |= (1<<CS01); //clock/8 scaler
  OCR0A = 131;  //1000000/(1*8*1+OCR0A) adjusted for clock error

  // Enable the compare match interrupt
  TIMSK |= (1 << OCIE0A);
}

// Setup an interrupt for the crank signal
void init_crank_interrupt() {
  PCMSK |= (1 << PCINT1); //PB6
  GIMSK |= (1<<PCIE); //enable pin change interrupts
}

// This is the millis function. Rather than read timer0_millis directly, we'll disable interrupts
// while we read it. This is because timer0_millis is volatile and we don't want to read it
// while it's being updated.
unsigned long millis(void) {
  uint32_t millis_return;
  cli();
  millis_return = timer0_millis;
  sei();
  return millis_return;
}

int main() {
  DDRB |= (1<<DDB4) | (1<<PB0); // Set PB4 and PB0 as outputs
  
  // Disable interrupts while we initialize everything.
  cli();
  init_ADC();
  init_crank_interrupt();
  init_millis();
  sei();

  _delay_ms(100); //let voltage settle out.

  uint8_t count = 0;
  uint16_t sum = 0;

  // Take 10 ADC readings and sum them.
  while( count < 10 ) {
    ADCSRA |= (1 << ADSC);         // start ADC measurement
    while ( ADCSRA & (1 << ADSC) ); // wait till conversion complete 
    sum += ADCH;
    count++;
    _delay_ms(1);
  }

  // Average the 10 readings and use that to index into the glow_data array.
  uint8_t adc_val = sum / 10;
  uint32_t glow_time = glow_data[adc_val] * 1000; // Convert to ms
  uint32_t last_ms = 0;

  if ( glow_time > 0 ) {
    // Turn on the glow plug output for the calculated time.
    PORTB |= (1<<PB4);
    while( millis() < glow_time ) {

      // If the crank signal is received, turn off the glow plug and exit.
      if ( crank ) {
        break;
      }
      // Heartbeat LED on/off every 1s.
      // Handy for checking timer accuracy.
      if (millis() - last_ms > 500) {
        PORTB ^= _BV(PB0);
        last_ms = millis();
      }
    }
  }

  // Idle until power is cycled again
  PORTB = 0; // Set all outputs low.
  MCUCR |= (1<<SM1); // Sleep forever
}


