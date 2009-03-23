#include "la.h"

void la_init(struct la_state *la, void *npi_la, void *mem_base, int mem_offset, int size)
{
	la->npi_la = npi_la;
	la->mem = mem_base;
	la->offset = mem_offset;
	la->size = size;

	la->trigger0 = 0;
	la->trigger0_mask = 0;
	la->state_mask = 0xffffffff;
	la->autostop = 0;
	la->trigger_offset = 0;
}

void la_start(struct la_state *la)
{
	la->npi_la[0] = la->offset;
	la->npi_la[1] = 0;
	la->npi_la[2] = la->size - 1;
	la->npi_la[4] = la->trigger0;
	la->npi_la[5] = la->trigger0_mask;

	la->npi_la[6] = la->state_mask;
	la->npi_la[7] = la->trigger_offset;

	la->npi_la[3] = (la->autostop ? 4 : 0) | 1;
}

int la_get_wptr(struct la_state *la)
{
	return la->npi_la[1];
}

int la_get_state(struct la_state *la)
{
	return la->npi_la[3] & 3; // triggered (only if pretriggering has been enabled), capturing
}

int la_get_trigger_wptr(struct la_state *la)
{
	return (la->npi_la[3] - la->trigger_offset) &~7;
}

void la_stop(struct la_state *la)
{
	la->npi_la[3] = 0;
}

void la_set_trigger(struct la_state *la, int stage, unsigned long trigger, unsigned long trigger_mask)
{
	la->trigger0 = trigger;
	la->trigger0_mask = trigger_mask;
}

	/* enabling the pretrigger will leave "pretrigger" bytes of samples before the trigger in the ringbuffer,
		after that, capture will stop. */
void la_set_pretrigger(struct la_state *la, int enable, int pretrigger)
{
	la->autostop = enable;
	la->trigger_offset = la->size - pretrigger;
}

void la_set_state_mask(struct la_state *la, unsigned long state_mask)
{
	la->state_mask = state_mask;
}

