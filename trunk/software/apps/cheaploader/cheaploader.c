/*
 * Copyright (c) 2001 William L. Pitts
 * Modifications (c) 2004 Felix Domke
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are freely
 * permitted provided that the above copyright notice and this
 * paragraph and the following disclaimer are duplicated in all
 * such forms.
 *
 * This software is provided "AS IS" and without any express or
 * implied warranties, including, without limitation, the implied
 * warranties of merchantability and fitness for a particular
 * purpose.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "blconfig.h"
#include "elf_abi.h"
#include "xparameters.h"
#include "xgpio_l.h"

static unsigned char load_exec ();
static unsigned char *flbuf;

/* We don't use interrupts/exceptions. 
   Dummy definitions to reduce code size on MicroBlaze */
#ifdef __MICROBLAZE__
void _interrupt_handler () {}
void _exception_handler () {}
void _hw_exception_handler () {}
#endif

#ifdef __PPC__
#include <unistd.h>
/* Save some code and data space on PowerPC 
   by defining a minimal exit */
void exit (int ret)
{
	_exit (ret);
}
#endif

/* ======================================================================
 * Determine if a valid ELF image exists at the given memory location.
 * ====================================================================== */
int valid_elf_image (void *addr)
{
	Elf32_Ehdr *ehdr;               /* Elf header structure pointer */

	ehdr = (Elf32_Ehdr *) addr;

	if (!IS_ELF (*ehdr)) {
		print("## No elf image at address ");
		putnum((int)addr);
		print("\r\n");
		return 0;
	}

	if (ehdr->e_type != ET_EXEC) {
		print("## Not a 32-bit elf image at address ");
		putnum((int)addr);
		print("\r\n");
		return 0;
	}

	return 1;
}

/* ======================================================================
 * A very simple elf loader, assumes the image is valid, returns the
 * entry point address.
 * ====================================================================== */
unsigned long load_elf_image (void *addr)
{
	Elf32_Ehdr *ehdr;
	Elf32_Shdr *shdr;
	unsigned char *strtab = 0;
	unsigned char *image;
	int i;

	ehdr = (Elf32_Ehdr *) addr;
	/* Find the section header string table for output info */
	shdr = (Elf32_Shdr *) (addr + ehdr->e_shoff +
	                      (ehdr->e_shstrndx * sizeof (Elf32_Shdr)));

	if (shdr->sh_type == SHT_STRTAB)
		strtab = (unsigned char *) (addr + shdr->sh_offset);

	/* Load each appropriate section */
	for (i = 0; i < ehdr->e_shnum; ++i) {
		shdr = (Elf32_Shdr *) (addr + ehdr->e_shoff +
		                       (i * sizeof (Elf32_Shdr)));

		if (!(shdr->sh_flags & SHF_ALLOC)
		   || shdr->sh_size == 0)
		        continue;

		if (strtab) {
			if (shdr->sh_type == SHT_NOBITS) print("Clear");
			else print("Load");
			print("ing ");
			print(&strtab[shdr->sh_name]);
			print(" @ 0x");
			putnum(shdr->sh_addr);
			print(" ("); putnum(shdr->sh_size);	print(" bytes)\n\r");
			}

		if (shdr->sh_type == SHT_NOBITS) {
		    memset((void *)shdr->sh_addr, 0, shdr->sh_size);
		} else {
		    image = (unsigned char *) addr + shdr->sh_offset;
		    memcpy((void *) shdr->sh_addr,
		           (const void *) image,
		            shdr->sh_size);
		}
    }

   return ehdr->e_entry;
 }


int main(void) {
    print ("\r\nCheapLA Bootloader v1.0: \r\n");
	unsigned int switches = XGpio_In32(XPAR_DIP_SWITCHES_4BIT_BASEADDR);
	print("DIP switches = ");
	putnum(switches);
	print("\r\n");
    flbuf = (unsigned char*)FLASH_IMAGE_BASEADDR;
	if (switches & 8) flbuf += 0x800000; // 8 MB
    return load_exec();
}

static unsigned char load_exec(void) {
    unsigned char ret;
    void (*laddr)();
    unsigned char done = 0;
    print("load_exec(");
	putnum((uint32_t)flbuf);
	print(")\n\r");
	if (!valid_elf_image(flbuf)) {
		print("Invalid elf image, hanging :(\r\n");
		while(1) ;
	}
	laddr = load_elf_image(flbuf);

    print("\r\nExecuting program starting at address: ");
    putnum((uint32_t)laddr);
    print("\r\n");

    (*laddr)();                 
  
    /* We will be dead at this point */
    return 0;
}
