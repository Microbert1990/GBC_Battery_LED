/*
 * GBC_Battery_LED.c
 *
 * Created: 14.07.2024 17:14:21
 * Author : Microbert
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>
#define F_CPU 1000000UL

/* GPIO pins */
#define RED_LED		PINB3 
#define GREEN_LED	PINB4

/* Voltage thresholds */
/*
	Battery level thresholds
		falling			rising
		=======	    	======
 green ----|///|		|///|--- green
	    \  |///|		|///| /
		 \ |///|		|///|/
	      \|///|--3.3---|###|<-- yellow High
 ye Low--->|###|--3.1---|###|
	       |###|		|###|
		   |###|--3.0---|\\\|<-- red High
 red Low-->|\\\|--2.8---|\\\|
		   |\\\|		|\\\|
 */
#define THRES_RED_LOW	2.8f
#define THRES_RED_HIGH	3.0f
#define THRES_YELLOW_LOW	3.1f
#define THRES_YELLOW_HIGH	3.3f
#define THRES_GREEN_LOW	3.4f

/* Battery Charge level */
#define MIN_BAT 0
#define MED_BAT 1
#define MAX_BAT 2

/* Main state machine states */
#define INIT 0
#define MEASURE 1
#define WAITING 2
#define CHANGING 3

/* Prototypes */
void Init_GPIO();
void Init_Timer();
void Init_ADC();
uint8_t volt_comp(double);
void setLED(uint8_t);
double getVoltage();

/* Variables */

uint16_t adc_val = 0;			// adc value variable
volatile double voltage = 0.0;	// measured voltage
volatile uint16_t timeout = 0;	// debounce time variable
volatile uint8_t batState = MAX_BAT, batStateLast = MAX_BAT;	// battery states

volatile uint8_t state = INIT;	// main state machine state
	
int main(void)
{
	cli();	// disable global interrupts
	Init_GPIO();
	Init_Timer();
	Init_ADC();
	
	sei();	// enable global interrupts
	
    while (1) 
    {
		switch(state)
		{
			case INIT:	/* Initial Measurement to set correct LED status*/
				while(voltage > 4.2f || voltage < 2.3f)	// first few adc measurements are much higher and can be ignored
				{
					voltage = getVoltage();
					batStateLast = batState;
					batState = volt_comp(voltage);
				}
				setLED(batState);
				timeout = 0x10;
				state = MEASURE;
				
			case MEASURE:	/* Measure current battery voltage */
			voltage = getVoltage();
				batState = volt_comp(voltage);
				if(batState != batStateLast)	// if voltage is in new range
				{
					batStateLast = batState;
					timeout = 0x2f0;
					state = WAITING;			// debouncing 
				}
				else
					state = CHANGING;			// when state still in the new voltage range - change LED state
				break;
			case WAITING:	/* debouncing */
				if(timeout == 0)				// after debounce time, measure again
				{
					state = MEASURE;
				}
				break;
				
			case CHANGING:	/* Set new LED state */
				setLED(batState);
				state = MEASURE;
				break;
			default: 
				break;
		}
	}
}

/************************************************************************
 * \details: Gets the current measured ADC value and converts it into 
 * the corresponding voltage level
 * \param: double - battery voltage [V]
 ************************************************************************/
double getVoltage()
{
	adc_val = (ADCH << 8) | ADCL; // read val
	if(adc_val > 0)
	return (1.13f * 1024) / adc_val;

}

/************************************************************************
 * \details: Set status LED to corresponding battery voltage state
 * \param: uint8_t batteryState	- current battery state
 ************************************************************************/
void setLED(uint8_t batteryState)
{
	switch(batteryState)
	{
		case MAX_BAT: PORTB = 0x08; break;	//green
		case MED_BAT: PORTB = 0x00; break;	//yellow
		case MIN_BAT: PORTB = 0x10; break;	//red
		default: break;
	}	
}


/************************************************************************
 * \details: Dependend of which level you're came from, the current battery
 * voltage will be compared against certain threshold ranges (wether falling
 * or rising voltage level), and returns the corresponding battery state
 * \param: double val	- current measured battery voltage [V]
 * \return: uint8_t ret - corresponding battery state  
 ************************************************************************/
uint8_t volt_comp(double val)
{
	uint8_t ret = MIN_BAT;
	switch(batState)
	{
		case MAX_BAT:
		{	
			/* Voltage dropping */
			if((val < THRES_YELLOW_LOW) && (val > THRES_RED_LOW))
				ret = MED_BAT;
			/* Voltage dropping even further */	
			else if(val <= THRES_RED_LOW)
				ret = MIN_BAT;
			else
				ret = MAX_BAT;
			break;
		}
		case MED_BAT:
		{	
			/* Voltage dropping */
			if(val <= THRES_RED_LOW)
				ret = MIN_BAT;
			/* voltage rising */
			else if(val >= THRES_YELLOW_HIGH)
				ret = MAX_BAT;
			else
				ret = MED_BAT;
				
			break;
		}
		case MIN_BAT:
		{
			/* voltage rising */
			if((val >= THRES_RED_HIGH) && (val < THRES_YELLOW_HIGH))
				ret = MED_BAT;
			else if(val >= THRES_YELLOW_HIGH)
				ret = MAX_BAT;
			else
				ret = MIN_BAT;
			break;
		}
		default:
		{
			ret = batState;
			break;
		}
	}
	return ret;
}

/************************************************************************
 * \details: Initializes GPIO pins                                                                   
 ************************************************************************/
void Init_GPIO()
{
	DDRB |= (1<<RED_LED) | (1<<GREEN_LED);
	PORTB |= (1<<RED_LED) | (1<<GREEN_LED);
}

/************************************************************************
 * \details: Initializes ADC                                                                   
 ************************************************************************/
void Init_ADC()
{

 ADCSRA|=(1<<ADEN);       //Enable ADC module
 ADMUX |=(1<<MUX3)|(1 << MUX2); // ADC1 (PA1) channel is selected (internal power supply on VCC pin with internal reference voltage)
 ADCSRB|=(1<<ADTS2);   //Timer / Counter 0 overflow triggers the ADC to perform conversion
 ADCSRA|=(1<<ADSC)|(1<<ADATE); //Enabling start of conversion and Auto trigger
}

/************************************************************************
 * \details: Initializes Timer Interrupt                                                                   
 ************************************************************************/
void Init_Timer()
{
 TCCR0A=0x00;            //Timer0 normal mode
 TCCR0B=0x00;
 TCCR0B |= (1<<CS01);   //no prescaling
 TCNT0=0;
 TIMSK |= (1 << TOIE0);
}

/************************************************************************
 * \details: Timer Interrupt Service Routine - decreases debounce time
 * and clearing Overflow flag
 ************************************************************************/
ISR(TIM0_OVF_vect)
{
	if(timeout > 0)
		timeout--;
	TIFR|=(1<<TOV0);  //Clearing overflow flag
}