#include "atcp.h"
#define MODULE
#include <linux/module.h>

#ifdef CONFIG_PROC_FS
#include <linux/proc_fs.h>

extern int get__netinfo(struct proto *, char *, int, char **, off_t, int);

int
atcp_get_info(char *buffer, char **start, off_t offset, int length, int dummy)
{
	return get__netinfo(&atcp_prot, buffer,0, start, offset, length);
}

int
atcp_snmp_get_info(char *buffer, char **start, off_t offset, int length, int dummy)
{
	int len;

	len = sprintf (buffer,
		"ATcp: RtoAlgorithm RtoMin RtoMax MaxConn ActiveOpens PassiveOpens AttemptFails EstabResets CurrEstab InSegs OutSegs RetransSegs InErrs OutRsts\n"
		"ATcp: %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu\n",
		    atcp_statistics.TcpRtoAlgorithm, atcp_statistics.TcpRtoMin,
		    atcp_statistics.TcpRtoMax, atcp_statistics.TcpMaxConn,
		    atcp_statistics.TcpActiveOpens, atcp_statistics.TcpPassiveOpens,
		    atcp_statistics.TcpAttemptFails, atcp_statistics.TcpEstabResets,
		    atcp_statistics.TcpCurrEstab, atcp_statistics.TcpInSegs,
		    atcp_statistics.TcpOutSegs, atcp_statistics.TcpRetransSegs,
		    atcp_statistics.TcpInErrs, atcp_statistics.TcpOutRsts);
	
	if (offset >= len)
	{
		*start = buffer;
		return 0;
	}
	*start = buffer + offset;
	len -= offset;
	if (len > length)
		len = length;
	if (len < 0)
		len = 0; 
	return len;
}

static struct proc_dir_entry proc_net_atcp = {
	0, 4, "atcp",
	S_IFREG | S_IRUGO, 1, 0, 0,
	0, &proc_net_inode_operations,
	atcp_get_info
};

static struct proc_dir_entry proc_net_atcp_snmp = {
	0, 9, "atcp_snmp",
	S_IFREG | S_IRUGO, 1, 0, 0,
	0, &proc_net_inode_operations,
	atcp_snmp_get_info
};
#endif

extern struct net_proto_family inet_family_ops;

static int atcp_rcv_drop(struct sk_buff *skb, unsigned short len)
{
  /* drop everything we receive while shutting down; pray for retransmission */
  kfree_skb(skb);
  return 0;
}

extern int
init_module(void)
{
  atcp_v4_init(&inet_family_ops);
  alternate_tcp_prot = &atcp_prot;
  alternate_tcp_rcv = &atcp_v4_rcv;
  alternate_tcp_err = &atcp_v4_err;
#ifdef CONFIG_PROC_FS
  proc_net_register(&proc_net_atcp);
  proc_net_register(&proc_net_atcp_snmp);
#endif
  return 0;
}

extern void
cleanup_module(void)
{
#ifdef CONFIG_PROC_FS
  proc_net_unregister(proc_net_atcp.low_ino);
  proc_net_unregister(proc_net_atcp_snmp.low_ino);
#endif
  alternate_tcp_prot = 0;	/* don't add any new sockets */
  alternate_tcp_rcv = atcp_rcv_drop; /* drop unknown packets while we shift
				       our sockets into normal ones */
  if (atcp_slow_timer.prev)
    del_timer(&atcp_slow_timer);
  atcp_v4_cleanup();
  alternate_tcp_rcv = 0;	/* now, all our sockets should be gone */
  alternate_tcp_err = 0;
}
