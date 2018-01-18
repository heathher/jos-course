/* See COPYRIGHT for copyright information. */

#include <inc/x86.h>
#include <kern/kclock.h>
#include <inc/vsyscall.h>
#include <kern/vsyscall.h>
#include <inc/time.h>

int gettime(void)
{
	nmi_disable();
	// LAB 12: your code here

	struct tm current;

	current.tm_sec = BCD2BIN(mc146818_read(RTC_SEC));
	current.tm_min = BCD2BIN(mc146818_read(RTC_MIN));
	current.tm_hour = BCD2BIN(mc146818_read(RTC_HOUR));
	current.tm_mday = BCD2BIN(mc146818_read(RTC_DAY));
	current.tm_mon = BCD2BIN(mc146818_read(RTC_MON)) - 1;
	current.tm_year = BCD2BIN(mc146818_read(RTC_YEAR));

	int time = timestamp(&current);
	vsys[VSYS_gettime] = time;

	nmi_enable();
	return time;
}

void
rtc_init(void)
{
	nmi_disable();
	// LAB 4: your code here
	//task 1
	outb(IO_RTC_CMND, RTC_BREG);   //switching on register B
	uint8_t reg_b;                 //new variable for register B
	reg_b = inb(IO_RTC_DATA);      //reading register B
	reg_b = reg_b | RTC_PIE;       //setting RTC_PIE bit
	outb(IO_RTC_CMND, RTC_BREG);   //writing new value of register
	outb(IO_RTC_DATA, reg_b);
	//task 5
	outb(IO_RTC_CMND, RTC_AREG);   //switching on register A
	uint8_t reg_a;                 //new variable for register A
	reg_a = inb(IO_RTC_DATA);      //reading register B
	reg_a = reg_a & 0xF0;          //take lower 4 bits
	reg_a = reg_a | 0x0F;
	outb(IO_RTC_CMND, RTC_AREG);   //writing new value of register
	outb(IO_RTC_DATA, reg_a);
	//
	nmi_enable();
}

uint8_t
rtc_check_status(void)
{
	uint8_t status = 0;
	// LAB 4: your code here
	//task 1
	outb(IO_RTC_CMND, RTC_CREG);   //switching on register C
	status = inb(IO_RTC_DATA);     //reading register C

	return status;
}

unsigned
mc146818_read(unsigned reg)
{
	outb(IO_RTC_CMND, reg);
	return inb(IO_RTC_DATA);
}

void
mc146818_write(unsigned reg, unsigned datum)
{
	outb(IO_RTC_CMND, reg);
	outb(IO_RTC_DATA, datum);
}

