// Copyright © 2019-2023
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

`include "vortex_afu.vh"

module vortex_afu_shim #(
    parameter C_S_AXI_CTRL_ADDR_WIDTH = 8,
	parameter C_S_AXI_CTRL_DATA_WIDTH = 32,
	parameter C_M_AXI_MEM_ID_WIDTH 	  = `PLATFORM_MEMORY_ID_WIDTH,
	parameter C_M_AXI_MEM_DATA_WIDTH  = (`PLATFORM_MEMORY_DATA_SIZE * 8),
	parameter C_M_AXI_MEM_ADDR_WIDTH  = 64,
    parameter C_M_AXI_MEM_NUM_BANKS   = `PLATFORM_MEMORY_NUM_BANKS + `NUM_MMIO_BANKS
) (
	// System signals
	input wire 									ap_clk,
	input wire 									ap_rst_n,

    // AXI4 master interface
    `REPEAT (`PLATFORM_MEMORY_NUM_BANKS, GEN_AXI_MEM, REPEAT_COMMA),

    // AXI4-Lite slave interface
    input  wire                                 s_axi_ctrl_awvalid,
    output wire                                 s_axi_ctrl_awready,
    input  wire [C_S_AXI_CTRL_ADDR_WIDTH-1:0]   s_axi_ctrl_awaddr,
    input  wire                                 s_axi_ctrl_wvalid,
    output wire                                 s_axi_ctrl_wready,
    input  wire [C_S_AXI_CTRL_DATA_WIDTH-1:0]   s_axi_ctrl_wdata,
    input  wire [C_S_AXI_CTRL_DATA_WIDTH/8-1:0] s_axi_ctrl_wstrb,
    input  wire                                 s_axi_ctrl_arvalid,
    output wire                                 s_axi_ctrl_arready,
    input  wire [C_S_AXI_CTRL_ADDR_WIDTH-1:0]   s_axi_ctrl_araddr,
    output wire                                 s_axi_ctrl_rvalid,
    input  wire                                 s_axi_ctrl_rready,
    output wire [C_S_AXI_CTRL_DATA_WIDTH-1:0]   s_axi_ctrl_rdata,
    output wire [1:0]                           s_axi_ctrl_rresp,
    output wire                                 s_axi_ctrl_bvalid,
    input  wire                                 s_axi_ctrl_bready,
    output wire [1:0]                           s_axi_ctrl_bresp,
`IGNORE_WARNINGS_BEGIN
    output wire                                 interrupt,
`IGNORE_WARNINGS_END
	output wire                                 m_axi_mem_2_wlast
);
    VX_afu_wrap #(
		.C_S_AXI_CTRL_ADDR_WIDTH (C_S_AXI_CTRL_ADDR_WIDTH),
		.C_S_AXI_CTRL_DATA_WIDTH (C_S_AXI_CTRL_DATA_WIDTH),
		.C_M_AXI_MEM_ID_WIDTH    (C_M_AXI_MEM_ID_WIDTH),
		.C_M_AXI_MEM_DATA_WIDTH  (C_M_AXI_MEM_DATA_WIDTH),
		.C_M_AXI_MEM_ADDR_WIDTH  (C_M_AXI_MEM_ADDR_WIDTH),
		.C_M_AXI_MEM_NUM_BANKS   (C_M_AXI_MEM_NUM_BANKS)
	) afu_wrap (
		.clk             	(ap_clk),
		.reset           	(~ap_rst_n),

		`REPEAT (`TOTAL_DEST_BANKS, AXI_MEM_ARGS, REPEAT_COMMA),

		.s_axi_ctrl_awvalid (s_axi_ctrl_awvalid),
		.s_axi_ctrl_awready (s_axi_ctrl_awready),
		.s_axi_ctrl_awaddr  (s_axi_ctrl_awaddr),
		.s_axi_ctrl_wvalid  (s_axi_ctrl_wvalid),
		.s_axi_ctrl_wready  (s_axi_ctrl_wready),
		.s_axi_ctrl_wdata   (s_axi_ctrl_wdata),
		.s_axi_ctrl_wstrb   (s_axi_ctrl_wstrb),
		.s_axi_ctrl_arvalid (s_axi_ctrl_arvalid),
		.s_axi_ctrl_arready (s_axi_ctrl_arready),
		.s_axi_ctrl_araddr  (s_axi_ctrl_araddr),
		.s_axi_ctrl_rvalid  (s_axi_ctrl_rvalid),
		.s_axi_ctrl_rready  (s_axi_ctrl_rready),
		.s_axi_ctrl_rdata   (s_axi_ctrl_rdata),
		.s_axi_ctrl_rresp   (s_axi_ctrl_rresp),
		.s_axi_ctrl_bvalid  (s_axi_ctrl_bvalid),
		.s_axi_ctrl_bready  (s_axi_ctrl_bready),
		.s_axi_ctrl_bresp   (s_axi_ctrl_bresp),

		.interrupt          (interrupt)
	);

/* verilator lint_off UNDRIVEN */
/* verilator lint_off UNUSEDSIGNAL */
wire                                 m_axi_mem_2_awvalid;
wire                                 m_axi_mem_2_awready;
wire [C_M_AXI_MEM_ADDR_WIDTH-1:0]       m_axi_mem_2_awaddr;
wire [C_M_AXI_MEM_ID_WIDTH-1:0]         m_axi_mem_2_awid;
wire [7:0]                           m_axi_mem_2_awlen;
wire                                 m_axi_mem_2_wvalid;
wire                                 m_axi_mem_2_wready;
wire [C_M_AXI_MEM_DATA_WIDTH-1:0]       m_axi_mem_2_wdata;
wire [C_M_AXI_MEM_DATA_WIDTH/8-1:0]     m_axi_mem_2_wstrb;
wire                                 m_axi_mem_2_arvalid;
wire                                 m_axi_mem_2_arready;
wire [C_M_AXI_MEM_ADDR_WIDTH-1:0]       m_axi_mem_2_araddr;
wire [C_M_AXI_MEM_ID_WIDTH-1:0]         m_axi_mem_2_arid;
wire [7:0]                           m_axi_mem_2_arlen;
wire                                 m_axi_mem_2_rvalid;
wire                                 m_axi_mem_2_rready;
wire [C_M_AXI_MEM_DATA_WIDTH-1:0]       m_axi_mem_2_rdata;
wire                                 m_axi_mem_2_rlast;
wire [C_M_AXI_MEM_ID_WIDTH-1:0]         m_axi_mem_2_rid;
wire [1:0]                           m_axi_mem_2_rresp;
wire                                 m_axi_mem_2_bvalid;
wire                                 m_axi_mem_2_bready;
wire [1:0]                           m_axi_mem_2_bresp;
wire [C_M_AXI_MEM_ID_WIDTH-1:0]         m_axi_mem_2_bid;



axi_ram #(
	.DATA_WIDTH(C_M_AXI_MEM_DATA_WIDTH),
	.ADDR_WIDTH(32),
	.ID_WIDTH(C_M_AXI_MEM_ID_WIDTH)

) axi_ram_instance (
		.clk              (ap_clk),
		.rst            (~ap_rst_n),


		.s_axi_awid		 (m_axi_mem_2_awid),
		.s_axi_awaddr	 (32'(m_axi_mem_2_awaddr)),
		.s_axi_awlen	 (m_axi_mem_2_awlen),
		.s_axi_awvalid	 (m_axi_mem_2_awvalid),
		.s_axi_awready	 (m_axi_mem_2_awready),
		.s_axi_wdata	 (m_axi_mem_2_wdata),
		.s_axi_wstrb	 (m_axi_mem_2_wstrb),
		.s_axi_wvalid	 (m_axi_mem_2_wvalid),
		.s_axi_wready	 (m_axi_mem_2_wready),
		.s_axi_bresp	 (m_axi_mem_2_bresp),
		.s_axi_bvalid	 (m_axi_mem_2_bvalid),
		.s_axi_bready	 (m_axi_mem_2_bready),
		.s_axi_bid		 (m_axi_mem_2_bid),
		.s_axi_arid		 (m_axi_mem_2_arid),
		.s_axi_araddr	 (32'(m_axi_mem_2_araddr)),
		.s_axi_arlen	 (m_axi_mem_2_arlen),
		.s_axi_arvalid	 (m_axi_mem_2_arvalid),
		.s_axi_arready	 (m_axi_mem_2_arready),
		.s_axi_rdata	 (m_axi_mem_2_rdata),
		.s_axi_rresp	 (m_axi_mem_2_rresp),
		.s_axi_rvalid	 (m_axi_mem_2_rvalid),
		.s_axi_rready	 (m_axi_mem_2_rready),
		.s_axi_rlast	 (m_axi_mem_2_rlast),
		.s_axi_rid		 (m_axi_mem_2_rid),
		`UNUSED_PIN(s_axi_arsize),
		`UNUSED_PIN(s_axi_awsize),
		`UNUSED_PIN(s_axi_arburst),
		`UNUSED_PIN(s_axi_awburst)

	);
/* verilator lint_on UNUSEDSIGNAL */
/* verilator lint_on UNDRIVEN */	
endmodule
