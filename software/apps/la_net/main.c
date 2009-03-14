#include <stdio.h>

	// the printf is buggy. I hate xilinx for this. Make sure to always use xil_printf.
#define printf xil_printf

#include "xenv_standalone.h"
#include "xparameters.h"

#include "lwip/init.h"
#include "lwip/tcp.h"

#include "netif/xadapter.h"

#include "mb_interface.h"
#include "xtmrctr_l.h"
#include "xintc.h"
#include "../../lib/la/la.h"

#define EMAC_BASEADDR  XPAR_ETHERNET_MAC_BASEADDR

#define LWIP_TIMER_CYCLES (XPAR_CPU_CORE_CLOCK_FREQ_HZ / 1000 \
                           * TCP_TMR_INTERVAL )

static struct netif *netif, server_netif;
static u32 timer_last;

		/* lightweight timer */
volatile u32 *reg_timer = (void*)XPAR_XPS_TIMER_1_BASEADDR;

u32 timer_get(void)
{
	return reg_timer[2]; // TCR0
}

void timer_init(void)
{
	reg_timer[0] = 0x90; // enable in generator mode 
}

void network_init(void)
{
	struct ip_addr ipaddr, netmask, gw;

	unsigned char mac_ethernet_address[] = { 0x00, 0x09, 0x34, 0x00, 0x01, 0x02 }; // we used a fixed mac address here.
	netif = &server_netif;
	/* initliaze IP addresses to be used */
	IP4_ADDR(&ipaddr,   10,   0, 120, 204); // we used a fixed IP address here
	IP4_ADDR(&netmask, 255, 255, 255,  0);
	IP4_ADDR(&gw,       10,   0, 120,  1);
	lwip_init();

	if (!xemac_add(netif, &ipaddr, &netmask, &gw, mac_ethernet_address, EMAC_BASEADDR)) {
		printf("Error adding N/W interface\n\r");
		return;
	}
	netif_set_default(netif);
//	dhcp_start(netif);
}

static void my_tmr(void)
{
	static int my_timer;
   ++my_timer;

   if(my_timer == 10) {
      my_timer = 0;
   }
   if(my_timer & 1) {
      /* Call tcp_fasttmr() every 2 ms, i.e.,
       * every other timer my_tmr() is called. */
      tcp_fasttmr();
   }
   if(my_timer == 0 || my_timer == 5) {
      /* Call tcp_slowtmr() every 5 ms, i.e.,
       * every fifth timer my_tmr() is called. */
      tcp_slowtmr();
      if (tcp_ticks%2000 == 0)
         /* Call etharp_tmr() every 20th call to tcp_slowtmr().
          * tcp_ticks is a global var defined in core/tcp.c */
         etharp_tmr();
   }
}

void network_poll(void)
{
	xemacif_input(netif);
	
	u32 now = timer_get();
	if (now - timer_last >= LWIP_TIMER_CYCLES)
	{
		timer_last = now;
		my_tmr();
	}
}

XIntc intc;

void interrupt_init(void)
{
	XIntc *intcp;
	intcp = &intc;


	XIntc_Initialize(intcp, XPAR_XPS_INTC_0_DEVICE_ID);
	XIntc_Start(intcp, XIN_REAL_MODE);

	/* Start the interrupt controller */
	XIntc_mMasterEnable(XPAR_XPS_INTC_0_BASEADDR);
	XIntc_mEnableIntr(XPAR_XPS_INTC_0_BASEADDR, XPAR_ETHERNET_MAC_IP2INTC_IRPT_MASK);

	microblaze_register_handler((XInterruptHandler)XIntc_InterruptHandler, intcp);
}

void cpu_invalidate_dcache_range(unsigned addr, unsigned len)
{
	microblaze_init_dcache_range(addr, len);
}

struct la_state *la;

#define BUFFER_SIZE 8192
struct control_state
{
	int retries;
	unsigned char *buffer;
	int buffer_start, buffer_end, buffer_len;
	int disc, idle;
	
	int stream, stream_last_wptr;

	char recv_line[128];
	int recv_ptr;
	
	struct tcp_pcb *pcb;
};
static void control_print(struct control_state *hs, const char *data);


static void control_err(void *arg, err_t err)
{
	struct control_state *hs = arg;
	mem_free(hs->buffer);
	mem_free(hs);
}

static void control_close_conn(struct tcp_pcb *pcb, struct control_state *hs)
{
	tcp_arg(pcb, 0);
	tcp_sent(pcb, 0);
	tcp_recv(pcb, 0);
	mem_free(hs->buffer);
	mem_free(hs);
	tcp_close(pcb);
}

static void control_send_data(struct tcp_pcb *pcb, struct control_state *hs)
{
	err_t err;
	int len;
	int streaming = 0;
	void *start = hs->buffer + hs->buffer_start;
	int valid = hs->buffer_end - hs->buffer_start;
	if (valid < 0)
		valid = hs->buffer_len - hs->buffer_start;
	printf("sending %d bytes\n", valid);
	
	if (!valid && hs->stream)
	{
		int wptr = la_get_wptr(la);
		if (wptr != hs->stream_last_wptr)
		{
			printf("have more data to stream, wptr now %08x, last %08x\n", wptr, hs->stream_last_wptr);
			start = ((void*)la->mem) + hs->stream_last_wptr;
			valid = wptr - hs->stream_last_wptr;
			if (valid < 0)
				valid = la->size - hs->stream_last_wptr;
			streaming = 1;
			microblaze_init_dcache_range((int)start, valid);
			printf("streaming out %d bytes\n", valid);
		}
	}
	
	if (!valid)
	{
		printf("now idle\n");

		hs->idle = 1;
		return;
	}
	
	if (tcp_sndbuf(pcb) < valid)
		len = tcp_sndbuf(pcb);
	else
		len = valid;

	do {
		err = tcp_write(pcb, start, len, 1);
		if (err == ERR_MEM)
			len /= 2;
	} while (err == ERR_MEM && len > 1);
	
	printf("wrote %d\n", len);
	
	if (err == ERR_OK)
	{
		if (!streaming)
		{
			hs->buffer_start += len;
			if (hs->buffer_start == hs->buffer_len)
				hs->buffer_start = 0;
		} else
		{
			hs->stream_last_wptr += len;
			if (hs->stream_last_wptr == la->size)
				hs->stream_last_wptr = 0;
		}
	} else
		printf("send_data: error %d len %d %d\n", err, len, tcp_sndbuf(pcb));
}

static err_t control_poll(void *arg, struct tcp_pcb *pcb)
{
	struct control_state *hs = arg;

	printf("poll!\n");
	
	if (!hs)
	{
		tcp_abort(pcb);
		return ERR_ABRT;
	} else if (!hs->idle)
	{
		++hs->retries;
		if (hs->retries == 4)
		{
			tcp_abort(pcb);
			return ERR_ABRT;
		}
		control_send_data(pcb, hs);
	} else if ((hs->buffer_start != hs->buffer_end) || hs->disc || hs->stream)
		control_send_data(pcb, hs);
	return ERR_OK;
}

static err_t control_sent(void *arg, struct tcp_pcb *pcb, u16_t len)
{
	struct control_state *hs = arg;
	
	hs->retries = 0;
	if ((!hs->disc) || (hs->buffer_start != hs->buffer_end)) 
		control_send_data(pcb, hs);
	else
		control_close_conn(pcb, hs);

	return ERR_OK;
}

static int control_handle_line(struct control_state *hs)
{
	char *line = hs->recv_line;
	printf("[%s]\n", line);
	switch (line[0])
	{
	case 'g':
		la_start(la);
		control_print(hs, "+LA RUNNING\n");
		break;
	case 's':
		la_stop(la);
		control_print(hs, "+LA STOPPED\n");
		break;
	case 'q':
	{
		char res[12];
		sprintf(res, "+%08x\n", la_get_wptr(la));
		control_print(hs, res);
		break;
	}
	case 'Q':
		control_print(hs, "+BYE!\n");
		return 1;
	case 'D':
		hs->stream = 1;
		control_print(hs, "+DATA\n");
		break;
	default:
		control_print(hs, "-UNKNOWN COMMAND\n");
		break;
	}
	return 0;
}

static int control_handle_input(struct control_state *hs, const void *data, int len)
{
	printf("handle %d bytes\n", len);
	int res = 0;
	const unsigned char *d = data;
	while (len)
	{
		if (*d == '\n' || *d == '\r')
		{
			hs->recv_line[hs->recv_ptr++] = 0;
			res |= control_handle_line(hs);
			hs->recv_ptr = 0;
		} else
			hs->recv_line[hs->recv_ptr++] = *d++;
	
		len--;
		if (hs->recv_ptr == sizeof (hs->recv_line) - 1)
		{
			control_print(hs, "-LINE TOO LONG\n");
			hs->recv_ptr = 0;
		}
	}
	return res;
}

static err_t control_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
	struct control_state *hs = arg;
	
	if (err == ERR_OK && p)
	{
		tcp_recved(pcb, p->tot_len);
		struct pbuf *q;
		for (q = p; q; q = q->next)
			hs->disc |= control_handle_input(hs, q->payload, q->len);
		pbuf_free(p);
	}
	
	control_poll(arg, pcb);
	
	if (err == ERR_OK && !p)
		control_close_conn(pcb, hs);
	return ERR_OK;
}

static void control_enqueue(struct control_state *hs, const void *data, int len)
{
	int towrite = len;
	if ((hs->buffer_len - hs->buffer_end) < towrite)
		towrite = hs->buffer_len - hs->buffer_end;
	
	memcpy(hs->buffer + hs->buffer_end, data, towrite);
	hs->buffer_end += towrite;
	if (hs->buffer_end == hs->buffer_len)
		hs->buffer_end = 0;
	
	len -= towrite;
	data = data + towrite;
	
	if (len)
	{
		memcpy(hs->buffer, data, len);
		hs->buffer_end = len;
	}
}

static void control_print(struct control_state *hs, const char *data)
{
	control_enqueue(hs, data, strlen(data));
}

static err_t control_accept(void *arg, struct tcp_pcb *pcb, err_t err)
{
	struct control_state * hs;
	printf("[CONTROL] new connection\n");
	
	hs = mem_malloc(sizeof(struct control_state));
	if (!hs)
		return ERR_MEM;
	
	hs->buffer = mem_malloc(BUFFER_SIZE);
	if (!hs->buffer)
	{
		mem_free(hs);
		return ERR_MEM;
	}
	hs->retries = 0;
	hs->buffer_start = hs->buffer_end = 0;
	hs->buffer_len = BUFFER_SIZE;
	hs->disc = hs->idle = 0;
	hs->stream = 0;
	hs->stream_last_wptr = 0;
	
	tcp_arg(pcb, hs);
	tcp_recv(pcb, control_recv);
	tcp_err(pcb, control_err);
	tcp_poll(pcb, control_poll, 4);
	tcp_sent(pcb, control_sent);
	
	hs->pcb = pcb;

	control_print(hs, ">CheapLA (+net)\n");
	control_send_data(pcb, hs);
	
	return ERR_OK;
}

void control_init(void)
{
	struct tcp_pcb *pcb = tcp_new();
	tcp_bind(pcb, IP_ADDR_ANY, 5000);
	pcb = tcp_listen(pcb);
	tcp_accept(pcb, control_accept);
}

extern void httpd_start(void);

int main(void)
{
	microblaze_init_icache_range(0, XPAR_MICROBLAZE_0_CACHE_BYTE_SIZE);
	microblaze_init_dcache_range(0, XPAR_MICROBLAZE_0_DCACHE_BYTE_SIZE);
	XCACHE_ENABLE_ICACHE();
	XCACHE_ENABLE_DCACHE(); 
	printf("hello world\n");
	
	int i;
	
	printf("[INT]");
	interrupt_init();
	printf("[NET]");
	network_init();
	printf("[TIMER]");
	timer_init();
	printf("[IRQ]");
	
	printf("\n");
	
	microblaze_enable_interrupts();
	printf("\n");
	netif_set_up(netif);
	
	control_init();
	httpd_start();

	printf("init LA\n");

	struct la_state my_la;
	la = &my_la;
	
	la_init(la, (void*)XPAR_NPI_LA_0_BASEADDR, (void*)(LA_BUFFER_BASEADDR + LA_BUFFER_OFFSET), 
			LA_BUFFER_OFFSET, LA_BUFFER_SIZE); // use 32MB for LA samples
	
	printf("CheapLA ready!\n");

	while (1)
	{
		volatile unsigned int *npi = (void*)XPAR_NPI_LA_0_BASEADDR;
		printf("|%08x %08x %08x %08x %08x %08x %08x %08x\r", npi[0], npi[1], npi[2], npi[3], npi[4], npi[5], npi[6], npi[7]);

		network_poll();
	}
}


