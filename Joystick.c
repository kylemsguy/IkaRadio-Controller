/*
Nintendo Switch Fightstick - Proof-of-Concept

Based on the LUFA library's Low-Level Joystick Demo
	(C) Dean Camera
Based on the HORI's Pokken Tournament Pro Pad design
	(C) HORI

This project implements a modified version of HORI's Pokken Tournament Pro Pad
USB descriptors to allow for the creation of custom controllers for the
Nintendo Switch. This also works to a limited degree on the PS3.

Since System Update v3.0.0, the Nintendo Switch recognizes the Pokken
Tournament Pro Pad as a Pro Controller. Physical design limitations prevent
the Pokken Controller from functioning at the same level as the Pro
Controller. However, by default most of the descriptors are there, with the
exception of Home and Capture. Descriptor modification allows us to unlock
these buttons for our use.
*/

/** \file
 *
 *  Main source file for the posts printer demo. This file contains the main tasks of
 *  the demo and is responsible for the initial application hardware configuration.
 */

#include "Joystick.h"

// extern const uint8_t image_data[0x12c1] PROGMEM;

/*** Debounce ****
The following is some -really bad- debounce code. I have a more robust library
that I've used in other personal projects that would be a much better use
here, especially considering that this is a stick indented for use with arcade
fighters.
This code exists solely to actually test on. This will eventually be replaced.
**** Debounce ***/
// Quick debounce hackery!
// We're going to capture each port separately and store the contents into a 32-bit value.
uint32_t pb_debounce = 0;
uint32_t pd_debounce = 0;
uint32_t pc_debounce = 0;

// We also need a port state capture. We'll use a 32-bit value for this.
uint32_t bd_state = 0;

// We'll also give us some useful macros here.
#define PINB_DEBOUNCED ((bd_state >> 0) & 0xFF)
#define PIND_DEBOUNCED ((bd_state >> 8) & 0xFF) 
#define PINC_DEBOUNCED ((bd_state >> 16) & 0xFF)

// So let's do some debounce! Lazily, and really poorly.
void debounce_ports(void) {
	// We'll shift the current value of the debounce down one set of 8 bits. We'll also read in the state of the pins.
	pb_debounce = (pb_debounce << 8) + PINB;
	pd_debounce = (pd_debounce << 8) + PIND;
	pc_debounce = (pc_debounce << 8) + PINC;

	// We'll then iterate through a simple for loop.
	for (int i = 0; i < 8; i++) {
		if ((pb_debounce & (0x1010101 << i)) == (0x1010101 << i)) // wat
			bd_state |= (1 << i);
		else if ((pb_debounce & (0x1010101 << i)) == (0))
			bd_state &= ~(uint32_t)(1 << i);

		if ((pd_debounce & (0x1010101 << i)) == (0x1010101 << i))
			bd_state |= (1 << (8 + i));
		else if ((pd_debounce & (0x1010101 << i)) == (0))
			bd_state &= ~(uint32_t)(1 << (8 + i));

		if ((pc_debounce & (0x1010101 << i)) == (0x1010101 << i))
			bd_state |= (1 << (16 + i));
		else if ((pc_debounce & (0x1010101 << i)) == (0))
			bd_state &= ~(uint32_t)(1 << (16 + i));
	}
	if (bd_state){
		PORTD |= 1 << 2;
	} else {
		PORTD &= ~(1 << 2);
	}
}

// Main entry point.
int main(void) {
	// We'll start by performing hardware and peripheral setup.
	SetupHardware();
	// We'll then enable global interrupts for our use.
	GlobalInterruptEnable();
	// Once that's done, we'll enter an infinite loop.
	for (;;)
	{
		// We need to run our task to process and deliver data for our IN and OUT endpoints.
		HID_Task();
		// We also need to run the main USB management task.
		USB_USBTask();
		// We also need to debounce the buttons
		debounce_ports();
	}
}

// Configures hardware and peripherals, such as the USB peripherals.
void SetupHardware(void) {
	// We need to disable watchdog if enabled by bootloader/fuses.
	MCUSR &= ~(1 << WDRF);
	wdt_disable();

	// We need to disable clock division before initializing the USB hardware.
	clock_prescale_set(clock_div_1);
	// We can then initialize our hardware and peripherals, including the USB stack.

	#ifdef ALERT_WHEN_DONE
	// Both PORTD and PORTB will be used for the optional LED flashing and buzzer.
	#warning LED and Buzzer functionality enabled. All pins on both PORTB and \
PORTD will toggle when printing is done.
	#endif

	DDRD  &= ~0xFF;
	PORTD |=  0xFF;

	DDRB  &= ~0xFF;
	PORTB |=  0xFF;

	// Init pin 13 on arduino leonardo (LEDPIN)
	DDRC &= ~0xFF;
	PORTC |= 0xff;
	DDRC |= 1 << 7;
	//PORTC &= ~(1 << 7);
	PORTC |= 1 << 7;

	// PIND to read from port D
	
	// The USB stack should be initialized last.
	USB_Init();
}

// Fired to indicate that the device is enumerating.
void EVENT_USB_Device_Connect(void) {
	// We can indicate that we're enumerating here (via status LEDs, sound, etc.).
	// turn off pin 13
	PORTC &= ~(1 << 7);
}

// Fired to indicate that the device is no longer connected to a host.
void EVENT_USB_Device_Disconnect(void) {
	// We can indicate that our device is not ready (via status LEDs, sound, etc.).
	// turn on pin 13
	PORTC |= 1 << 7; // TODO turn this into a #define
}

// Fired when the host set the current configuration of the USB device after enumeration.
void EVENT_USB_Device_ConfigurationChanged(void) {
	bool ConfigSuccess = true;

	// We setup the HID report endpoints.
	ConfigSuccess &= Endpoint_ConfigureEndpoint(JOYSTICK_OUT_EPADDR, EP_TYPE_INTERRUPT, JOYSTICK_EPSIZE, 1);
	ConfigSuccess &= Endpoint_ConfigureEndpoint(JOYSTICK_IN_EPADDR, EP_TYPE_INTERRUPT, JOYSTICK_EPSIZE, 1);

	// We can read ConfigSuccess to indicate a success or failure at this point.
}

// Process control requests sent to the device from the USB host.
void EVENT_USB_Device_ControlRequest(void) {
	// We can handle two control requests: a GetReport and a SetReport.

	// Not used here, it looks like we don't receive control request from the Switch.
}

// Process and deliver data from IN and OUT endpoints.
void HID_Task(void) {
	// If the device isn't connected and properly configured, we can't do anything here.
	if (USB_DeviceState != DEVICE_STATE_Configured)
		return;

	// We'll start with the OUT endpoint.
	Endpoint_SelectEndpoint(JOYSTICK_OUT_EPADDR);
	// We'll check to see if we received something on the OUT endpoint.
	if (Endpoint_IsOUTReceived())
	{
		// If we did, and the packet has data, we'll react to it.
		if (Endpoint_IsReadWriteAllowed())
		{
			// We'll create a place to store our data received from the host.
			USB_JoystickReport_Output_t JoystickOutputData;
			// We'll then take in that data, setting it up in our storage.
			while(Endpoint_Read_Stream_LE(&JoystickOutputData, sizeof(JoystickOutputData), NULL) != ENDPOINT_RWSTREAM_NoError);
			// At this point, we can react to this data.

			// However, since we're not doing anything with this data, we abandon it.
		}
		// Regardless of whether we reacted to the data, we acknowledge an OUT packet on this endpoint.
		Endpoint_ClearOUT();
	}

	// We'll then move on to the IN endpoint.
	Endpoint_SelectEndpoint(JOYSTICK_IN_EPADDR);
	// We first check to see if the host is ready to accept data.
	if (Endpoint_IsINReady())
	{
		// We'll create an empty report.
		USB_JoystickReport_Input_t JoystickInputData;
		// We'll then populate this report with what we want to send to the host.
		GetNextReport(&JoystickInputData);
		// Once populated, we can output this data to the host. We do this by first writing the data to the control stream.
		while(Endpoint_Write_Stream_LE(&JoystickInputData, sizeof(JoystickInputData), NULL) != ENDPOINT_RWSTREAM_NoError);
		// We then send an IN packet on this endpoint.
		Endpoint_ClearIN();
	}
}

typedef enum {
	SYNC_CONTROLLER,
	SYNC_POSITION,
	STOP_X,
	STOP_Y,
	MOVE_X,
	MOVE_Y,
	DONE
} State_t;
State_t state = SYNC_CONTROLLER;

#define ECHOES 2
int echoes = 0;
USB_JoystickReport_Input_t last_report;

int report_count = 0;
int xpos = 0;
int ypos = 0;
int portsval = 0;

// Prepare the next report for the host.
void GetNextReport(USB_JoystickReport_Input_t* const ReportData) {

	// Prepare an empty report
	memset(ReportData, 0, sizeof(USB_JoystickReport_Input_t));
	ReportData->LX = STICK_CENTER;
	ReportData->LY = STICK_CENTER;
	ReportData->RX = STICK_CENTER;
	ReportData->RY = STICK_CENTER;
	ReportData->HAT = HAT_CENTER;

	// Repeat ECHOES times the last report
	if (echoes > 0)
	{
		memcpy(ReportData, &last_report, sizeof(USB_JoystickReport_Input_t));
		echoes--;
		return;
	}

	// Read from the pins (4 buttons, + plus)
	int plus = PIND_DEBOUNCED & 2; // pin2
	int minus = PIND_DEBOUNCED & 1; // pin3
	int left = PIND_DEBOUNCED & (1 << 4); // pin4
	int L = PINC_DEBOUNCED & (1 << 6); // pin5
	int ZR = PIND_DEBOUNCED & (1 << 7); // pin6
	int A = PIND_DEBOUNCED & (1 << 6); // pin12; PD6

	// TODO fix PORTC reading

	switch (state)
	{
		case SYNC_CONTROLLER:
			if (report_count > 100)
			{
				report_count = 0;
				state = SYNC_POSITION;
			}
			else if (report_count == 25 || report_count == 50)
			{
				ReportData->Button |= SWITCH_L | SWITCH_R;
			}
			else if (report_count == 75 || report_count == 100)
			{
				ReportData->Button |= SWITCH_A;
			}
			report_count++;
			break;
		default:
			if(plus)
				ReportData->Button |= SWITCH_PLUS;
			if(minus)
				ReportData->Button |= SWITCH_MINUS;
			if(left)
				ReportData->HAT = HAT_LEFT;
			if(L)
				ReportData->Button |= SWITCH_L;
			if(ZR)
				ReportData->Button |= SWITCH_R;
			if(A)
				ReportData->Button |= SWITCH_A;
		}

	// States and moves management
	// switch (state)
	// {
	// 	case SYNC_CONTROLLER:
	// 		if (report_count > 100)
	// 		{
	// 			report_count = 0;
	// 			state = SYNC_POSITION;
	// 		}
	// 		else if (report_count == 25 || report_count == 50)
	// 		{
	// 			ReportData->Button |= SWITCH_L | SWITCH_R;
	// 		}
	// 		else if (report_count == 75 || report_count == 100)
	// 		{
	// 			ReportData->Button |= SWITCH_A;
	// 		}
	// 		report_count++;
	// 		break;
	// 	case SYNC_POSITION:
	// 		if (report_count == 250)
	// 		{
	// 			report_count = 0;
	// 			xpos = 0;
	// 			ypos = 0;
	// 			state = STOP_X;
	// 		}
	// 		else
	// 		{
	// 			// Moving faster with LX/LY
	// 			ReportData->LX = STICK_MIN;
	// 			ReportData->LY = STICK_MIN;
	// 		}
	// 		if (report_count == 75 || report_count == 150)
	// 		{
	// 			// Clear the screen
	// 			ReportData->Button |= SWITCH_MINUS;
	// 		}
	// 		report_count++;
	// 		break;
	// 	case STOP_X:
	// 		state = MOVE_X;
	// 		break;
	// 	case STOP_Y:
	// 		if (ypos < 120 - 1)
	// 			state = MOVE_Y;
	// 		else
	// 			state = DONE;
	// 		break;
	// 	case MOVE_X:
	// 		if (ypos % 2)
	// 		{
	// 			ReportData->HAT = HAT_LEFT;
	// 			xpos--;
	// 		}
	// 		else
	// 		{
	// 			ReportData->HAT = HAT_RIGHT;
	// 			xpos++;
	// 		}
	// 		if (xpos > 0 && xpos < 320 - 1)
	// 			state = STOP_X;
	// 		else
	// 			state = STOP_Y;
	// 		break;
	// 	case MOVE_Y:
	// 		ReportData->HAT = HAT_BOTTOM;
	// 		ypos++;
	// 		state = STOP_X;
	// 		break;
	// 	case DONE:
	// 		#ifdef ALERT_WHEN_DONE
	// 		portsval = ~portsval;
	// 		PORTD = portsval; //flash LED(s) and sound buzzer if attached
	// 		PORTB = portsval;
	// 		_delay_ms(250);
	// 		#endif
	// 		return;
	// }

	// // Inking
	// if (state != SYNC_CONTROLLER && state != SYNC_POSITION)
	// 	if (pgm_read_byte(&(image_data[(xpos / 8) + (ypos * 40)])) & 1 << (xpos % 8))
	// 		ReportData->Button |= SWITCH_A;

	// Prepare to echo this report
	memcpy(&last_report, ReportData, sizeof(USB_JoystickReport_Input_t));
	echoes = ECHOES;

}
