/* MicroPs Final Project
  Robert Gambee and Jirka Hladis
  Fall 2014
  
  Pong program
*/

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#include <P32xxxx.h>
//#include <proc/P32MX675F512H.h>
//#include <peripheral/timer.h>
//#include <plib.h>

#define fpga_reset PORTFbits.RF0
#define fpga_reset_tris TRISFbits.TRISF0

#define clk PORTEbits.RE0 
#define dataline PORTEbits.RE1
// Remember: TRIS has 0 for output and 1 for input. 
#define clk_tris TRISEbits.TRISE0
#define dataline_tris TRISEbits.TRISE1 
// define bits for setting open drain mode
#define clk_odc ODCEbits.ODCE0
#define dataline_odc ODCEbits.ODCE1

#define MS_RESET 0xff 
#define MS_DATA_REPORTING_ENABLED 0xf4 
#define MS_SUCCESS 0xaa 
#define MS_DEVICE_ID 0x00 
#define MS_ACKNOWLEDGEMENT 0xfa 
#define COORINFO_XSIGN 0x10 
#define COORINFO_YSIGN 0x20 

#define TEN_USEC 55
#define HUNDRD_USEC 1860

// function prototypes
void ps2setup(void);          // set up ps2 mouse connection
void delay (unsigned int usec);
void sendData(unsigned char data); 
unsigned char recvData(void); 
void readMouse(void);         // read amd process data from mouse

void initSPI(void);           // initialize SPI link with FPGA
void spi_send_receive(unsigned long send1, unsigned long send2);

void sendtoFPGA(void);         // send ball coords and scores, receive paddle coords
void initTimers(void);        // set up timers
void playSound(int soundSel); // tell FPGA to play a sound
void updateBall(void);        // update ball position and velocity

// game parameters
#define DEFXPOS_BALL 320
#define DEFYPOS_BALL 320
#define DEFXVEL_BALL 1
#define DEFYVEL_BALL 1
#define UPDATE_PERIOD 800 //30ms refresh period, 20MHz Pb_clk, 1:256 prescaler

// global variables
long ballXpos = DEFXPOS_BALL; // 10 bits for each position
long ballYpos = DEFYPOS_BALL;
long pad1Ypos = 240; // start at midpoint of screen
long pad2Ypos = 240;
signed short ballXvel = DEFXVEL_BALL; // initialize to default
signed short ballYvel = DEFYVEL_BALL;
short halfpadwidth = 50; //pixel width/2
short soundSel = 0; // up to 12 bit sound selecor
short score1 = 0; // 6 bits for score
short score2 = 0; 


unsigned short xpos=0,ypos=0; // Absolute x and y positions. 
unsigned char coorinfo;



//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


void main(void) {

//ps2setup();
initSPI();
initTimers();

//TRISD = 0; // PORTD as output, to display x-axis.

 while(1) {
	//readMouse();

	if (TMR1>=UPDATE_PERIOD){
    	TMR1=0; // reset timer
		updateBall(); // update ball position
	}

	sendtoFPGA();
 } 
}


void updateBall(void){
 	ballXpos=ballXpos+ballXvel;
 	ballYpos=ballYpos+ballYvel;
 	
	if (ballXpos>640) {
 		ballXpos=640;
 		ballXvel=-1*ballXvel;
 	}

 	if (ballXpos<0) {
 		//if (ballYpos<pad1Ypos+halfpadwidth && ballYpos>pad1Ypos-halfpadwidth) {
 			// means reflected with paddle
 			ballXpos=0;
 			ballXvel=-1*ballXvel;
 		/*} else { //reset ball with default values
 			// if missed paddle, incement other player's score, reset ball
 			score2++; 
 			short ballXpos = DEFXPOS_BALL;
 			short ballYpos = DEFYPOS_BALL;
 			short ballXvel = DEFXVEL_BALL;
 			short ballYvel = DEFYVEL_BALL;
 		}*/
 	}
	
 	if (ballYpos>480) {
 		ballYpos=480;
 		ballYvel=-1*ballYvel;
 	}
 	if (ballYpos<0) {
 		ballYpos=0;
 		ballYvel=-1*ballYvel;
 	}
}

void sendtoFPGA(void){ 
	/*Sending from PIC master to FPGA
	SPI2: [ballXpos, ballYpos, score1, score2]
	SPI4: [pad1Ypod, pad2Ypos, (sound select)]*/

	//mask off to prevent overflow
	unsigned long send1 = (ballXpos<<22)|(ballYpos<<12)|((0x3f&score1)<<6)|(0x3f&score2);
	unsigned long send2 = (pad1Ypos<<22)|(pad2Ypos<<12)|(0xfff&soundSel);
	spi_send_receive(send1, send2);
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

//Timer setup, adapted from class slides

void initTimers(void) {
// Peripheral clock at 20MHz
// T1CON
// bit 15: ON=1: enable timer
// bit 14: FRZ=0: keep running in exception mode
// bit 13: SIDL = 0: keep running in idle mode
// bit 12: TWDIS=1: ignore writes until current write completes
// bit 11: TWIP=0: don't care in synchronous mode
// bit 10-8: unused
// bit 7: TGATE=0: disable gated accumulation
// bit 6: unused
// bit 5-4: TCKPS=11: 1:256 prescaler, 0.05us*256=12.8us
// bit 3: unused
// bit 2: don't care in internal clock mode
// bit 1: TCS=0: use internal peripheral clock
// bit 0: unused
T1CON = 0b1001000000110000;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

//SPI communication, adapted from class slides

void initSPI(void) {
	long junk;
	fpga_reset_tris=0;
	fpga_reset=1; //reset fpga SPI module

	SPI4CONbits.ON = 0; // disable SPI to reset any previous state
	junk = SPI4BUF; // read SPI buffer to clear the receive buffer
	SPI4BRG = 0; //set BAUD rate to 10MHz, with Pclk at 20MHz 
	SPI4CONbits.MSTEN = 1; // enable master mode
	SPI4CONbits.CKE = 1; // set clock-to-data timing (data centered on rising SCK edge) 
	SPI4CONbits.MODE32 = 1; //enable 32 bit mode
	SPI4CONbits.ON = 1; // turn SPI on
	
	SPI2CONbits.ON = 0; // disable SPI to reset any previous state
	junk = SPI2BUF; // read SPI buffer to clear the receive buffer
	SPI2BRG = 0; //set BAUD rate to 10MHz, with Pclk at 20MHz 
	SPI2CONbits.MSTEN = 1; // enable master mode
	SPI2CONbits.CKE = 1; // set clock-to-data timing (data centered on rising SCK edge) 
	SPI2CONbits.MODE32 = 1; //enable 32 bit mode
	SPI2CONbits.ON = 1; // turn SPI on

	fpga_reset=0;
}

void spi_send_receive(unsigned long send1, unsigned long send2) {
	SPI2BUF = send1; // send data to slave
	while (SPI2STATbits.SPIBUSY); // wait until received buffer fills, indicating data received 

	SPI4BUF = send2; // send data to slave
	while (SPI4STATbits.SPIBUSY); // wait until received buffer fills, indicating data received 
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// PS2 communication, adapted from "MicroToys Guide: PS/2 Mouse N. Pinckney April 2005"

void readMouse(void){
	coorinfo = recvData(); // Mouse coordinate information 
 	// The tertiary operator is to determine if we should sign extend 
 	// the movement data.
	xpos += (COORINFO_XSIGN & coorinfo) ? 
    0xFFF0 | recvData() : recvData(); 
 	pad1Ypos += (COORINFO_YSIGN & coorinfo) ? 
 	0xFFF0 | recvData() : recvData();
 	
 	//keep within screen
 	if (pad1Ypos>480) pad1Ypos=480;
 	if (pad1Ypos<0) pad1Ypos=0;
	PORTD = xpos;
}

void sendData(unsigned char data) { 
 char i; 
 unsigned char parity = 1; 
 dataline_tris = 0; // Drive data pin 
 clk_tris = 0; // Drive clock pin. 
 clk = 0; // Drive clock pin low. 
 delay(HUNDRD_USEC); // Wait a bit, 100 us. 
 dataline = 0; // Drive data pin low. 
 delay(TEN_USEC); // Wait 10 us. 
 clk_tris = 1; // Stop driving clock. 
 
 for(i=0; i < 8; i++) {
	while(clk) ; // Wait until clock is low. 
 	dataline = data & 0x1; // Send bit. 
 	parity += data & 0x1; // Calculate parity. 
 	data = data >> 1; // Shift data. 
 	while(!clk) ; // Wait until clock is high. 
 } 

 while(clk) ; // Wait until clock is low. 
 dataline = parity & 0x1; // Send parity 
 while(!clk) ; // Wait until clock is high. 
 while(clk) ; // Wait until clock is low. 
 dataline = 1; // Send stop bit. 
 while(!clk) ; // Wait until clock goes high 
 dataline_tris = 1; // Stop driving data pin. 
 while(dataline); // Wait until acknowledgement bit. 
 while(clk); // Wait until clock goes low. 
 while(!clk || !dataline); // Wait until mouse releases clk and dataline. 
 // (both go high.) 
}

unsigned char recvData(void) { 
 unsigned char data = 0x0; 
 unsigned char parity; 
 char i; 
 // Start bit 
 while(clk); // Wait until clock is low. 
 while(!clk) ; // Wait until clock is high. 
 // 8 bits of data 
 for(i=0; i < 8; i++) { 
 while(clk); // Wait until clock line is low. 
 data = data >> 1; // Shift buffer. 
 data += dataline * 0x80; // Read next bit into buffer. 
 parity += data & 0x1; // Update parity. 
 while(!clk); // Wait until clock is high. 
 } 
 // Parity bit. Calculated but not checked. Just compare 
 // to value on dataline and handle accordingly. 
 while(clk) ; // Wait until clock is low. 
 while(!clk) ; // Wait until clock is high. 
 // Stop bit. Not checked. 
 while(clk) ; // Wait until clock is low. 
 while(!clk) ; // Wait until clock is high. 
 return data; 
} 

void ps2setup(void) { 
 clk_odc = 1; // enable open drain output on clock pin
 dataline_odc = 1; //  enable open drain output on data pin
 //clk_tris = 1; // Clock as input. (Open collector) 
 //dataline_tris = 1; // Dataline as input. (Open collector) 
 
sendData(MS_RESET); // Reset mouse. 
//unsigned char test=recvData();

while(recvData() != MS_SUCCESS); // Wait for self-test success. 
while(recvData() != MS_DEVICE_ID); // Wait for mouse device id to be 
// sent. 
 sendData(MS_DATA_REPORTING_ENABLED); // Enable data reporting. 
 while(recvData() != MS_ACKNOWLEDGEMENT); // Wait for acknowledgement 
 // that mouse is in data reporting 
// mode. 
}

void delay (unsigned int usec) {
 unsigned int start, stop;

  WriteCoreTimer(0);

  while (ReadCoreTimer() < usec);
}
