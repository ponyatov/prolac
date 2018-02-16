#ifdef PRINT
# define PDEBUG(fmt, args...) printk("<1>"fmt,##args)
#else
# define PDEBUG(fmt, args...)
#endif

#define assert(expr) ((void) 0)

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
	     ntohs(*((unsigned short *)(data+2))));
      break;
    case 3:
      PDEBUG("WSCALE len=%d scale=%d\n", data[1], data[2]);
      break;
    case 8:
      PDEBUG("TSTAMP len=%d stmp=%ld echo=%ld\n", 
	     data[1], 
	     ntohl(*(unsigned long *)(data+2)),
	     ntohl(*(unsigned long *)(data+6)));
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
"ip->ihl = %-10d\n\
ip->version = %-10d  ip->tos = %x\n\
ip->tot_len = %-10hu\n\
ip->id = %-10hu       ip->frag_off = %hu\n\
ip->ttl = %-10d      ip->protocol = %x\n\
ip->saddr = %-10x    ip->daddr = %x\n\
";

#ifndef PRINT
    return;
#endif

  printk(format,
	 ip->ihl,
	 ip->version,
	 ip->tos,
	 ntohs(ip->tot_len),
	 ntohs(ip->id),
	 ntohs(ip->frag_off),
	 ip->ttl,
	 ip->protocol,
	 ntohl(ip->saddr),
	 ntohl(ip->daddr));
}

#define PRINT_TCPHDR_RCV 0
#define PRINT_TCPHDR_SND 1
#define PRINT_TCPHDR_XXX 2
void
print_tcphdr(struct tcphdr *tcp, struct iphdr *ip, int variety, unsigned int irs, unsigned int iss) 
{
#ifdef PRINT
  unsigned int seqno = ntohl(tcp->seq);
  unsigned int ackno = ntohl(tcp->ack_seq);
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
  if (ip) printk("%u:%u", seqno, seqno + ntohs(ip->tot_len) - tcp->doff);
  else printk("%u:?", seqno);
  if (tcp->ack) printk(" ack %u", ackno);
  printk(" win %u", ntohs(tcp->window));
  printk("\n");
#endif

#if 0
  char format[] =
"\n\
source = %-10hu  dest = %hu\n\
seq = %-10u     ack_seq = %u\n\
data_offset = %-10d\n\
window = %hu \n\
check = %-10hu  urg_ptr = %hu\n\
flags = %s%s%s%s%s%s\n\
";


  printk(format,
	 ntohs(tcp->source),
	 ntohs(tcp->dest),
	 ntohl(tcp->seq),
	 ntohl(tcp->ack_seq),
	 tcp->doff,
	 ntohs(tcp->window),
	 tcp->check,
	 ntohs(tcp->urg_ptr),
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

static char *state_names[] = {
  "closed", "listen", "syn-sent", "syn-received",
  "established", "close-wait", "fin-wait-1",
  "closing", "last-ack", "fin-wait-2", "time-wait"
};
