/* MicroPs Final Project
  Robert Gambee and Jirka Hladis
  Fall 2014
  
  Pong program
*/

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#include <P32xxxx.h>
#include <stdlib.h>

#define fpga_reset PORTFbits.RF0
#define fpga_reset_tris TRISFbits.TRISF0

#define clk1 PORTEbits.RE0 
#define dataline1 PORTEbits.RE1 
// Remember: TRIS has 0 for output and 1 for input. 
#define clk_tris1 TRISEbits.TRISE0
#define dataline_tris1 TRISEbits.TRISE1 
// define bits for setting open drain mode
#define clk_odc1 ODCEbits.ODCE0
#define dataline_odc1 ODCEbits.ODCE1

#define clk2 PORTEbits.RE2 
#define dataline2 PORTEbits.RE3 
// Remember: TRIS has 0 for output and 1 for input. 
#define clk_tris2 TRISEbits.TRISE2
#define dataline_tris2 TRISEbits.TRISE3 
// define bits for setting open drain mode
#define clk_odc2 ODCEbits.ODCE2
#define dataline_odc2 ODCEbits.ODCE3

#define MS_READ_DATA 0xeb
#define MS_RESET 0xff 
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
void sendData(unsigned char data, char mouse);
unsigned char recvData(char mouse);
void readMouse(char mouse);         // read and process data from mouse

void initSPI(void);           // initialize SPI link with FPGA
void spi_send_receive(unsigned long send1, unsigned long send2);

void sendtoFPGA(void);         // send ball coords and scores, receive paddle coords
void initTimers(void);        // set up timers
void playSound(int soundSel); // tell FPGA to play a sound
void updateBall(void);        // update ball position and velocity

// game parameters

// bounce space dimenions
// screen origin defined as upper right
#define YLOW  10
#define YHIGH 480
#define XLOW  0
#define XHIGH 640
#define OFFSET 200 //offset for starting ball in middle
#define DEFXPOS_BALL (rand()%(XHIGH-OFFSET-XLOW-OFFSET)+XLOW+OFFSET)
#define DEFYPOS_BALL (rand()%(YHIGH-YLOW)+YLOW)
#define VEL_MULT 4
#define DEFXVELDIR_BALL (VEL_MULT*(rand()%2*2-1)) //1 or -1
#define DEFYVELDIR_BALL (VEL_MULT*(rand()%2*2-1))
#define DEFXVELMAG_BALL (rand()%100+400) //400 to 500 pixels per second
#define DEFYVELMAG_BALL (rand()%100+400)
#define DEFUPDATEPERX (1000000*VEL_MULT/DEFXVELMAG_BALL/12.8) //times 12.8 us period
#define DEFUPDATEPERY (1000000*VEL_MULT/DEFYVELMAG_BALL/12.8)

#define PADWIDTH 50
#define PADTHICKNESS 10
#define BALLRADIUS 10

#define MSG_PAUSE 3*1000000/12.8 // 1:256 prescaler, 20MHz PB_clk
								 // time to display each message
#define PLAYING (score1<WINSCORE && score2<WINSCORE)

// sound effect adresses to send
#define START_SOUND 0
#define WALL_SOUND  0
#define PAD_SOUND   0
#define SCORE_SOUND 0
#define END_SOUND   0

// global variables
long ballXpos, ballYpos; // 10 bits for each position
long pad1Ypos = 240; // start at midpoint of screen
long pad2Ypos = 240;
unsigned short update_periodx, update_periody;
signed short ballXvel, ballYvel;

short msgSel = 0; // 3 bits for game messages

short soundSel = START_SOUND; // up to 12 bit sound selecor

short score1 = 0; // 6 bits for score
short score2 = 0; 

#define WINSCORE 10 //max of 63 because only 6 bits

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


void main(void) {

 ps2setup();
 initSPI();
 initTimers();

 ballXpos = DEFXPOS_BALL;// initialize to default
 ballYpos = DEFYPOS_BALL;
 ballXvel = DEFXVELDIR_BALL;
 ballYvel = DEFYVELDIR_BALL;
 update_periodx = DEFUPDATEPERX;
 update_periody = DEFUPDATEPERY;
 
 TMR2=0;
 while (msgSel<3){
 	readMouse(0);
 	readMouse(1);
 	sendtoFPGA();
 	if (TMR2>MSG_PAUSE){
 		msgSel++;
		TMR2=0;
 	}
 }
 
 msgSel=0;
 TMR1=0; //reset ball motion timers
 TMR4=0;
 while(PLAYING) {  // main game loop
	readMouse(0);
	readMouse(1);
    updateBall();
	sendtoFPGA();
 } 

 msgSel=3;
 TMR2=0;
 while (1){
	updateBall();
 	sendtoFPGA();
 	if (TMR2>MSG_PAUSE){
	msgSel = (score1>=WINSCORE) ? 4 : 5;
 	}
 }
}

void updateBall(void){
	if (TMR1>=update_periodx){
    	TMR1=0; // reset timer
		ballXpos+=ballXvel; // update ball position
	}
	if (TMR4>=update_periody){
    	TMR4=0; // reset timer
		ballYpos+=ballYvel; // update ball position
	}

	if (ballXpos>XHIGH-BALLRADIUS-PADTHICKNESS) {
		if (!PLAYING || ballYpos-BALLRADIUS<pad2Ypos+PADWIDTH && ballYpos+BALLRADIUS>pad2Ypos) {
 			// means reflected with paddle, or game over
	 		ballXpos=XHIGH-BALLRADIUS-PADTHICKNESS;
	 		ballXvel=-1*ballXvel;
			soundSel = (PLAYING) ? PAD_SOUND : WALL_SOUND;
 		} else { //reset ball with default values
 			// if missed paddle, incement other player's score, reset ball
 			score1++; 
			soundSel = SCORE_SOUND;
 			ballXpos = DEFXPOS_BALL;
 			ballYpos = DEFYPOS_BALL;
 			ballXvel = DEFXVELDIR_BALL;
 			ballYvel = DEFYVELDIR_BALL;
			update_periodx = DEFUPDATEPERX;
			update_periody = DEFUPDATEPERY;
 		}
 	}
 	

 	if (ballXpos<XLOW+BALLRADIUS+PADTHICKNESS) {
		if (!PLAYING || ballYpos-BALLRADIUS<pad1Ypos+PADWIDTH && ballYpos+BALLRADIUS>pad1Ypos) {
 			// means reflected with paddle, or game over
 			ballXpos=XLOW+BALLRADIUS+PADTHICKNESS;
 			ballXvel=-1*ballXvel;
			soundSel = (PLAYING) ? PAD_SOUND : WALL_SOUND;
 		} else { //reset ball with default values
 			// if missed paddle, incement other player's score, reset ball
 			score2++; 
			soundSel = SCORE_SOUND;
 			ballXpos = DEFXPOS_BALL;
 			ballYpos = DEFYPOS_BALL;
 			ballXvel = DEFXVELDIR_BALL;
 			ballYvel = DEFYVELDIR_BALL;
			update_periodx = DEFUPDATEPERX;
			update_periody = DEFUPDATEPERY;
 		}
 	}
	
 	if (ballYpos>YHIGH-BALLRADIUS) {
 		ballYpos=YHIGH-BALLRADIUS;
 		ballYvel=-1*ballYvel;
		soundSel = WALL_SOUND;
 	}
 	if (ballYpos<YLOW+BALLRADIUS) {
 		ballYpos=YLOW+BALLRADIUS;
 		ballYvel=-1*ballYvel;
		soundSel = WALL_SOUND;
 	}
}

void sendtoFPGA(void){ 
	/*Sending from PIC master to FPGA
	SPI2: [ballXpos, ballYpos, score1, score2]
	SPI4: [pad1Ypod, pad2Ypos, (sound select)]*/

	//mask off to prevent overflow
    fpga_reset=1; //reset fpga SPI module
	unsigned long send1 = (ballXpos<<22)|(ballYpos<<12)|((0x3f&score1)<<6)|(0x3f&score2);
	unsigned long send2 = (pad1Ypos<<22)|(pad2Ypos<<12)|(0xfff&soundSel<<3)|(0x7&msgSel);
	fpga_reset=0;
	spi_send_receive(send2, send1);
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

//Timer setup, adapted from class slides

void initTimers(void) {
// Peripheral clock at 20MHz
// T1CON and T2CON
// bit 15: ON=1: enable timer
// bit 14: FRZ=0: keep running in exception mode
// bit 13: SIDL = 0: keep running in idle mode
// bit 12: TWDIS=1: ignore writes until current write completes
// bit 11: TWIP=0: don't care in synchronous mode
// bit 10-8: unused
// bit 7: TGATE=0: disable gated accumulation
// bit 6: unused
// bit 5-4: TCKPS=11: 1:256 prescaler, 0.05us*256=12.8us
// bit 6-4: TCKPS=111: 1:256 prescaler for timer 2
// bit 3: T32=1 for 32 bit mode for timer 2, unused for timer 1
// bit 2: don't care in internal clock mode
// bit 1: TCS=0: use internal peripheral clock
// bit 0: unused
T1CON = 0b1001000000110000;
T2CON = 0b1001000001111000;
T4CON = 0b1001000000111000;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

//SPI communication, adapted from class slides

void initSPI(void) {
	long junk;
	fpga_reset_tris=0;
	fpga_reset=1; //reset fpga SPI module

	SPI4CONbits.ON = 0; // disable SPI to reset any previous state
	junk = SPI4BUF; // read SPI buffer to clear the receive buffer
	SPI4BRG = 1999; //set BAUD rate to 1MHz, with Pclk at 20MHz 
	SPI4CONbits.MSTEN = 1; // enable master mode
	SPI4CONbits.CKE = 1; // set clock-to-data timing (data centered on rising SCK edge) 
	SPI4CONbits.MODE32 = 1; //enable 32 bit mode
	SPI4CONbits.ON = 1; // turn SPI on
	
	SPI2CONbits.ON = 0; // disable SPI to reset any previous state
	junk = SPI2BUF; // read SPI buffer to clear the receive buffer
	SPI2BRG = 1999; //set BAUD rate to 10MHz, with Pclk at 20MHz 
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

void readMouse(char mouse){
	sendData(MS_READ_DATA, mouse); //request data
	while(recvData(mouse) != MS_ACKNOWLEDGEMENT); // Wait for acknowledgement
	unsigned char coorinfo = recvData(mouse); // Mouse coordinate information 
 	// The tertiary operator is to determine if we should sign extend 
 	// the movement data.
	short xpos = recvData(mouse);
	if (mouse) {
	 	pad2Ypos -= (COORINFO_YSIGN & coorinfo) ? 
 		0xFFFFFFF0 | recvData(mouse) : recvData(mouse);
		//keep within screen
		if (pad2Ypos>YHIGH-PADWIDTH) pad2Ypos=YHIGH-PADWIDTH;
		if (pad2Ypos<YLOW) pad2Ypos=YLOW;
	} else {
		pad1Ypos -= (COORINFO_YSIGN & coorinfo) ? 
 		0xFFFFFFF0 | recvData(mouse) : recvData(mouse);
		//keep within screen
		if (pad1Ypos>YHIGH-PADWIDTH) pad1Ypos=YHIGH-PADWIDTH;
		if (pad1Ypos<YLOW) pad1Ypos=YLOW;
	}
}


void sendData(unsigned char data, char mouse) { 
 char i; 
 unsigned char parity = 1; 
 if (mouse){
	 dataline_tris2 = 0; // Drive data pin 
	 clk_tris2 = 0; // Drive clock pin. 
	 clk2 = 0; // Drive clock pin low. 
	 delay(HUNDRD_USEC); // Wait a bit, 100 us. 
	 dataline2 = 0; // Drive data pin low. 
	 delay(TEN_USEC); // Wait 10 us. 
	 clk_tris2 = 1; // Stop driving clock. 
	 
	 for(i=0; i < 8; i++) {
		while(clk2) ; // Wait until clock is low. 
	 	dataline2 = data & 0x1; // Send bit. 
	 	parity += data & 0x1; // Calculate parity. 
	 	data = data >> 1; // Shift data. 
	 	while(!clk2) ; // Wait until clock is high. 
	 } 
	
	 while(clk2) ; // Wait until clock is low. 
	 dataline2 = parity & 0x1; // Send parity 
	 while(!clk2) ; // Wait until clock is high. 
	 while(clk2) ; // Wait until clock is low. 
	 dataline2 = 1; // Send stop bit. 
	 while(!clk2) ; // Wait until clock goes high 
	 dataline_tris2 = 1; // Stop driving data pin. 
	 while(dataline2); // Wait until acknowledgement bit. 
	 while(clk2); // Wait until clock goes low. 
	 while(!clk2 || !dataline2); // Wait until mouse releases clk and dataline. 
	 // (both go high.) 
 } else {
	 dataline_tris1 = 0; // Drive data pin 
	 clk_tris1 = 0; // Drive clock pin. 
	 clk1 = 0; // Drive clock pin low. 
	 delay(HUNDRD_USEC); // Wait a bit, 100 us. 
	 dataline1 = 0; // Drive data pin low. 
	 delay(TEN_USEC); // Wait 10 us. 
	 clk_tris1 = 1; // Stop driving clock. 
	 
	 for(i=0; i < 8; i++) {
		while(clk1) ; // Wait until clock is low. 
	 	dataline1 = data & 0x1; // Send bit. 
	 	parity += data & 0x1; // Calculate parity. 
	 	data = data >> 1; // Shift data. 
	 	while(!clk1) ; // Wait until clock is high. 
	 } 
	
	 while(clk1) ; // Wait until clock is low. 
	 dataline1 = parity & 0x1; // Send parity 
	 while(!clk1) ; // Wait until clock is high. 
	 while(clk1) ; // Wait until clock is low. 
	 dataline1 = 1; // Send stop bit. 
	 while(!clk1) ; // Wait until clock goes high 
	 dataline_tris1 = 1; // Stop driving data pin. 
	 while(dataline1); // Wait until acknowledgement bit. 
	 while(clk1); // Wait until clock goes low. 
	 while(!clk1 || !dataline1); // Wait until mouse releases clk and dataline
 }
}


unsigned char recvData(char mouse) { 
 unsigned char data = 0x0; 
 unsigned char parity; 
 char i; 
 if (mouse) { 
	// Start bit 
	 while(clk2); // Wait until clock is low. 
	 while(!clk2) ; // Wait until clock is high. 
	 // 8 bits of data 
	 for(i=0; i < 8; i++) { 
	  while(clk2); // Wait until clock line is low. 
	 data = data >> 1; // Shift buffer. 
	 data += dataline2 * 0x80; // Read next bit into buffer. 
	 parity += data & 0x1; // Update parity. 
	 while(!clk2); // Wait until clock is high. 
	 } 
	 // Parity bit. Calculated but not checked. Just compare 
	 // to value on dataline and handle accordingly. 
	 while(clk2) ; // Wait until clock is low. 
	 while(!clk2) ; // Wait until clock is high. 
	 // Stop bit. Not checked. 
	 while(clk2) ; // Wait until clock is low. 
	 while(!clk2) ; // Wait until clock is high. 
 } else {
	// Start bit 
	 while(clk1); // Wait until clock is low. 
	 while(!clk1) ; // Wait until clock is high. 
	 // 8 bits of data 
	 for(i=0; i < 8; i++) { 
	  while(clk1); // Wait until clock line is low. 
	 data = data >> 1; // Shift buffer. 
	 data += dataline1 * 0x80; // Read next bit into buffer. 
	 parity += data & 0x1; // Update parity. 
	 while(!clk1); // Wait until clock is high. 
	 } 
	 // Parity bit. Calculated but not checked. Just compare 
	 // to value on dataline and handle accordingly. 
	 while(clk1) ; // Wait until clock is low. 
	 while(!clk1) ; // Wait until clock is high. 
	 // Stop bit. Not checked. 
	 while(clk1) ; // Wait until clock is low. 
	 while(!clk1) ; // Wait until clock is high. 
 }
 return data; 
} 


void ps2setup(void) {
 clk_odc1 = 1; // enable open drain output on clock pin
 dataline_odc1 = 1; //  enable open drain output on data pin
 
 clk_odc2 = 1; // enable open drain output on clock pin
 dataline_odc2 = 1; //  enable open drain output on data pin
 
 sendData(MS_RESET, 0); // Reset mouse. 
 while(recvData(0) != MS_SUCCESS); // Wait for self-test success. 
 while(recvData(0) != MS_DEVICE_ID); // Wait for mouse device id to be sent. 

 sendData(MS_RESET, 1); // Reset mouse. 
 while(recvData(1) != MS_SUCCESS); // Wait for self-test success. 
 while(recvData(1) != MS_DEVICE_ID); // Wait for mouse device id to be sent. 
}


void delay (unsigned int usec) {
 unsigned int start, stop;
  WriteCoreTimer(0);
  while (ReadCoreTimer() < usec);
}
