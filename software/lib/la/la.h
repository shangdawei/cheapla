#ifndef __la_h
#define __la_h

#ifdef XPAR_DDR2_SDRAM_MPMC_BASEADDR
#define LA_BUFFER_BASEADDR XPAR_DDR2_SDRAM_MPMC_BASEADDR
#endif
#ifdef XPAR_DDR_SDRAM_MPMC_BASEADDR
#define LA_BUFFER_BASEADDR XPAR_DDR_SDRAM_MPMC_BASEADDR
#endif

#define LA_BUFFER_OFFSET (32 * 1024 * 1024)
#define LA_BUFFER_SIZE   (32 * 1024 * 1024)

struct la_state
{
	int offset;
	volatile unsigned int *npi_la;
	volatile unsigned int *mem;
	int size;
	
	unsigned long trigger0, trigger0_mask, state_mask;
	unsigned long trigger_offset;
	int autostop;
};

extern void la_init(struct la_state *la, void *npi_la, void *mem_base, int mem_offset, int size);
extern void la_start(struct la_state *la);
extern int la_get_wptr(struct la_state *la);
extern int la_get_state(struct la_state *la);

extern void la_stop(struct la_state *la);
extern void la_set_trigger(struct la_state *la, int stage, unsigned long trigger, unsigned long trigger_mask);
extern void la_set_pretrigger(struct la_state *la, int enable, int pretrigger);
extern void la_set_state_mask(struct la_state *la, unsigned long state_mask);

#endif
