/* MicroPs Final Project
  Robert Gambee and Jirka Hladis
  Fall 2014
  
  Pong program System Verilog code
*/

module toplevel(          input  logic    clk,
			  output logic       vgaclk,		   // 25 MHz VGA clock
			  output logic       hsync, vsync, sync_b, // to monitor & DAC
			  output logic [7:0] r, g, b, light,       // to video DAC
			  
			  output logic audio_out, // to audio amplifier
			  
			  inout logic ps2clk1, ps2clk2, //bidirectional interface to two mice
			  inout logic ps2data1, ps2data2,
			  
			  input logic        sck, sdo,             //spi clock and output from pic
			  output logic       sdi);                 //spi input to pic
	
	logic [9:0] mousepos1, mousepos2, ballx, bally;
	logic [11:0] score;
	logic [31:0] send, received;
	
	always_ff @(posedge clk)
	begin
		send  <= {{12'b0},{mousepos1}, {mousepos2}};
		ballx <= received [31:22];
		bally <= received [21:12];
		score <= received [11:0];
	end
	
	vga vga(.clk(clk), .paddle1(mousepos1), .paddle2(mousepos2), .ballx(ballx), .bally(bally), 
		.vgaclk(vgaclk), .hsync(hsync), .vsync(vsync), .sync_b(sync_b), .r(r), .g(g), .b(b));
	spi_slave spi_slave(.sck(sck),.sdo(sdo),.sdi(sdi),.reset(reset),.d(send),.q(received));
	sound sound(.clk(clk), .soundsel(soundsel), .audio_out(audio_out));
	ps2 ps2_1(.clk(clk), .ps2clk(ps2clk1), .ps2data(ps2data1), .mousepos(mousepos1));
	ps2 ps2_2(.clk(clk), .ps2clk(ps2clk2), .ps2data(ps2data2), .mousepos(mousepos2));
endmodule
