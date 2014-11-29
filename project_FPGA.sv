/* MicroPs Final Project
  Robert Gambee and Jirka Hladis
  Fall 2014
  
  SystemVerilog code for Pong game
*/

module project_FPGA(input  logic       clk,
                    output logic       vgaclk,                 // 25 MHz VGA clock
                    output logic       hsync, vsync, sync_b,   // to monitor & DAC
                    output logic [7:0] r_out, g_out, b_out,    // to video DAC
                    
                    input  logic       spiRst,
                    input  logic       sck_a, sck_b, sdo_a, sdo_b, // spi clock and output from pic
                    output logic       sdi_a, sdi_b,             // spi input to pic
                    output logic [7:0] led,
                    
                    output logic       audio_out);             // to audio amplifier

    logic [9:0] paddle1, paddle2, ballx, bally;
    logic [9:0] paddle1TEST, paddle2TEST, ballxTEST, ballyTEST;
    logic [9:0] x, y;
    logic [5:0] score1, score2;
    logic [31:0] send_a, send_b, received_a, received_b;
    logic [7:0] r_int, g_int, b_int;
    logic [11:0] sound_sel;
    
    assign led = paddle1[9:2];
    
    assign paddle1TEST = 10'd100;
    assign paddle2TEST = 10'd100;
    assign ballxTEST = 10'd100;
    assign ballyTEST = 10'd100;
    
    vgaCont vgaCont(.clk(clk), .r_int(r_int), .g_int(g_int), .b_int(b_int),
                    .vgaclk(vgaclk), .hsync(hsync), .vsync(vsync), .sync_b(sync_b), .x(x), .y(y),
                    .r_out(r_out), .g_out(g_out), .b_out(b_out));
    
    videoGen videoGen(.paddle1(paddle1TEST), .paddle2(paddle2TEST), .ballx(ballxTEST), .bally(ballyTEST),
                      .score1(score1), .score2(score2),.x(x), .y(y),
                      .r_int(r_int), .g_int(g_int), .b_int(b_int));
    
    spiSlave spiSlave1(.sck(sck_a), .sdo(sdo_a), .sdi(sdi_a), .reset(spiRst), .vsync(vsync),
                       .d(send_a), .q(received_a));
    spiSlave spiSlave2(.sck(sck_b), .sdo(sdo_b), .sdi(sdi_b), .reset(spiRst), .vsync(vsync),
                       .d(send_b), .q(received_b));
    
    dataDecode dataDecode(.send_a(send_a), .send_b(send_b), .received_a(received_a), .received_b(received_b),
                          .ballx(ballx), .bally(bally), .score1(score1), .score2(score2),
                          .paddle1(paddle1), .paddle2(paddle2), .sound_sel(sound_sel));

    audio audio(.pclk(clk), .sound_sel(sound_sel), .audio_out(audio_out));
endmodule


// module to drive VGA signals
module vgaCont#(parameter HMAX   = 10'd800,
                          VMAX   = 10'd525, 
                          HSTART = 10'd152,
                          WIDTH  = 10'd640,
                          VSTART = 10'd37,
                          HEIGHT = 10'd480)
               (input  logic       clk,
                input  logic [7:0] r_int, g_int, b_int,
                output logic       vgaclk, hsync, vsync, sync_b,
                output logic [9:0] x, y,
                output logic [7:0] r_out, g_out, b_out);
    
    logic [9:0] hcnt, vcnt;
    logic       oldhsync;
    logic       valid;
    
    // use PLL to create 25.175 MHz VGA clock
    vgaPLL vgaPLL_inst (.areset ( areset_sig ),
                        .inclk0 ( clk ),
                        .c0 ( vgaclk ),
                        .locked ( locked_sig ));
    
    // counters for horizontal and vertical positions
    always @(posedge vgaclk) begin
        if (hcnt >= HMAX) hcnt = 0;
        else hcnt++;
        if (hsync & ~oldhsync) begin // start of hsync; advance to next row
            if (vcnt >= VMAX) vcnt = 0;
            else vcnt++;
        end
        oldhsync = hsync;
    end
    
    // compute sync signals (active low)
    assign hsync = ~(hcnt >= 10'd8 & hcnt < 10'd104); // horizontal sync
    assign vsync = ~(vcnt >= 2 & vcnt < 4); // vertical sync
    assign sync_b = hsync | vsync;
    
    // determine x and y positions
    assign x = hcnt - HSTART;
    assign y = vcnt - VSTART;
    
    // force outputs to black when outside the legal display area
    assign valid = (hcnt >= HSTART & hcnt < HSTART+WIDTH &
                    vcnt >= VSTART & vcnt < VSTART+HEIGHT);
    assign {r_out,g_out,b_out} = valid ? {r_int,g_int,b_int} : 24'b0;
endmodule

// module to determine a given pixel's color
module videoGen#(parameter SCREENWIDTH = 10'd640,
                           SCREENHEIGHT = 10'd480,
                           PADDLEWIDTH = 10'd10,
                           PADDLEHEIGHT = 10'd50,
                           BALLSIZE = 10'd100,
                           PADDLECOLOR = 24'hFFFFFF,
                           BALLCOLOR = 24'h6060FF)
                (input  logic [9:0] paddle1, paddle2, ballx, bally, x, y,
                 input  logic [5:0] score1, score2,
                 output logic [7:0] r_int, g_int, b_int);

    logic [7:0] r_head, g_head, b_head;
    
    headerDisp headerDisp(.x(x), .y(y), .score1(score1), .score2(score2),
                          .r(r_head), .g(g_head), .b(b_head));
    
    logic in_paddle1, in_paddle2, in_ball;
    logic [9:0] dist2ballx, dist2bally;
    logic [19:0] dist2ballr;
    
    always_comb begin
        in_paddle1 = (x < PADDLEWIDTH) & (y >= paddle1) & (y < paddle1+PADDLEHEIGHT);
        in_paddle2 = (x >= SCREENWIDTH-PADDLEWIDTH) & (y >= paddle2) & (y < paddle2+PADDLEHEIGHT);
    
        dist2ballx = (x > ballx) ? (x - ballx) : (ballx - x);
        dist2bally = (y > bally) ? (y - bally) : (bally - y);
        dist2ballr = (dist2ballx*dist2ballx + dist2bally*dist2bally);
        in_ball = dist2ballr >= '0 & dist2ballr <= BALLSIZE;
    
        if (in_ball)
            {r_int, g_int, b_int} = BALLCOLOR;
        else if (in_paddle1 | in_paddle2)
            {r_int, g_int, b_int} = PADDLECOLOR;
        else
            {r_int, g_int, b_int} = {r_head, g_head, b_head};
    end
endmodule

// module to display header at top of screen, includes score
module headerDisp#(parameter HEADHEIGHT = 10'd10,
                             CHARHEIGHT = 10'd8,
                             SCREENWIDTH = 10'd640,
                             LABELWIDTH = 10'd60,
                             BGCOLOR = 24'h000000,
                             DIGCOLOR = 24'hFFFFFF)
                  (input  logic [9:0] x, y,
                   input  logic [5:0] score1, score2,
                   output logic [7:0] r, g, b);
    
    logic [3:0] digit_1a, digit_1b, digit_2a, digit_2b;
    logic [5:0] chars [1023:0];
    logic [7:0] str [9:0];
    logic [5:0] line;
    logic       pixel;
    
    initial $readmemb("charrom.txt", chars);
    initial $readmemh("str.txt", str);
    
    always_comb begin
//        digit_1a = score1 / 6'd10;
//        digit_1b = score1 % 6'd10;
//        digit_2a = score2 / 6'd10;
//        digit_2b = score2 % 6'd10;
        digit_1a = 4'd0;
        digit_1b = 4'd1;
        digit_2a = 4'd2;
        digit_2b = 4'd3;
    
        if (y < CHARHEIGHT) begin
            if (x < LABELWIDTH) begin
                str[6] = 8'h31;
                str[8] = digit_1a + 8'h30;
                str[9] = digit_1b + 8'h30;
                line = chars[{str[x/10'd6], y[2:0]}];
                pixel = line[3'd5 - x % 10'd6];
            end else if (x >= SCREENWIDTH - LABELWIDTH) begin
                str[6] = 8'h32;
                str[8] = digit_2a + 8'h30;
                str[9] = digit_2b + 8'h30;
                line = chars[{str[(x-(SCREENWIDTH-LABELWIDTH))/10'd6], y[2:0]}];
                pixel = line[3'd5 - (x - (SCREENWIDTH-LABELWIDTH)) % 10'd6];
            end else begin
                line = '0;
                pixel = 0;
            end
        end else if (y == HEADHEIGHT) begin
            line = '0;
            pixel = 1;
        end else begin
            line = '0;
            pixel = 0;
        end
        {r, g, b} = pixel ? DIGCOLOR : BGCOLOR;
    end
endmodule

// module to transmit information to and from PIC over SPI
module spiSlave(input  logic        sck, sdo, reset, vsync,
                input  logic [31:0] d,              // d = data to send
                output logic        sdi,
                output logic [31:0] q);             // q = received data
    
    logic [31:0] dSR, qSR;      // internal shift registers for data
    logic [31:0] qBuf;          // buffer for received data
    logic [4:0]  c;             // internal count signal
    logic        updateBuf;
    
    always_ff @(negedge sck, posedge reset) begin
        if (reset) begin
            c <= 5'b11111;
            dSR <= '0;
            qSR <= '0;
            sdi <= 0;
        end else begin
            c <= c + '1;
            qSR <= {qSR[30:0], sdo};
            dSR <= {dSR[30:0], 1'b0};
            sdi <= dSR[31];
        end
    end
    
    assign updateBuf = &c;
    
    always_ff @(posedge updateBuf)
        qBuf <= qSR;            // update qBuf once 32 bits have been received
    
    always_ff @(posedge vsync)
        q <= qBuf;              // update q every monitor refresh cycle
    
endmodule

// module to encode data to send to PIC
// and decode data received from PIC
module dataDecode(input  [31:0] received_a, received_b,
                  output [31:0] send_a, send_b,
                  output  [9:0] ballx, bally, paddle1, paddle2,
                  output  [5:0] score1, score2,
                  output [11:0] sound_sel);

    always_comb begin
        send_a = '0;         // don't send anything
        send_b = '0;         // don't send anything
        {ballx, bally, score1, score2} = received_a;
        {paddle1, paddle2, sound_sel} = received_b;
    end
endmodule

module audio#(parameter PERSIZE = 24,
              parameter DURSIZE = 8)
             (input  logic        pclk,
              input  logic [11:0] sound_sel,
              output logic        audio_out);
    
    logic [(PERSIZE+DURSIZE)-1:0] notes [15:0];
    logic [(PERSIZE+DURSIZE)-1:0] next_note;
    logic [PERSIZE-1:0] period, pcount;
    logic [DURSIZE-1:0] duration;
    logic [DURSIZE+3:0] dcount;
    
    initial $readmemh("notes.txt", notes);
    
    // 2 kHz clock for note durations
    audioPLL audioPLL_inst (.areset ( areset_sig ),
                            .inclk0 ( pclk ),
                            .c0 ( dclk ),
                            .locked ( locked_sig ));
    
    assign next_note = notes[sound_sel[6:0]];
    
    always_ff@(posedge dclk) begin
        if (dcount >= {duration, 4'b0}) begin
            dcount <= '0;
            {period, duration} <= next_note;
        end else
            dcount <= dcount + 1'b1;
    end
    
    always_ff@(posedge pclk) begin
        if (~|period)
            audio_out <= 0;
        else if (pcount >= period) begin
            pcount <= '0;
            audio_out <= ~audio_out;
        end else
            pcount <= pcount + 1'b1;
    end
endmodule
