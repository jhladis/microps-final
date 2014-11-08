/* MicroPs Final Project
  Robert Gambee and Jirka Hladis
  Fall 2014
  
  Pong program
*/

// global variables
short ballXpos = 0;
short ballYpos = 0;
short ballXvel = 0;
short ballYvel = 0;
short pad1Ypos = 0;
short pad2Ypos = 0;
char score1 = 0;
char score2 = 0;

// function prototypes
void initSPI(void);           // initialize SPI link with FPGA
void spiTXRX(void);           // send ball coords and scores, receive paddle coords
void playSound(int soundSel); // tell FPGA to play a sound
void updateBall(void);        // update ball position and velocity

void main(void) {

}
