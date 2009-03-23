library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;

library proc_common_v2_00_a;
use proc_common_v2_00_a.proc_common_pkg.all;

--library UNISIM;
--use UNISIM.VComponents.all;

entity npi_ring_write is
  generic
  (
 	C_PI_ADDR_WIDTH : integer := 32;
	C_PI_DATA_WIDTH : integer := 64;
	C_PI_BE_WIDTH   : integer := 8;
	C_PI_RDWDADDR_WIDTH: integer := 4
   );

	port (
		-- Clk and (sync) reset (must use MPMC clock)
	NPI_clk : in std_logic;
	NPI_rst : in std_logic;
		-- NPI signals, read path is unused.
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

		-- data input (sync fifo style)
	In_Data: in std_logic_vector(0 to C_PI_DATA_WIDTH-1);
	In_Valid: in std_logic;
	
		-- address 
	Ring_write_enable: in std_logic; -- global enable signal
	Ring_wptr_new: in std_logic_vector(0 to C_PI_ADDR_WIDTH-1); -- for updating write pointer
	Ring_wptr_update: in std_logic;
	Ring_wptr_current: out std_logic_vector(0 to C_PI_ADDR_WIDTH-1); -- current write pointer
	Ring_base: in std_logic_vector(0 to C_PI_ADDR_WIDTH-1); -- base address (inside MPMC)
	Ring_mask: in std_logic_vector(0 to C_PI_ADDR_WIDTH-1); -- wptr mask

	Status: out std_logic_vector(0 to 31)
	);
	
end npi_ring_write;

architecture Behavioral of npi_ring_write is

	type write_fsm_type is (IDLE, ADDR);
	signal write_fsm: write_fsm_type;
	signal do_write: std_logic;
	signal data_counter: std_logic_vector(4 downto 0);
	signal ring_wptr: std_logic_vector(0 to C_PI_ADDR_WIDTH-1);

begin
	la_capture: process (NPI_Clk)
	begin
		if NPI_Clk'event and NPI_Clk = '1' then
			if NPI_Rst = '1' then
				XIL_NPI_WrFIFO_Flush <= '1';
				do_write <= '0';
				data_counter <= (others => '0');
			else
				XIL_NPI_WrFIFO_Flush <= '0';
				
				do_write <= '0';
				
				if Ring_write_enable = '1' and In_Valid = '1' then
					data_counter <= data_counter + 1;
					if data_counter = "11111" then -- 32 words written into queue?
						do_write <= '1';
					end if;
				end if;
			end if;
		end if;
	end process;
	process (NPI_Clk)
	begin
		if NPI_Clk'event and NPI_Clk = '1' then

				-- allow wptr update (also in reset)
			if Ring_wptr_update = '1' then
				Ring_wptr <= Ring_wptr_new;
			end if;

			if NPI_Rst = '1' then
				write_fsm <= IDLE;
			else
				write_fsm <= write_fsm;
				case write_fsm is
					when IDLE =>
						if XIL_NPI_InitDone  = '1' and do_write = '1'  then -- and XIL_NPI_WrFIFO_AlmostFull = '0'
							write_fsm <= ADDR;
						end if;
					when ADDR =>
						if XIL_NPI_AddrAck = '1' then	
							write_fsm <= IDLE;
							ring_wptr <= ring_wptr + (32 * C_PI_DATA_WIDTH / 8);
						end if;
					when others =>
						null;
				end case;
			end if;
		end if;
		case write_fsm is
			when IDLE =>
				XIL_NPI_AddrReq <= '0';
			when ADDR =>
				XIL_NPI_AddrReq <= '1';
			when others =>
				null;
		end case;
	end process;

	Status <= x"000000" & "000" & data_counter;

	XIL_NPI_RNW <= '0';
	XIL_NPI_Size <= x"5"; -- 64 word (=32bit) burst transfer
	XIL_NPI_WrFIFO_BE  <= (others => '1');
	XIL_NPI_Addr <= (ring_wptr and ring_mask) or ring_base;
	XIL_NPI_RdFIFO_Pop <= '0';
	XIL_NPI_RdFIFO_Flush <= '0';
	XIL_NPI_WrFIFO_Data <= In_Data;

	XIL_NPI_WrFIFO_Push <= In_Valid and Ring_write_enable and not NPI_Rst;

	Ring_wptr_current <= ring_wptr;

end Behavioral;

