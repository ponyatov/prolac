#define PIPE
#define UNTIMER1	1
#define UNTIMER2	1

#define SKB_TO_TCPH(s)	   (((Tcp_Header *)(s)->h.th))
#define SEGMENT_TO_TCPH(s) ((s)->_tcp_header)
#define SEGMENT_TO_ETHERH(s) (&(s)->ether)

#include <linux/skbuff.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/interrupt.h>
#include <linux/netdevice.h>
#include <linux/random.h>
#include <asm/atomic.h>

#include <asm/byteorder.h>
#include "cksum.h"

#define atomic_inc(x) 
#define atomic_dec(x) 

#undef PDEBUG
#ifdef PRINT
# ifdef __KERNEL__
#  define PDEBUG(fmt, args...) printk("<1>"fmt,##args)
# else
#  define PDEBUG(fmt, args...) printf(fmt,##args)
# endif
#else
# define PDEBUG(fmt, args...)
#endif

#define assert(expr) ((void) 0)

#define NPORT           10
#define ANYPORT         0

struct demux {
    int myport;
    int rport;
    Tcp_Tcb *tcb;
};

int prolac_sock_snd_len;

static struct demux table[NPORT];

static Tcp_Tcb *
tcb_demultiplex(Tcp_Header *h)
{
  int i;
  Tcp_Tcb *t;
  unsigned short dport = __ntohs(h->dport);
  unsigned short sport = __ntohs(h->sport);
  
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

void
print_tcp_options(struct tcphdr *tcp) 
{
  unsigned char *data, n, l;

  if (tcp->doff <= 5) {
    return;
  }

  n = 20; l = 0;
  data = (unsigned char *)(tcp+1);
  do {
    data += l;
    switch(*data) {
    case 0:
      PDEBUG("EOL\n");
      break;
    case 1:
      PDEBUG("NOP\n");
      break;
    case 2:
      PDEBUG("MSS len=%d mss=%hd\n", 
	     data[1], 
	     __ntohs(*((unsigned short *)(data+2))));
      break;
    case 3:
      PDEBUG("WSCALE len=%d scale=%d\n", data[1], data[2]);
      break;
    case 8:
      PDEBUG("TSTAMP len=%d stmp=%ld echo=%ld\n", 
	     data[1], 
	     __ntohl(*(unsigned long *)(data+2)),
	     __ntohl(*(unsigned long *)(data+6)));
      break;
    default:
      PDEBUG("ERROR!\n");
    }
    l = data[1];
    n += l;
  } while (*data && (n < tcp->doff * 4));
  
}

void
print_iphdr(struct iphdr *ip) 
{

  char format[] = 
"ip->ihl = %-10d
ip->version = %-10d  ip->tos = %x
ip->tot_len = %-10hu
ip->id = %-10hu       ip->frag_off = %hu
ip->ttl = %-10d      ip->protocol = %x
ip->saddr = %-10x    ip->daddr = %x
";

#ifndef PRINT
    return;
#endif

  printk(format,
	 ip->ihl,
	 ip->version,
	 ip->tos,
	 __ntohs(ip->tot_len),
	 __ntohs(ip->id),
	 __ntohs(ip->frag_off),
	 ip->ttl,
	 ip->protocol,
	 __ntohl(ip->saddr),
	 __ntohl(ip->daddr));
}

#define PRINT_TCPHDR_RCV 0
#define PRINT_TCPHDR_SND 1
#define PRINT_TCPHDR_XXX 2
void
print_tcphdr(struct tcphdr *tcp, struct iphdr *ip, int variety, unsigned int irs, unsigned int iss) 
{
#ifdef PRINT
  unsigned int seqno = __ntohl(tcp->seq);
  unsigned int ackno = __ntohl(tcp->ack_seq);
  switch (variety) {
   case PRINT_TCPHDR_RCV:
    if (seqno != irs) seqno -= irs;
    if (ackno != iss) ackno -= iss;
    printk("<< ");
    break;
   case PRINT_TCPHDR_SND:
    if (seqno != iss) seqno -= iss;
    if (ackno != irs) ackno -= irs;
    printk(">> ");
    break;
   default:
    printk("?? ");
  }
  if (tcp->syn) printk("S");
  if (tcp->fin) printk("F");
  if (tcp->psh) printk("P");
  if (ip) printk("%u:%u", seqno, seqno + __ntohs(ip->tot_len) - tcp->doff);
  else printk("%u:?", seqno);
  if (tcp->ack) printk(" ack %u", ackno);
  printk(" win %u", __ntohs(tcp->window));
  printk("\n");
#endif

#if 0
  char format[] =
"
source = %-10hu  dest = %hu
seq = %-10u     ack_seq = %u
data_offset = %-10d
window = %hu 
check = %-10hu  urg_ptr = %hu
flags = %s%s%s%s%s%s
";


  printk(format,
	 __ntohs(tcp->source),
	 __ntohs(tcp->dest),
	 __ntohl(tcp->seq),
	 __ntohl(tcp->ack_seq),
	 tcp->doff,
	 __ntohs(tcp->window),
	 tcp->check,
	 __ntohs(tcp->urg_ptr),
	 tcp->fin ? "fin " : "",
	 tcp->syn ? "syn " : "",
	 tcp->rst ? "rst " : "",
	 tcp->psh ? "psh " : "",
	 tcp->ack ? "ack " : "",
	 tcp->urg ? "urg " : ""
	 );
  print_tcp_options(tcp);
  return;
#endif
}
static atomic_t skb_count = 0;

static int global_port;
unsigned long flags;

static char *state_names[] = {
  "closed", "listen", "syn-sent", "syn-received",
  "established", "close-wait", "fin-wait-1",
  "closing", "last-ack", "fin-wait-2", "time-wait"
};


static void
tcp_init(int c)
{
  tcb_init();
  global_port = 3000;
}


static void
tcp_send_segment(Segment *seg)
{
  struct sk_buff *skb = (struct sk_buff *)seg;
  int j;

  skb->len = skb->tail - skb->data;
  ip_queue_xmit((void *)0, skb->dev, skb, 1);

  //save_flags(flags);
  //cli();
  //  skb_count--;

  //atomic_dec(&skb_count);
  //printk("Handed skbuff to IP. Effectively freeing...(%d)\n", skb_count);
  PDEBUG("OUTGOING segment... in host order, after send. (skb_count = %d)\n", skb_count);
  //print_iphdr(skb->ip_hdr);
  print_tcphdr(skb->h.th, skb->ip_hdr, PRINT_TCPHDR_SND, 0, 0);
  //restore_flags(flags);

#ifdef SLOWDOWN  
  // Slow it down!
  j = jiffies + HZ;
  while (jiffies < j);
#endif
  
}


void
pretty_print_tcp_header(Tcp_Header *tcp, int packetlen, int dir)
{
}

void
tcp_send_all_penders(void)
{
}

static Segment *
tcp_poll(Tcp_Tcb **tcbp)
{
  return 0;
}

int
overlaps(void *begin1, void *end1, void *begin2, void *end2) {
  return ((begin1 >= begin2 && begin1 <= end2) 
    || (begin2 >= begin1 && begin2 <= end1)
    || (end1 >= begin2 && end1 <= end2)
    || (end2 >= begin1 && end2 <= end1));
} 

int overlaps_skb(struct sk_buff *sk1, struct sk_buff *sk2) {
  return overlaps(sk1, sk1+1, sk2, sk2+1)
    || overlaps(sk1, sk1+1, sk2->head, sk2->end)
    || overlaps(sk1->head, sk1->end, sk2, sk2+1)
    || overlaps(sk1->head, sk1->end, sk2->head, sk2->end);
}

void
test_chain(struct sk_buff *sentinel, struct sk_buff *test, int include_sentinel, char *what) 
{
  struct sk_buff *ptr = (include_sentinel ? sentinel : sentinel->next);
  if (include_sentinel) goto fudsainbkj;
  while (sentinel != ptr) {

    fudsainbkj:
    if (!ptr) {
      printk("broken next pointer\n");
      break;
    }

    if (overlaps_skb(ptr, test)) {
      printk("test %s sk_buff %p overlaps %p!", what, ptr, test);
    }

    if (!ptr->prev) {
      printk("broken prev pointer.\n");
    }
    ptr = ptr->next;
  }
}


void
test_skb(struct sk_buff *s)
{
  int i;
  for (i = 0; i < NPORT; i++) {
    if (table[i].tcb) {
      test_chain((struct sk_buff *)(table[i].tcb), s, 0, "tcb");
      test_chain((struct sk_buff *)&(table[i].tcb->_socket->rcv_buf), s, 0, "receive");
      test_chain((struct sk_buff *)&(table[i].tcb->_socket->snd_buf), s, 0, "send");
    }
  }
}
