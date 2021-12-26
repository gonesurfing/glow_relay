#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

// array of glow times for each 8bit ADC value
const uint8_t glow_data[256] = {30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,29,27,26,25,23,22,21,20,20,19,18,17,17,16,16,15,15,14,14,13,13,12,12,12,11,11,11,11,10,10,10,10,9,9,9,9,9,8,8,8,8,8,8,8,7,7,7,7,7,7,7,7,6,6,6,6,6,6,6,6,6,6,6,6,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,0,0,0,0,0,0,0,0,0};
const uint32_t safety_delay = 30 * 1000;
uint8_t crank = 0;
volatile uint32_t timer0_millis;

ISR(PCINT0_vect) {
  crank = 1;
}

ISR(TIMER0_COMPA_vect) {
  timer0_millis++;
}

void init_ADC()
{
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

void init_millis() {
  TCCR0A = (1<<WGM01); // CTC mode
  TCCR0B |= (1<<CS01); //clock/8 scaler
  OCR0A = 131;  //1000000/(1*8*1+OCR0A) adjusted for clock error

  // Enable the compare match interrupt
  TIMSK |= (1 << OCIE0A);
}

void init_crank_interrupt() {
  PCMSK |= (1 << PCINT1); //PB6
  GIMSK |= (1<<PCIE); //enable pin change interrupts
}

unsigned long millis(void) {
  uint32_t millis_return;
  cli();
  millis_return = timer0_millis;
  sei();
  return millis_return;
}

int main() {
  DDRB |= (1<<DDB4) | (1<<PB0); //PB4 and PB0 as output
  
  cli();
  init_ADC();
  init_crank_interrupt();
  init_millis();
  sei();

  _delay_ms(100); //let voltage settle out.

  uint8_t count = 0;
  uint16_t sum = 0;

  while(count<10) {
    ADCSRA |= (1 << ADSC);         // start ADC measurement
    while (ADCSRA & (1 << ADSC) ); // wait till conversion complete 
    sum += ADCH;
    count++;
    _delay_ms(1);
  }
  uint8_t adc_val = sum/10;
  uint32_t glow_time = glow_data[adc_val] * 1000;
  uint32_t safety_time = glow_time + safety_delay;
  uint32_t last_ms = 0;

  if (glow_time > 0 ) {
    PORTB |= (1<<PB4);
    while(millis() < safety_time) {

      if ((millis() > glow_time) | crank) {
        break;
      }
      //heartbeat
      if (millis() - last_ms > 500) {
        PORTB ^= _BV(PB0);
        last_ms = millis();
      }
    }
  }
  //power down mode until power is cycled again
  PORTB = 0; //turn off outputs
  MCUCR |= (1<<SM1);
}


