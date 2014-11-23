/* MicroPs Final Project
  Robert Gambee and Jirka Hladis
  Fall 2014
  
  Pong program
*/

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#include <P32xxxx.h> 
#include <peripheral/timer.h>

#define clk PORTEbits.RE0 
#define dataline PORTEbits.RE1 
// Remember: TRIS has 0 for output and 1 for input. 
#define clk_tris TRISEbits.TRISE0
#define dataline_tris TRISEbits.TRISE1 
// define bits for setting open drain mode
#define clk_odc ODCEbits.ODCE0
#define dataline_odc ODCEbits.ODCE1
// one microsecond delay, for Delay10TCYx 
#define US_DELAY 0.5 
#define MS_RESET 0xff 
#define MS_DATA_REPORTING_ENABLED 0xf4 
#define MS_SUCCESS 0xaa 
#define MS_DEVICE_ID 0x00 
#define MS_ACKNOWLEDGEMENT 0xfa 
#define COORINFO_XSIGN 0x10 
#define COORINFO_YSIGN 0x20 
/* Prototypes */ 
void wait(unsigned char t); 
void sendData(unsigned char data); 
void ps2setup(void);
unsigned char recvData(void); 
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// global variables
short ballXpos = 0;
short ballYpos = 0;
short ballXvel = 0;
short ballYvel = 0;
short pad1Ypos = 0;
short pad2Ypos = 0;
char score1 = 0;
char score2 = 0;

unsigned char coorinfo; // Mouse coordinate information 
unsigned short xpos=0,ypos=0; // Absolute x and y positions. 

// function prototypes
void initSPI(void);           // initialize SPI link with FPGA
void spiTXRX(void);           // send ball coords and scores, receive paddle coords
void playSound(int soundSel); // tell FPGA to play a sound
void updateBall(void);        // update ball position and velocity

void delay (unsigned int usec);
//

#define TEN_USEC 55
#define HUNDRD_USEC 1860

void main(void) {

ps2setup();

 while(1) { 
 	coorinfo = recvData(); 
 	// The tertiary operator is to determine if we should sign extend 
 	// the movement data.
	xpos += (COORINFO_XSIGN & coorinfo) ? 
 	0xFFF0 | recvData() : recvData(); 
 	ypos += (COORINFO_YSIGN & coorinfo) ? 
 	0xFFF0 | recvData() : recvData(); 
 	PORTD = xpos >> 8; // Drive MSB of xpos on PORTD. 

 } 
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
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
 TRISD = 0; // PORTD as output, to display x-axis.
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

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void delay (unsigned int usec) {
 unsigned int start, stop;

  /* get start ticks */
  WriteCoreTimer(0);

  /* calculate number of ticks for the given number of microseconds */
  //stop = ReadCoreTimer() + usec*2; // core timer frequnecy is 20Hz

  //stop = usec*7;
  /* wait till Count reaches the stop value */

  while (ReadCoreTimer() < usec);
}
