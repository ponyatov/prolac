#define PIPE
#define UNTIMER1	1
#define UNTIMER2	1

#define SKB_TO_TCPH(s)	   (((Tcp_Header *)(s)->h.th))
#define SEGMENT_TO_TCPH(s) ((s)->_tcp_header)
#define SEGMENT_TO_ETHERH(s) (&(s)->ether)

#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/tcp.h>

#ifndef __KERNEL__
#include <stdio.h>
#include <malloc.h>
#include <signal.h>

#include <sys/errno.h>
#include <sys/stat.h>
#endif

#define ushort SYS_ushort
#define uint SYS_uint
#define ulong SYS_ulong

#include <assert.h>

#ifdef __linux
#include <asm/byteorder.h>
#endif

#ifdef __linux__
#include <endian.h>
#else
#include <sys/ioccom.h>
#include <machine/endian.h>
#include <machine/cpu.h>
#include <machine/pctr.h>
#endif

#include "cksum.h"

#undef ushort
#undef uint
#undef ulong

#define NBIN            14


#undef PDEBUG
#ifdef PRINT
# ifdef __KERNEL__
#  define PDEBUG(fmt, args...) printk(KERN_DEBUG "prolac: " fmt, ## args)
# else
#  define PDEBUG(fmt, args...) PDEBUG(fmt, ## args)
# endif
#else
# define PDEBUG(fmt, args...)
#endif

#ifndef __KERNEL__

/*
 * Define kernel functions here for user-level testing.
 */

#define GFP_KERNEL   0

void *
kmalloc(unsigned int size, int priority)
{
  return malloc(size);
}

void 
kfree(void * obj)
{
  free(obj);
}


static unsigned long flags;

void
save_flags(unsigned long flags)
{
}

void
cli() 
{
}

void
restore_flags(unsigned long flags) 
{
}


unsigned char *
skb_put(struct sk_buff *skb, int len)
{
	unsigned char *tmp=skb->tail;
	skb->tail+=len;
	skb->len+=len;
	assert(skb->tail<=skb->end);
	return tmp;
}

struct sk_buff *
alloc_skb(unsigned int size,int priority)
{
	struct sk_buff *skb;
	int len=size;
	unsigned char *bptr;

	size=(size+15)&~15;		/* Allow for alignments. Make a multiple of 16 bytes */
	size+=sizeof(struct sk_buff);	/* And stick the control itself on the end */
	
	/*
	 *	Allocate some space
	 */
	 
	bptr=(unsigned char *)malloc(size);
	if (bptr == NULL)
	{
		return NULL;
	}
	memset(bptr, 0, size);
	
	skb=(struct sk_buff *)(bptr+size)-1;

	skb->count = 1;		/* only one reference to this */
	skb->data_skb = NULL;	/* and we're our own data skb */

	skb->free = 2;	/* Invalid so we pick up forgetful users */
	skb->lock = 0;
	skb->pkt_type = PACKET_HOST;	/* Default type */
	skb->pkt_bridged = 0;		/* Not bridged */
	skb->prev = skb->next = skb->link3 = NULL;
	skb->list = NULL;
	skb->sk = NULL;
	skb->truesize=size;
	skb->localroute=0;
	skb->stamp.tv_sec=0;	/* No idea about time */
	skb->localroute = 0;
	skb->ip_summed = 0;
	memset(skb->proto_priv, 0, sizeof(skb->proto_priv));
	skb->users = 0;

	/* Load the data pointers */
	skb->head=bptr;
	skb->data=bptr;
	skb->tail=bptr;
	skb->end=bptr+len;
	skb->len=0;
	skb->destructor=NULL;
	return skb;
}

void			
kfree_skb(struct sk_buff *skb, int rw)
{
  free(skb->head);
}

int ip_build_header(struct sk_buff *skb, __u32 saddr, __u32 daddr,
		void **dev, int type, void *opt,
		int len, int tos, int ttl, void ** rp)
{
	struct iphdr *iph;

	/*
	 *	Book keeping
	 */

	skb->saddr = saddr;
	

	/*
	 *	Build the IP addresses
	 */
	 
	iph=(struct iphdr *)skb_put(skb,sizeof(struct iphdr));

	iph->version  = 4;
	iph->ihl      = 5;
	iph->tos      = tos;
	iph->frag_off = 0;
	iph->ttl      = ttl;
	iph->daddr    = daddr;
	iph->saddr    = saddr;
	iph->protocol = type;
	skb->ip_hdr   = iph;

	return sizeof(struct iphdr);
}

#endif /* ifndef __KERNEL__ */


static int client;
static int rd_msg, wt_msg, rd_reply, wt_reply;

static int global_port;

int tv_poll_count;
double tv_poll_sum;
double tv_poll_sqsum;

extern int is_server;
extern char *program_name; 
static char *state_names[] = {
  "closed", "listen", "syn-sent", "syn-received",
  "established", "close-wait", "fin-wait-1",
  "closing", "last-ack", "fin-wait-2", "time-wait"
};



static volatile int timeout;

void
signal_timeout(int sig)
{
  timeout++;
}


static void
tcp_init(int c)
{
    struct sigaction sa;
    struct itimerval itval;

    client = c;

    rd_msg = 3;
    wt_msg = 4;
    rd_reply = 5;
    wt_reply = 6;
    
    
    tcb_init();
    setvbuf(stdout, 0, _IONBF, 0);
    
    sa.sa_handler = signal_timeout;
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGALRM, &sa, 0)) {
	perror("sigaction");
	exit(-1);
    }
    
    itval.it_interval.tv_sec = 0;
    itval.it_interval.tv_usec = 250000;
    itval.it_value = itval.it_interval;
    setitimer(ITIMER_REAL, &itval, 0);
    
    srandom(getpid());
    
    global_port = (getpid() << 9) | random();
}


static void tcp_actually_send_segment(Segment *);
Segment *segment_buffer[10];

void
pretty_print_tcp_header(Tcp_Header *tcp, int packetlen, int dir)
{
  char *dirstr = dir == 0 ? "<-" : (dir == 1 ? "->" : "::");
  seqint seqno = dir == 2 ? tcp->seqno : ntohl(tcp->seqno);
  seqint ackno = dir == 2 ? tcp->ackno : ntohl(tcp->ackno);
  unsigned short win = dir == 2 ? tcp->window : ntohs(tcp->window);
  PDEBUG("   ** %s %s (%u+%u) win(%u)", program_name, dirstr, seqno,
	 packetlen, (unsigned)win);
  if (tcp->fv & 2) PDEBUG(" syn");
  if (tcp->fv & 4) PDEBUG(" rst");
  if (tcp->fv & 16) PDEBUG(" ack(%u)", ackno);
  if (tcp->fv & 1) PDEBUG(" fin");
  if (tcp->fv & 8) PDEBUG(" psh");
  if (tcp->fv & 32) PDEBUG(" urg");
  PDEBUG("\n");
}


void
tcp_send_all_penders(void)
{
  int i;
  for (i = 0; i < 10; i++)
    if (segment_buffer[i]) {
      tcp_actually_send_segment(segment_buffer[i]);
      segment_buffer[i] = 0;
    }
}

static void
tcp_send_segment(Segment *seg)
{
  int i = (rand() >> 5) % 10;
#ifdef DROP
  if (i > 7) {
    PDEBUG(" %s DROPPING\n", program_name);
    pretty_print_tcp_header(SEGMENT_TO_TCPH(seg), sz, 3); 
    return;
  }
#endif
#ifdef REORDER
  if (segment_buffer[i])
    tcp_actually_send_segment(segment_buffer[i]);
  segment_buffer[i] = seg;
#else
  tcp_actually_send_segment(seg);
#endif
}

static int total_time;

void
print_iphdr(struct iphdr *ip) 
{

  char format[] = 
"ip->ihl = %d
ip->version = %d
ip->tos = %x
ip->tot_len = %hu
ip->id = %hu
ip->frag_off = %hu
ip->ttl = %d
ip->protocol = %x
ip->check = %hu
ip->saddr = %x
ip->daddr = %x
";
  PDEBUG(format,
	 ip->ihl,
	 ip->version,
	 ip->tos,
	 ip->tot_len,
	 ip->id,
	 ip->frag_off,
	 ip->ttl,
	 ip->protocol,
	 ip->check,
	 ip->saddr,
	 ip->daddr);
}

void
print_tcphdr(struct tcphdr *tcp) 
{
  char format[] =
"
source = %hu;
dest = %hu;
seq = %u;
ack_seq = %u;
data_offset = %d
window = %hu
check = %hu;
urg_ptr = %hu
";

  PDEBUG(format,
	 tcp->source,
	 tcp->dest,
	 tcp->seq,
	 tcp->ack_seq,
	 tcp->doff,
	 tcp->window,
	 tcp->check,
	 tcp->urg_ptr);
}


static int
do_write(int fd, void *datav, int amt)
{
  char *data = (char *)datav;
  int have_wrote = 0;
  while (amt > 0) {
    int this_wrote;
    this_wrote = write(fd, data, amt);
    if (this_wrote == -1 && errno != EINTR)
      return -1;
    if (this_wrote > -1) {
      have_wrote += this_wrote;
      data += this_wrote;
      amt -= this_wrote;
    }
  }
  return have_wrote;
}


static void
tcp_actually_send_segment(Segment *seg)
{
  struct iphdr *iph = (struct iphdr *)seg->_data;
  int r;
  int fd = client ? wt_msg : wt_reply;
  int s = seg->_tail - seg->_data;

  iph->tot_len = s;

#ifdef PRINT
  pretty_print_tcp_header(SEGMENT_TO_TCPH(seg), s, 1);
  print_iphdr(iph);
  print_tcphdr((struct tcphdr *)seg->_tcp_header);
#endif
  
  if ((r = do_write(fd, &s, sizeof(int))) < 0) {
    perror("tcp_send_data_segment: size");
    exit(1);
  }
  
  if ((r = do_write(fd, seg->_data, s)) < 0) {
    perror("tcp_send_data_segment: data");
    exit(1);
  }
}


static int
do_read(int fd, void *datav, int amt)
{
  char *data = (char *)datav;
  int have_read = 0, this_read;
  while (amt > 0) {
    this_read = read(fd, data, amt);
    if (this_read == -1 && errno != EINTR)
      return -1;
    if (this_read > -1) {
      have_read += this_read;
      data += this_read;
      amt -= this_read;
    }
  }
  return have_read;
}


static Segment *
tcp_poll(Tcp_Tcb **tcbp)
{
  int fd = client ? rd_reply : rd_msg;
  char *message = 0;
  struct sk_buff *skb; 
  Tcp_Tcb *tcb;
  int size;
  int r;
  int nbytes;
  int i;
  
  if (timeout > total_time) {
    fast_timeout__Tcp_Interface();
    if (timeout > (total_time | 1))
      slow_timeout__Tcp_Interface();
    total_time = timeout;
  }
  
  {
    struct sigaction sa;
    sa.sa_handler = signal_timeout;
    sa.sa_flags = 0;
    sigaction(SIGALRM, &sa, 0);
    r = read(fd, &nbytes, sizeof(nbytes));
    sa.sa_flags = SA_RESTART;
    sigaction(SIGALRM, &sa, 0);
  }
  
  if (r <= 0) {
    return 0;
  }
  
  if (r < 4) {
    PDEBUG("%s exit\n", program_name);
    exit(0);
  }
   
  assert(r == sizeof(nbytes));
  assert(nbytes >= sizeof(Tcp_Header));
  
  skb = alloc_skb(nbytes, 0);
  assert(skb);
  
  r = do_read(fd, skb_put(skb, nbytes), nbytes);

  if (r < sizeof(Tcp_Header)) PDEBUG("ONLY READ %d\n", r);
  assert(r >= sizeof(Tcp_Header));

  PDEBUG("Read %d of %d promised bytes into packet.\n", r, nbytes);
  skb->ip_hdr = (void *)skb->data;
  skb->h.raw = (void *)skb->data + skb->ip_hdr->ihl*4;
#ifdef PRINT
  pretty_print_tcp_header(SKB_TO_TCPH(skb), r, 0);
  print_iphdr(skb->ip_hdr);
  print_tcphdr(skb->h.th);
#endif
  
  tcb = tcb_demultiplex(SKB_TO_TCPH(skb));
  assert(tcb);
  
  *tcbp = tcb;
  return (Segment *)skb;
}

#define NPORT           10
#define ANYPORT         0

struct demux {
    int myport;
    int rport;
    Tcp_Tcb *tcb;
};

static struct demux table[NPORT];

static Tcp_Tcb *
tcb_demultiplex(Tcp_Header *h)
{
  int i;
  Tcp_Tcb *t;
  unsigned short dport = ntohs(h->dport);
  unsigned short sport = ntohs(h->sport);
  
  // XXX Use DPF
  t = 0;
  for (i = 0; i < NPORT; i++) {
    if (table[i].myport == dport) {
      // rport == ANYPORT means any source port will match
      if (table[i].rport == sport) return table[i].tcb;
      else if (table[i].rport == ANYPORT) t = table[i].tcb;
    }
  }
  return t;
}


static void
tcb_register(int mport, int rport, Tcp_Tcb *tcb)
{
    int i;

    for (i = 0; i < NPORT; i++) {
	if (table[i].tcb == 0) {
	    table[i].myport = mport;
	    table[i].rport = rport;
	    table[i].tcb = tcb;
	    return;
	}
    }
    assert(0);
}

static void
tcb_unregister(Tcp_Tcb *tcb)
{
    int i;

    for (i = 0; i < NPORT; i++) {
	if (table[i].tcb == tcb) {
	    table[i].rport = -1;
	    table[i].tcb = 0;
	    return;
	}
    }
    assert(0);
}


static void
tcb_init(void)
{
    int i;

    for (i = 0; i < NPORT; i++) {
        table[i].rport = -1;
	table[i].tcb = 0;
    }
}



