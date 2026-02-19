/* readclock - read the realtime clock */

#define RTC_BASE (long)0xDC0000

int fd;

main(argc, argv) 
int argc;
char **argv;
{
	char t1[20], t2[20];

	fd = open("/dev/mem", 0);
	if (!fd) {
		perror("/dev/mem");
		exit(1);
	}
	readrtc(t1);
	sleep(1);		/* wait to see if clock is running */
	readrtc(t2);
	if (strcmp(t1, t2))
		puts(t2);
	else
		puts("-q");	/* let date(1) prompt user for date */
}


readrtc(string)
char *string;
{
    int date[6], i;
    struct {
	char pad1[3], low, pad2[3], high;
    } clk_mem;

    lseek(fd, RTC_BASE, 0);
    for (i = 0; i < 6; ++i) {
	read(fd, &clk_mem, sizeof(clk_mem));
	date[i] = (clk_mem.low & 15) + 10 * (clk_mem.high & 15);
    }
    sprintf(string, "%02d%02d%02d%02d%02d%02d", date[4], date[3], date[5],
     date[2], date[1], date[0]);
}
