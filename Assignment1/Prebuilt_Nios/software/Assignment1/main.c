#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
// For specifying int size
#include <stdint.h>
//Timer ISR
#include "sys/alt_alarm.h"
//USED FOR LEDS
#include <system.h>
//THESE are for button isr
#include <altera_avalon_pio_regs.h> //to use PIO functions
#include <alt_types.h>   // alt_32 is a kind of alt types
#include <sys/alt_irq.h> // to register interrupts

//Clear array contents
#define CLEAR(array) memset(&(array), 0, sizeof(array))

// Traffic light states for N/S and E/W
#define Red_Red	    0
#define Green_Red   1
#define Yellow_Red  2
#define Red_Red_2   3
#define Red_Green   4
#define Red_Yellow  5

// Traffic light delays
#define RedRed_delay 500
#define RedGreen_delay 6000
#define RedYellow_delay 2000


//Define LEDS
#define NS_G 0b1
#define NS_Y 0b10
#define NS_R 0b100
#define EW_G 0b1000
#define EW_Y 0b10000
#define EW_R 0b100000
#define pedNSgo 0b01000000
#define pedEWgo 0b10000000

//Mode 2 states
#define Green_RedP1 6
#define Red_GreenP2 7

//Pedestrian states
#define NS_Idle 0
#define EW_Idle 0
#define NS_pressed 1
#define EW_pressed 1
//Modes
#define Mode1 1
#define Mode2 2
#define Mode3 3
#define Mode4 4
//Switches
#define Switch0 0x01
#define Switch1 0x02
#define Switch2 0x04
//Printing LCD
#define ESC 27
#define CLEAR_LCD_STRING "[2J"

//Globals
//create mode_changing option
volatile uint8_t mode=Mode1;
volatile uint8_t currentState = Red_Red;
volatile uint16_t delay_nextState = RedRed_delay;

//Mode2 pedestrian conditions
volatile bool pedNS, pedEW, EWhandled, NShandled, NSraised, EWraised = 0;
volatile uint8_t stateNSped, stateEWped = NS_Idle;
//UART file recording user Inputs
FILE *fp;

//INIT MODE 3 CHAR
volatile char digits[50];
volatile uint16_t gt1 = 500;
volatile uint16_t gt2 = 6000;
volatile uint16_t gt3 = 2000;
volatile uint16_t gt4 = 500;
volatile uint16_t gt5 = 6000;
volatile uint16_t gt6 = 2000;
//Using flag in ISR
volatile bool timer_Flag=false;
//Use flag for UART
bool user_input_received =false;

//Mode 4
#define cameraIdle 0
#define cameraOdd  1
#define cameraEven 2
#define Camera_timeout 2000
int Camera_speed = 100;
// Overflow counter
volatile int vehicleDuration = 0;
// Inputs for FSM
volatile bool isVehiclePresent = false;
// Current state for camera
volatile uint8_t cameraState = cameraIdle;
// Using second timer
volatile int timeCountMain2 = 0;
// Context for camera_timer
void* timerContext2 = (void*) &Camera_speed;
//Flags for FSM
volatile bool snapshotTaken, takeSnapshot = false;
volatile bool enteredJunction = false;

//Main timer
alt_alarm timer;
//Camera timer
alt_alarm timer2;
//LCD
FILE *lcd;
///////////////////////////////////////////
           //ISR Routines\\
//////////////////////////////////////////
alt_u32 camera_timer_isr(void* context2){
	//@detail Counts duration a vehicle stays in intersection
	vehicleDuration++;
	if(takeSnapshot){
		if(vehicleDuration >= (Camera_timeout/Camera_speed)) {
			sendUartMessage("Snapshot Taken");
			takeSnapshot = false;

		}
	}
	return Camera_speed;
}

uint16_t tlc_timer_isr(void* context){
	//set the flag=true and return 0 to stop timer
	timer_Flag=true;
	return 0;
}

void Buttons_isr(void* context, alt_u32 id){
	int* temp = (int*) context; // need to cast the context first before using it
	(*temp) = IORD_ALTERA_AVALON_PIO_EDGE_CAP(BUTTONS_BASE);
	// clear the edge capture register
	IOWR_ALTERA_AVALON_PIO_EDGE_CAP(BUTTONS_BASE, 0);
	printf("button: %i\n", *temp);

	if(*temp == 1){
		NSraised = 1;

	}else if(*temp == 2){
		EWraised = 1;

	}else if(*temp == 4){

		isVehiclePresent = true;

	}

}

//////////////////////////////////////////
      //Initialize on Startup\\
/////////////////////////////////////////
void uart_init(){

	fp = fopen(UART_NAME, "r+");
	if(!fp){
		printf("Failed to open UART");
	}
}

void init_buttons_pio(){
  //@detail Initialises buttons and registers for ISR
  //	    Buttons are for pedestrians

  uint8_t buttonValue = 1;
  void* context_going_to_be_passed = (void*) &buttonValue; // cast before passing to ISR
  // clears the edge capture register
  IOWR_ALTERA_AVALON_PIO_EDGE_CAP(BUTTONS_BASE, 0);
  // enable interrupts for all buttons
  IOWR_ALTERA_AVALON_PIO_IRQ_MASK(BUTTONS_BASE, 0x7);
  // register the ISR
  alt_irq_register(BUTTONS_IRQ,context_going_to_be_passed, Buttons_isr);

}

void clearLeds(){
	IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0x00);
}

int main()
{
  printf("Hello from Nios II!\n");
  //@note Timer code does not work if put in a function and called
  //      Has to be in main

  uint8_t timeCountMain = 0;
  // start the timer, with timeout of Red-Red state
  void* timerContext = (void*) &timeCountMain;
  alt_alarm_start(&timer, delay_nextState, tlc_timer_isr, timerContext);

  //Clear Leds
  clearLeds();

  //button interrupt code
  init_buttons_pio();
  //UART
  uart_init();

  //Clear global arrays
  CLEAR(digits);
  //LCD init and show mode

  lcd = fopen(LCD_NAME, "w");
  fprintf(lcd, "%c%s", ESC, CLEAR_LCD_STRING);
  fprintf(lcd, "Mode: %d\n", mode);
  fclose(lcd);
  while(1){
	  if(timer_Flag){
		  Run();
	  }

	  if (mode==Mode4){
		  handle_vehicle_button();
	  }
  }

  return 0;

}

//Runs TLC based on mode
void Run(){
	//We account for mode changing
	if( currentState==Red_Red || currentState==Red_Red_2 ){
		checkSwitch();
		lcd_sel_mode();
	}

	switch(mode){
	  case Mode1:
		  simple_tlc();
		  break;
	  case Mode2:
		  pedestrian_tlc();
		  break;
	  case Mode3:
		  mode3_tlc();
		  uint8_t S2=(IORD_ALTERA_AVALON_PIO_DATA(SWITCHES_BASE)&Switch2)&Switch2;

		  //Resetting for configuring new delays next time
		  if(!S2) user_input_received = false;

		  if(currentState==Red_Red||currentState==Red_Red_2){
			  while(user_input_received==false && S2 && mode==Mode3){
			  			  if(getUartMessage()) parseDigits();
			  }
		  }
		  break;
	  case Mode4:
		  camera_tlc();
		  uint8_t S2_2=(IORD_ALTERA_AVALON_PIO_DATA(SWITCHES_BASE)&Switch2)&Switch2;

		  //Resetting for configuring new delays next time
		  if(!S2_2) user_input_received = false;

		  if(currentState==Red_Red||currentState==Red_Red_2){
			  while(user_input_received==false && S2_2 && mode==Mode4){
						  if(getUartMessage()) parseDigits();
			  }
		  }
		  break;
	}


	//need to restart timer and reset flag
	void* timerContext = (void*) &delay_nextState;
	alt_alarm_start(&timer, delay_nextState, tlc_timer_isr, timerContext);
	// and at the end reset the flag
	timer_Flag=false;
}

//////////////////////////////////////////////
        //TLCs Based on Mode\\
/////////////////////////////////////////////


void simple_tlc(){
	//@note - Sets led for state, delay for the next state and increments state after that
	printf("Current state is : %d \n", currentState);
	led_outputs();
	setStateDelay();
	updateState();
}

void pedestrian_tlc(){
	printf("Current state is : %d \n", currentState);
	led_outputs_mode2();
	setStateDelay_mode2();
	FSM_NSped();
	FSM_EWped();
	FSM_mode2();

}

void camera_tlc(){
	printf("Current state is %d\n", currentState);
	//led_outputs();
	setStateDelay_mode3();
	FSM_NSped();
	FSM_EWped();
	FSM_mode2();
	led_outputs_mode2();

}

void mode3_tlc(){
	//@detail 3 concurrent FSMS.
	printf("Current state is : %d \n", currentState);

	setStateDelay_mode3();
	FSM_NSped();
	FSM_EWped();
	FSM_mode2();
	led_outputs_mode2();


}

///////////////////////////////////////////////
        //FSM's for all Modes\\
///////////////////////////////////////////////

//Mode 1
void updateState(){
	if (currentState < Red_Yellow) {
		currentState++;
	}else{
		currentState = Red_Red;
	}
}
//Mode 2 - 3
void FSM_mode2(){
	switch(currentState){
	case Red_Red:

		if(pedNS){
			currentState = Green_RedP1;
		}else{
			currentState = Green_Red;
		}
		break;

	case Green_Red:

		currentState = Yellow_Red;
		break;

	case Green_RedP1:

		NShandled = 1;
		currentState = Yellow_Red;
		break;
	case Yellow_Red:

		NShandled = 0;
		currentState = Red_Red_2;
		break;

	case Red_Red_2:

		//NShandled = 0;
		if(pedEW){
			currentState = Red_GreenP2;
		}else{
			currentState = Red_Green;
		}
		break;

	case Red_Green:

		currentState = Red_Yellow;
		break;
	case Red_GreenP2:

		EWhandled = 1;
		currentState = Red_Yellow;
		break;
	case Red_Yellow:

		EWhandled = 0;
		currentState = Red_Red;
		break;
	}




}

void FSM_NSped(){

	switch(stateNSped){
			case NS_Idle:
				if(NSraised){
					NSraised = 0;
					pedNS = 1;
					stateNSped = NS_pressed;
				}else{
					stateNSped = NS_Idle;
				}
				break;

			case NS_pressed:
				if(NShandled){
					pedNS = 0;
					stateNSped = NS_Idle;
				}else{
					stateNSped = NS_pressed;
				}
				break;

		}
}

void FSM_EWped(){

	switch(stateEWped){
			case EW_Idle:
				if(EWraised){
					EWraised = 0;
					pedEW = 1;
					stateEWped = EW_pressed;
				}else{
					stateEWped = EW_Idle;
				}
				break;

			case EW_pressed:
				if(EWhandled){
					pedEW = 0;
					stateEWped = EW_Idle;
				}else{
					stateEWped = EW_pressed;
				}
				break;
	}
}

//Mode 4
void FSM_mode4(){
	//@detail Same as mode 1. Takes photo immediately at red light if vehicle is detected

	switch(currentState){
	case Red_Red:
		currentState = Green_Red;
		break;

	case Green_Red:
		currentState = Yellow_Red;
		break;

	case Yellow_Red:

		currentState = Red_Red_2;
		break;

	case Red_Red_2:
		currentState = Red_Green;
		break;

	case Red_Green:
		currentState = Red_Yellow;
		break;

	case Red_Yellow:

		currentState = Red_Red;
		break;
	}

}

void handle_vehicle_button(){
	//@detail cameraOdd state denotes that a vehicle has crossed the intersection
	//		  cameraEven state denotes that a vehicle has left the intersection
		switch(cameraState){

			case cameraIdle:
				enteredJunction = false;
				vehicleDuration = 0;

				if((!enteredJunction)&&(isVehiclePresent)){
					if(( currentState == Red_Green  || currentState == Green_Red))
					{
						alt_alarm_start(&timer2, Camera_speed, camera_timer_isr, timerContext2);
						sendUartMessage("Vehicle enters on Green Light");


					}else if((currentState == Red_Red) || (currentState == Red_Red_2)){
						alt_alarm_start(&timer2, Camera_speed, camera_timer_isr, timerContext2);
						sendUartMessage("Camera activated");
						sendUartMessage("Snapshot Taken");

					}else if((currentState == Red_Yellow) || (currentState == Yellow_Red)){
						alt_alarm_start(&timer2, Camera_speed, camera_timer_isr, timerContext2);
						sendUartMessage("Camera activated");
						takeSnapshot = true;

					}
					enteredJunction=true;
					cameraState = cameraOdd;
				}

				break;

			case cameraOdd:

				if((enteredJunction)&&(isVehiclePresent)){
					alt_alarm_stop(&timer2);
					cameraState = cameraEven;
				}

				break;

			case cameraEven:
				vehicleDuration *= 100;
				sendUartMessage("Vehicle left");
				char buffer[20];
				sendUartMessage(itoa(vehicleDuration, buffer, 10));
				cameraState = cameraIdle;
				break;
		}
		//Reset button Press
		isVehiclePresent = false;
}
/////////////////////////////////////////////////
               //State Delays\\
/////////////////////////////////////////////////

void setStateDelay(){
	// Sets delay according to state

	if( (currentState == Red_Red)||(currentState == Red_Red_2) ){
		delay_nextState = RedRed_delay;

	}else if( (currentState == Green_Red)||(currentState == Red_Green) ){
		delay_nextState = RedGreen_delay;

	}else if( (currentState == Yellow_Red) || (currentState == Red_Yellow) ){
		delay_nextState = RedYellow_delay;
	}

}

void setStateDelay_mode2(){
	if( (currentState == Red_Red)||(currentState == Red_Red_2) ){
			delay_nextState = RedRed_delay;

	}else if( (currentState == Green_Red)||(currentState == Red_Green) ){
		delay_nextState = RedGreen_delay;

	}else if( (currentState == Yellow_Red) || (currentState == Red_Yellow) ){
		delay_nextState = RedYellow_delay;

	}else if((currentState == Green_RedP1) || (currentState == Red_GreenP2) ){
		delay_nextState = RedGreen_delay;
	}
}

void setStateDelay_mode3(){


	switch(currentState){
		case Red_Red:
			delay_nextState = gt1;
			//printf("%d delay_next state mode3\n", gt1);
			break;

		case Green_Red:
			delay_nextState = gt2;
			//printf("%d delay_next state mode3\n", gt2);
			break;

		case Green_RedP1:
			delay_nextState = gt2;
			//printf("%d delay_next state mode3\n", gt2);
			break;

		case Yellow_Red:
			delay_nextState = gt3;
			//printf("%d delay_next state mode3\n", gt3);
			break;

		case Red_Red_2:
			delay_nextState = gt4;
			//printf("%d delay_next state mode3\n", gt4);
			break;

		case Red_Green:
			delay_nextState = gt5;
			//printf("%d delay_next state mode3\n", gt5);
			break;

		case Red_GreenP2:
			delay_nextState = gt5;
			//printf("%d delay_next state mode3\n", gt5);
			break;

		case Red_Yellow:
			delay_nextState = gt6;
			//printf("%d delay_next state mode3\n", gt6);
			break;

	}
}

/////////////////////////////////////////////////
       //Set outputs based on state\\
////////////////////////////////////////////////

void led_outputs(){
	switch(currentState){
		case Red_Red:
			IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, NS_R + EW_R);
			break;
		case Green_Red:
			IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, NS_G + EW_R);
			break;
		case Yellow_Red:
			IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, NS_Y + EW_R);
			break;
		case Red_Red_2:
			IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, NS_R + EW_R);
			break;
		case Red_Green:
			IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, NS_R + EW_G);
			break;
		case Red_Yellow:
			IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, NS_R + EW_Y);
			break;
	}
}

void led_outputs_mode2(){
	switch(currentState){
		case Red_Red:
			IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, NS_R + EW_R);
			break;
		case Green_Red:
			IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, NS_G + EW_R );
			break;
		case Green_RedP1:
			IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, NS_G + EW_R + pedNSgo);
			break;
		case Yellow_Red:
			IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, NS_Y + EW_R );
			break;
		case Red_Red_2:
			IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, NS_R + EW_R);
			break;
		case Red_Green:
			IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, NS_R + EW_G );
			break;
		case Red_GreenP2:
			IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, NS_R + EW_G + pedEWgo);
			break;
		case Red_Yellow:
			IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, NS_R + EW_Y );
			break;




	}
}

///////////////////////////////////////////////
         //UART functions for mode 3-4\\
//////////////////////////////////////////////
int getUartMessage(){
	//@detail Returns inputs entered by user provided they are equal to 6
	//		  and not letters
	CLEAR(digits);
	char a = 0;
	uint8_t charcount = 0;
	uint8_t validnumInputs= 0;
	uint8_t numberLength = 0;
	bool isFirstDigitZero = false;

	while(a != '\n'){
		uint8_t S2=(IORD_ALTERA_AVALON_PIO_DATA(SWITCHES_BASE)&Switch2)&Switch2;
		checkSwitch();
		if((mode == Mode3||mode==Mode4) && S2){
			a = fgetc(fp);
			if((a == ',') || (a == '\n')){
				validnumInputs++;
				numberLength = 0;
				isFirstDigitZero = false;

			}else if((a < '0') || (a > '9')){
				sendUartMessage("Invalid! Input has to be 6 numbers!");
				CLEAR(digits);
				return false;
			}else{
				if((a == '0') && !isFirstDigitZero && numberLength == 0) {
					isFirstDigitZero = true;
					sendUartMessage("Invalid! Enter value above 0");
					return false;
				}

				numberLength++;
			}
			
			digits[charcount] = a;
			charcount++;


			if(numberLength > 4){
				sendUartMessage("Number has to be 4 digits only!");
				numberLength = 0;
				return false;
			}

		}else{
			a = '\n';
		}
	}

	digits[charcount] = '\0';	//Terminate string
	printf("%s\n", digits);
	printf("validinputs: %d\n ", validnumInputs);

	if(validnumInputs != 6) {
		printf("Inputs have to be = 6!");
		sendUartMessage("Inputs have to be = 6!");
		CLEAR(digits);
		return false;
	}else{
		printf("Inputs are 6!");
		sendUartMessage("Success.6 valid Inputs!");
		user_input_received=true;
		return true;
	}

}

void sendUartMessage(char message[]){
	//@detail puts user input on putty terminal
	//@note \r and \n are needed for ending line on Windows platform
	for(int i = 0; i < strlen(message); i++){
		fputc(message[i], fp);
	}
	fputc('\r', fp);
	fputc('\n', fp);
}

void parseDigits(){
	//@detail - Concatenates string till it finds a comma, converts to number
	//			and adds to global delay (t1-t6) Variables
	//@function atoi converts char string to int type
	//@macro CLEAR used for clearing array. This is due to retention of chars
	//		in indexes where a smaller number follows a larger number
	int j = 0;
	int delay = 0;
	char number[10];
	int charcount = 0;

	for(int i = 0; i < strlen(digits); i++){

		if(digits[i] == ',' || digits[i] == 'n' ){
			delay = atoi(number);
			storeDelay(delay, j);
			j++;
			charcount = 0;
			CLEAR(number);
		}else{
			number[charcount] = digits[i];
			charcount++;
		}
	}
}

void storeDelay(int delay, int j){
	//@detail Puts delays in global variables in order they were pressed
	j++;
	switch(j){
		case 1:
			gt1 = delay;
			printf("Delay is t1: %d\n", gt1);
			break;
		case 2:
			gt2 = delay;
			printf("Delay is t2: %d\n", gt2);
			break;
		case 3:
			gt3 = delay;
			printf("Delay is t3: %d\n", gt3);
			break;
		case 4:
			gt4 = delay;
			printf("Delay is t4: %d\n", gt4);
			break;
		case 5:
			gt5 = delay;
			printf("Delay is t5: %d\n", gt5);
			break;
		case 6:
			gt6 = delay;
			printf("Delay is t6: %d\n", gt6);
			break;
	}
}

//////////////////////////////////////////
		       //Helpers\\
//////////////////////////////////////////

//Check Switch Press for Mode Switching in a safe state
void checkSwitch(){

	int S0=(IORD_ALTERA_AVALON_PIO_DATA(SWITCHES_BASE)&Switch0)&Switch0;
	int S1=(IORD_ALTERA_AVALON_PIO_DATA(SWITCHES_BASE)&Switch1)&Switch1;

	if(!S0 && !S1){
		printf("mode 1\n");
		mode=Mode1;
	}else if(S0 && !S1){   
		printf("mode 2\n");
		mode=Mode2;
	}else if(!S0 && S1){
		printf("mode 3\n");
		mode=Mode3;
	}else if (S0 && S1){
		printf("mode 4\n");
		mode=Mode4;
	}

}

//Show Current Mode
void lcd_sel_mode(){
//Used to show current mode
	  lcd = fopen(LCD_NAME, "w");
	  fprintf(lcd, "%c%s", ESC, CLEAR_LCD_STRING);
	  fprintf(lcd, "Mode: %d\n", mode);
	  fclose(lcd);
}
