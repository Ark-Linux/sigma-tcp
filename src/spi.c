/*
 * MIT License
 * 
 * Copyright (c) 2019 Tymphany
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

#include "sigma_tcp.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static void spi_mode_enable(void);

static void pabort(const char *s)
{
	perror(s);
	abort();
}

static const char *device = "/dev/spidev1.0";
static uint32_t mode = 0;
static uint8_t bits = 8;
static char *input_file;
static char *output_file;
static uint32_t speed = 1000000;
static uint16_t delay=0;
static int verbose;
static int i2s_fd;
char *input_tx;

static void print_usage(const char *prog)
{

	printf("Usage: %s [-DsbdlHOLC3]\n", prog);
	puts("  -D --device   device to use (default /dev/spidev1.1)\n"
	     "  -s --speed    max speed (Hz)\n"
	     "  -d --delay    delay (usec)\n"
	     "  -b --bpw      bits per word\n"
	     "  -i --input    input data from a file (e.g. \"test.bin\")\n"
	     "  -o --output   output data to a file (e.g. \"results.bin\")\n"
	     "  -l --loop     loopback\n"
	     "  -H --cpha     clock phase\n"
	     "  -O --cpol     clock polarity\n"
	     "  -L --lsb      least significant bit first\n"
	     "  -C --cs-high  chip select active high\n"
	     "  -3 --3wire    SI/SO signals shared\n"
	     "  -v --verbose  Verbose (show tx buffer)\n"
	     "  -p            Send data (e.g. \"1234\\xde\\xad\")\n"
	     "  -N --no-cs    no chip select\n"
	     "  -R --ready    slave pulls low to pause\n"
	     "  -2 --dual     dual transfer\n"
	     "  -4 --quad     quad transfer\n");
	exit(1);
}

static void parse_opts(int argc, char *argv[])
{
	while (1) {
		static const struct option lopts[] = {
			{ "device",  1, 0, 'D' },
			{ "speed",   1, 0, 's' },
			{ "delay",   1, 0, 'd' },
			{ "bpw",     1, 0, 'b' },
			{ "input",   1, 0, 'i' },
			{ "output",  1, 0, 'o' },
			{ "loop",    0, 0, 'l' },
			{ "cpha",    0, 0, 'H' },
			{ "cpol",    0, 0, 'O' },
			{ "lsb",     0, 0, 'L' },
			{ "cs-high", 0, 0, 'C' },
			{ "3wire",   0, 0, '3' },
			{ "no-cs",   0, 0, 'N' },
			{ "ready",   0, 0, 'R' },
			{ "dual",    0, 0, '2' },
			{ "verbose", 0, 0, 'v' },
			{ "quad",    0, 0, '4' },
			{ NULL, 0, 0, 0 },
		};
		int c;

		c = getopt_long(argc, argv, "D:s:d:b:i:o:lHOLC3NR24p:v",
				lopts, NULL);

		if (c == -1)
			break;

		switch (c) {
		case 'D':
			device = optarg;
			break;
		case 's':
			speed = atoi(optarg);
			break;
		case 'd':
			delay = atoi(optarg);
			break;
		case 'b':
			bits = atoi(optarg);
			break;
		case 'i':
			input_file = optarg;
			break;
		case 'o':
			output_file = optarg;
			break;
		case 'l':
			mode |= SPI_LOOP;
			break;
		case 'H':
			mode |= SPI_CPHA;
			break;
		case 'O':
			mode |= SPI_CPOL;
			break;
		case 'L':
			mode |= SPI_LSB_FIRST;
			break;
		case 'C':
			mode |= SPI_CS_HIGH;
			break;
		case '3':
			mode |= SPI_3WIRE;
			break;
		case 'N':
			mode |= SPI_NO_CS;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'R':
			mode |= SPI_READY;
			break;
		case 'p':
			input_tx = optarg;
			break;
		case '2':
			mode |= SPI_TX_DUAL;
			break;
		case '4':
			mode |= SPI_TX_QUAD;
			break;
		default:
			print_usage(argv[0]);
			break;
		}
	}
	if (mode & SPI_LOOP) {
		if (mode & SPI_TX_DUAL)
			mode |= SPI_RX_DUAL;
		if (mode & SPI_TX_QUAD)
			mode |= SPI_RX_QUAD;
	}
}

static void spi_mode_enable(void)
{
	uint8_t buf[10] ={0x00};
	int ret =0;
	ret = write(i2s_fd,buf,10);
	usleep(1000);
	ret = write(i2s_fd,buf,10);
	usleep(1000);
	ret = write(i2s_fd,buf,10);
	usleep(1000);
	if(ret ==0)
	  ret++;
	else
	  ret =0;
}
static int spi_open(int argc, char *argv[])
{
	int ret = 0;

	parse_opts(argc, argv);

	i2s_fd = open(device, O_RDWR);
	if (i2s_fd < 0) {
		pabort("can't open device");
		return 1;
	}
	else
	{
		printf("spi open\n");
	}

	/*
	 * spi mode
	 */
	ret = ioctl(i2s_fd, SPI_IOC_WR_MODE32, &mode);
	if (ret == -1) {
		pabort("can't set spi mode");
		return 1;
	}

	ret = ioctl(i2s_fd, SPI_IOC_RD_MODE32, &mode);
	if (ret == -1) {
		pabort("can't get spi mode");
		return 1;
	}

	/*
	 * bits per word
	 */
	ret = ioctl(i2s_fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
	if (ret == -1) {
		pabort("can't set bits per word");
		return 1;
	}

	ret = ioctl(i2s_fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
	if (ret == -1) {
		pabort("can't get bits per word");
		return 1;
	}

	/*
	 * max speed hz
	 */
	ret = ioctl(i2s_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
	if (ret == -1) {
		pabort("can't set max speed hz");
		return 1;
	}

	ret = ioctl(i2s_fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
	if (ret == -1) {
		pabort("can't get max speed hz");
		return 1;
	}

	printf("spi mode: 0x%x\n", mode);
	printf("bits per word: %d\n", bits);
	printf("max speed: %d Hz (%d KHz)\n", speed, speed/1000);
	spi_mode_enable();
	return 0;
}

static int spi_read(unsigned int addr, unsigned int len, uint8_t *data)
{
	int ret;
	//int out_fd;
	uint8_t msg_buf[3];
	struct spi_ioc_transfer xfer[2] = {
		{
		.tx_buf = (unsigned long)0,
		.rx_buf = (unsigned long)0,
		.len = 3,
		.delay_usecs = delay,
		.speed_hz = speed,
		.bits_per_word = bits,
		},
		{
		.tx_buf = (unsigned long)0,
		.rx_buf = (unsigned long)data,
		.len = len,
		.delay_usecs = delay,
		.speed_hz = speed,
		.bits_per_word = bits,
		}
	};
	msg_buf[0] = 0x01;
	msg_buf[1] = addr >> 8;
	msg_buf[2] = addr & 0xff;
	xfer[0].tx_buf = (unsigned long)msg_buf;
	xfer[0].len = 3;

	if (mode & SPI_TX_QUAD)
		xfer[0].tx_nbits = 4;
	else if (mode & SPI_TX_DUAL)
		xfer[0].tx_nbits = 2;
	if (mode & SPI_RX_QUAD)
		xfer[0].rx_nbits = 4;
	else if (mode & SPI_RX_DUAL)
		xfer[0].rx_nbits = 2;
	if (!(mode & SPI_LOOP)) {
		if (mode & (SPI_TX_QUAD | SPI_TX_DUAL))
			xfer[0].rx_buf = 0;
		else if (mode & (SPI_RX_QUAD | SPI_RX_DUAL))
			xfer[0].tx_buf = 0;
	}

	ret = ioctl(i2s_fd, SPI_IOC_MESSAGE(2), xfer);
	if (ret < 1) {
		pabort("read: can't send spi message");
		return 1;
	}
	return 0;
}

static int spi_write(unsigned int addr, unsigned int len, const uint8_t *data)
{
	int ret;
	int i;
	//int out_fd;
	uint8_t msg_buf[3];
	struct spi_ioc_transfer xfer[2] = {
		{
		.tx_buf = (unsigned long)0,
		.rx_buf = (unsigned long)0,
		.len = 3,
		.delay_usecs = delay,
		.speed_hz = speed,
		.bits_per_word = bits,
		},
		{
		.tx_buf = (unsigned long)data,
		.rx_buf = (unsigned long)0,
		.len = len,
		.delay_usecs = delay,
		.speed_hz = speed,
		.bits_per_word = bits,
		}
	};
	msg_buf[0] = 0; //chip spi address
	msg_buf[1] = addr >> 8;
	msg_buf[2] = addr & 0xff;
	xfer[0].tx_buf = (unsigned long)msg_buf;
	xfer[0].len = 3;
	//printf("xfer[0].tx_buf:\n");
	//for(i = 0; i < 3; i++)
	//printf(" %.2x",*((char*)(xfer[0].tx_buf)+i));
	//printf("\n");
	
	//printf("xfer[1].tx_buf:\n");
	//for(i = 0; i < len; i++)
	//printf(" %.2x",*((char*)(xfer[1].tx_buf)+i));
	//printf("\n");

	if (mode & SPI_TX_QUAD)
		xfer[1].tx_nbits = 4;
	else if (mode & SPI_TX_DUAL)
		xfer[1].tx_nbits = 2;
	if (mode & SPI_RX_QUAD)
		xfer[1].rx_nbits = 4;
	else if (mode & SPI_RX_DUAL)
		xfer[1].rx_nbits = 2;
	if (!(mode & SPI_LOOP)) {
		if (mode & (SPI_TX_QUAD | SPI_TX_DUAL))
			xfer[1].rx_buf = 0;
		else if (mode & (SPI_RX_QUAD | SPI_RX_DUAL))
			xfer[1].tx_buf = 0;
	}

	ret = ioctl(i2s_fd, SPI_IOC_MESSAGE(2), xfer);
	if (ret < 1) {
		pabort("write: can't send spi message");
		return 1;
	}
	return 0;
}

const struct backend_ops spi_backend_ops = {
	.open = spi_open,
	.read = spi_read,
	.write = spi_write,
};
