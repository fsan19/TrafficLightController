# Compsys303 Assignment 1
This assignment involves designing a traffic light controller on a Altera(DE2-115) FPGA 
with four modes that successively increase in complexity. LEDs(0 - 5) model the traffic lights. 

## Run instructions
* Connect FPGA to PC using blaster and UART via RS232 cable
* Open Quartus Prime Programmer and click on Add File to add the cs303.sof for programming the nios processor
* Open nios2 build tools for eclipse. Click on File -> New -> Nios II application for BSP and from Template. Select nios2_system.sopcinfo, 
  select cpu, name project and select Hello World from template.
* Copy-paste the code from main.c into hello world.c and rename the file to main.c
* SW1 and SW0 switches are used to switch modes. 

Alternatively one can:
* In project explorer, right click -> import -> Nios II Software Build Tools Project -> Import Nios II Software Build Tools Project
  Choose Assignment1, tick on clean project when importing and repeat process for Assignment1_bsp

## Note
Mode changes only take place in a safe state i.e Red Red. Pedestrians can only cross if the repsective lane has a green light and polling for new values in mode 3 and mode 4 take place in a safe state as well.

### Mode 1 (SW1 and SW0 are low)
This mode is a simple traffic light controller that proceeds to the next state after a certain delay.

### Mode 2 (SW1 is low and SW0 is high)
This mode is the successor to mode 1 and uses KEY0 and KEY1 as pedestrians for North-South and East-West respectively. LED6 lights up for 
North South and LED7 for East West.

### Mode 3 (SW1 is high and SW0 is low)
This mode is the successor to mode 2 and includes UART for reconfiguring the delay between each state. It does not poll for new values
unless SW2 is set to 1. Once SW2 is raised the program will only accept values above 0 and digits. One can switch modes during reconfiguring values 
provided they pull SW2 down, change the mode and press Ctrl + J. After 6 values are entered, press Ctrl + J to resume the mode with new values.

### Mode 4 (SW1 and SW0 are high)
This is the last mode and contains logic from all the previous modes. It contains a red light camera that activates whenever a vehicle 
enters the intersection on a red or yellow light. The vehicle is modelled by KEY2. Odd press to enter and even press to exit. It captures
a picture instantly for red light but takes 2 seconds for a yellow light. The time a car enters and leaves the intersection is always printed 
on the screen.
