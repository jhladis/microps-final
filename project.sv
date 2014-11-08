module vga(input  logic    clk,
			  output logic       vgaclk,						// 25 MHz VGA clock
			  output logic       hsync, vsync, sync_b,	// to monitor & DAC
			  output logic [7:0] r, g, b, light,          // to video DAC
			  
			  input logic        sck, sdo, //spi clock and output from pic
			  output logic       sdi); //spi input to pic
