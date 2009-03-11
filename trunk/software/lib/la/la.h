#ifndef __la_h
#define __la_h

struct la_state
{
	int offset;
	volatile unsigned int *npi_la;
	volatile unsigned int *mem;
	int size;
	
	unsigned long trigger0, trigger0_mask, state_mask;
	
};

extern void la_init(struct la_state *la, void *npi_la, void *mem_base, int mem_offset, int size);
extern void la_start(struct la_state *la);
extern int la_get_wptr(struct la_state *la);
extern void la_stop(struct la_state *la);
extern void la_set_trigger(struct la_state *la, int stage, unsigned long trigger, unsigned long trigger_mask);
extern void la_set_state_mask(struct la_state *la, unsigned long state_mask);

#endif
