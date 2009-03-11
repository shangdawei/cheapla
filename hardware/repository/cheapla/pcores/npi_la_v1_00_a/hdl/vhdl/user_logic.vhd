-- DO NOT EDIT BELOW THIS LINE --------------------
library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_arith.all;
use ieee.std_logic_unsigned.all;

library proc_common_v2_00_a;
use proc_common_v2_00_a.proc_common_pkg.all;


-- DO NOT EDIT ABOVE THIS LINE --------------------

library npi_la_v1_00_a;
use npi_la_v1_00_a.npi_ring_write;

entity user_logic is
  generic
  (
    -- ADD USER GENERICS BELOW THIS LINE ---------------
    --USER generics added here
	C_PI_ADDR_WIDTH : integer := 32;  -- fixed for XIL_NPI components
	C_PI_DATA_WIDTH : integer := 64;  -- fixed for XIL_NPI components (since mpmc3 also 32 allowed)
	C_PI_BE_WIDTH   : integer := 8;   -- fixed for XIL_NPI components (since mpmc3 also 4  allowed)
	C_PI_RDWDADDR_WIDTH: integer := 4;-- fixed for XIL_NPI components
	C_LA_BITS : integer := 32;
    -- ADD USER GENERICS ABOVE THIS LINE ---------------

    -- DO NOT EDIT BELOW THIS LINE ---------------------
    -- Bus protocol parameters, do not add to or delete
    C_SLV_DWIDTH                   : integer              := 32;
    C_NUM_REG                      : integer              := 8
    -- DO NOT EDIT ABOVE THIS LINE ---------------------
  );
  port
  (
    -- ADD USER PORTS BELOW THIS LINE ------------------
    --USER ports added here
	NPI_clk : in std_logic;
	NPI_rst : in std_logic;
	XIL_NPI_Addr : out std_logic_vector(C_PI_ADDR_WIDTH-1 downto 0); 
	XIL_NPI_AddrReq : out std_logic; 
	XIL_NPI_AddrAck : in std_logic; 
	XIL_NPI_RNW : out std_logic; 
	XIL_NPI_Size : out std_logic_vector(3 downto 0); 
	XIL_NPI_InitDone : in std_logic; 
	XIL_NPI_WrFIFO_Data : out std_logic_vector(C_PI_DATA_WIDTH-1 downto 0); 
	XIL_NPI_WrFIFO_BE : out std_logic_vector(C_PI_BE_WIDTH-1 downto 0); 
	XIL_NPI_WrFIFO_Push : out std_logic; 
	XIL_NPI_RdFIFO_Data : in std_logic_vector(C_PI_DATA_WIDTH-1 downto 0); 
	XIL_NPI_RdFIFO_Pop : out std_logic; 
	XIL_NPI_RdFIFO_RdWdAddr: in std_logic_vector(C_PI_RDWDADDR_WIDTH-1 downto 0); 
	XIL_NPI_WrFIFO_AlmostFull: in std_logic; 
	XIL_NPI_WrFIFO_Flush: out std_logic; 
	XIL_NPI_RdFIFO_Empty: in std_logic; 
	XIL_NPI_RdFIFO_Latency: in std_logic_vector(1 downto 0);
	XIL_NPI_RdFIFO_Flush: out std_logic;
	LA_Data: in std_logic_vector(0 to C_LA_BITS-1);
    -- ADD USER PORTS ABOVE THIS LINE ------------------

    -- DO NOT EDIT BELOW THIS LINE ---------------------
    -- Bus protocol ports, do not add to or delete
    Bus2IP_Clk                     : in  std_logic;
    Bus2IP_Reset                   : in  std_logic;
    Bus2IP_Data                    : in  std_logic_vector(0 to C_SLV_DWIDTH-1);
    Bus2IP_BE                      : in  std_logic_vector(0 to C_SLV_DWIDTH/8-1);
    Bus2IP_RdCE                    : in  std_logic_vector(0 to C_NUM_REG-1);
    Bus2IP_WrCE                    : in  std_logic_vector(0 to C_NUM_REG-1);
    IP2Bus_Data                    : out std_logic_vector(0 to C_SLV_DWIDTH-1);
    IP2Bus_RdAck                   : out std_logic;
    IP2Bus_WrAck                   : out std_logic;
    IP2Bus_Error                   : out std_logic
    -- DO NOT EDIT ABOVE THIS LINE ---------------------
  );

  attribute SIGIS : string;
  attribute SIGIS of Bus2IP_Clk    : signal is "CLK";
  attribute SIGIS of Bus2IP_Reset  : signal is "RST";

end entity user_logic;

------------------------------------------------------------------------------
-- Architecture section
------------------------------------------------------------------------------

architecture IMP of user_logic is

  signal slv_reg0                       : std_logic_vector(0 to C_SLV_DWIDTH-1);
  signal slv_reg1                       : std_logic_vector(0 to C_SLV_DWIDTH-1);
  signal slv_reg2                       : std_logic_vector(0 to C_SLV_DWIDTH-1);
  signal slv_reg3                       : std_logic_vector(0 to C_SLV_DWIDTH-1);
  signal slv_reg4                       : std_logic_vector(0 to C_SLV_DWIDTH-1);
  signal slv_reg5                       : std_logic_vector(0 to C_SLV_DWIDTH-1);
  signal slv_reg6                       : std_logic_vector(0 to C_SLV_DWIDTH-1);
  signal slv_reg7                       : std_logic_vector(0 to C_SLV_DWIDTH-1);
  signal slv_reg_write_sel              : std_logic_vector(0 to 7);
  signal slv_reg_read_sel               : std_logic_vector(0 to 7);
  signal slv_ip2bus_data                : std_logic_vector(0 to C_SLV_DWIDTH-1);
  signal slv_read_ack                   : std_logic;
  signal slv_write_ack                  : std_logic;


	signal ring_wptr, ring_base, ring_mask, status: std_logic_vector(0 to 31);
	signal engine_enabled, do_write: std_logic;

	signal la_reg, la_reg_pre, la_last: std_logic_vector(0 to C_LA_BITS - 1);

	signal trigger0, trigger0_mask: std_logic_vector(0 to C_LA_BITS - 1);
	signal triggered: std_logic;
	
	signal state_mask: std_logic_vector(0 to C_LA_BITS - 1);
	
	signal la_timestamp: std_logic_vector(0 to 31);
	signal write_data: std_logic_vector(0 to C_PI_DATA_WIDTH - 1);
	
	signal update_wptr: std_logic;
	
	signal timestamp: std_logic_vector(0 to 31);
	
begin

  slv_reg_write_sel <= Bus2IP_WrCE(0 to 7);
  slv_reg_read_sel  <= Bus2IP_RdCE(0 to 7);
  slv_write_ack     <= Bus2IP_WrCE(0) or Bus2IP_WrCE(1) or Bus2IP_WrCE(2) or Bus2IP_WrCE(3) or Bus2IP_WrCE(4) or Bus2IP_WrCE(5) or Bus2IP_WrCE(6) or Bus2IP_WrCE(7);
  slv_read_ack      <= Bus2IP_RdCE(0) or Bus2IP_RdCE(1) or Bus2IP_RdCE(2) or Bus2IP_RdCE(3) or Bus2IP_RdCE(4) or Bus2IP_RdCE(5) or Bus2IP_RdCE(6) or Bus2IP_RdCE(7);

  SLAVE_REG_WRITE_PROC : process( Bus2IP_Clk ) is
  begin

    if Bus2IP_Clk'event and Bus2IP_Clk = '1' then
      if Bus2IP_Reset = '1' then
        slv_reg0 <= (others => '0');   -- ring_base
        slv_reg1 <= (others => '0');   -- ring_wptr
        slv_reg2 <= (others => '0');   -- ring_mask
        slv_reg3 <= (others => '0');   -- command (EE)
        slv_reg4 <= (others => '0');   -- trigger0
        slv_reg5 <= (others => '0');   -- trigger0 mask
        slv_reg6 <= (others => '0');   -- state mask
        slv_reg7 <= (others => '0');
      else
			update_wptr <= '0';
        case slv_reg_write_sel is
			when "10000000" =>
				slv_reg0 <= Bus2IP_Data;
			when "01000000" =>
				slv_reg1 <= Bus2IP_Data;
				update_wptr <= '1';
			when "00100000" =>
				slv_reg2 <= Bus2IP_Data;
			when "00010000" =>
				slv_reg3 <= Bus2IP_Data;
			when "00001000" =>
				slv_reg4 <= Bus2IP_Data;
			when "00000100" =>
				slv_reg5 <= Bus2IP_Data;
			when "00000010" =>
				slv_reg6 <= Bus2IP_Data;
			when "00000001" =>
				slv_reg7 <= Bus2IP_Data;
			when others => null;
        end case;
      end if;
    end if;

  end process SLAVE_REG_WRITE_PROC;
  

	process (NPI_Clk)
	begin 
		if NPI_Clk'event and NPI_Clk = '1' then
			la_reg_pre <= LA_Data;
			la_reg <= la_reg_pre;

			do_write <= '0';
			if NPI_Rst = '1' then
				la_last <= (others => '0');
				timestamp <= (others => '0');
			else
				timestamp <= timestamp + '1';
				if (la_last and state_mask) /= (la_reg and state_mask) then
					do_write <= '1';
					la_last <= la_reg;
					la_timestamp <= timestamp;
				end if;
				
				if engine_enabled = '0' then
					triggered <= '0';
				elsif (trigger0_mask and la_reg) = trigger0 then
					triggered <= '1';
				end if;
			end if;
		end if;
	end process;
  
	ring_base <= slv_reg0;
	ring_mask <= slv_reg2;
	engine_enabled <= slv_reg3(0);

	trigger0 <= slv_reg4;
	trigger0_mask <= slv_reg5;
	state_mask <= slv_reg6;

	Inst_npi_ring_write: entity npi_la_v1_00_a.npi_ring_write
    generic map
    (
	  C_PI_ADDR_WIDTH => C_PI_ADDR_WIDTH,
	  C_PI_DATA_WIDTH => C_PI_DATA_WIDTH,
	  C_PI_BE_WIDTH   => C_PI_BE_WIDTH,
	  C_PI_RDWDADDR_WIDTH => C_PI_RDWDADDR_WIDTH
    )

	PORT MAP(
		NPI_clk => NPI_clk,
		NPI_rst => NPI_rst,
		XIL_NPI_Addr => XIL_NPI_Addr,
		XIL_NPI_AddrReq => XIL_NPI_AddrReq,
		XIL_NPI_AddrAck => XIL_NPI_AddrAck,
		XIL_NPI_RNW => XIL_NPI_RNW,
		XIL_NPI_Size => XIL_NPI_Size,
		XIL_NPI_InitDone => XIL_NPI_InitDone,
		XIL_NPI_WrFIFO_Data => XIL_NPI_WrFIFO_Data,
		XIL_NPI_WrFIFO_BE => XIL_NPI_WrFIFO_BE,
		XIL_NPI_WrFIFO_Push => XIL_NPI_WrFIFO_Push,
		XIL_NPI_RdFIFO_Data => XIL_NPI_RdFIFO_Data ,
		XIL_NPI_RdFIFO_Pop => XIL_NPI_RdFIFO_Pop,
		XIL_NPI_RdFIFO_RdWdAddr => XIL_NPI_RdFIFO_RdWdAddr,
		XIL_NPI_WrFIFO_AlmostFull => XIL_NPI_WrFIFO_AlmostFull,
		XIL_NPI_WrFIFO_Flush => XIL_NPI_WrFIFO_Flush,
		XIL_NPI_RdFIFO_Empty => XIL_NPI_RdFIFO_Empty,
		XIL_NPI_RdFIFO_Latency => XIL_NPI_RdFIFO_Latency,
		XIL_NPI_RdFIFO_Flush => XIL_NPI_RdFIFO_Flush,
		In_Data => write_data,
		In_Valid => do_write,
		Ring_write_enable => triggered,
		Ring_wptr_new => slv_reg1,
		Ring_wptr_update => update_wptr,
		Ring_wptr_current => ring_wptr,
		Ring_base => ring_base,
		Ring_mask => ring_mask,
		Status => status
	);
	
	write_data <= la_timestamp & la_last; -- write stimestamp and new value
	
  -- implement slave model software accessible register(s) read mux
  SLAVE_REG_READ_PROC : process( slv_reg_read_sel, slv_reg0, slv_reg1, slv_reg2, slv_reg3, slv_reg4, slv_reg5, slv_reg6, slv_reg7 ) is
  begin

    case slv_reg_read_sel is
      when "10000000" => slv_ip2bus_data <= slv_reg0;
      when "01000000" => slv_ip2bus_data <= ring_wptr; -- slv_reg1;
      when "00100000" => slv_ip2bus_data <= slv_reg2;
      when "00010000" => slv_ip2bus_data <= status; -- slv_reg3;
      when "00001000" => slv_ip2bus_data <= la_reg; -- slv_reg4;
      when "00000100" => slv_ip2bus_data <= la_last; -- slv_reg5;
      when "00000010" => slv_ip2bus_data <= timestamp; -- slv_reg6;
      when "00000001" => slv_ip2bus_data <= la_timestamp; -- slv_reg7;
      when others => slv_ip2bus_data <= (others => '0');
    end case;

  end process SLAVE_REG_READ_PROC;

  ------------------------------------------
  -- Example code to drive IP to Bus signals
  ------------------------------------------
  IP2Bus_Data  <= slv_ip2bus_data when slv_read_ack = '1' else
                  (others => '0');

  IP2Bus_WrAck <= slv_write_ack;
  IP2Bus_RdAck <= slv_read_ack;
  IP2Bus_Error <= '0';

end IMP;
