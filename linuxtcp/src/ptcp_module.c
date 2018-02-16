#include "ptcp.h"
#include "ptcp_prolac.h"
#include <linux/module.h>

#ifdef CONFIG_PROC_FS
#include <linux/proc_fs.h>

extern int get__netinfo(struct proto *, char *, int, char **, off_t, int);

int
ptcp_get_info(char *buffer, char **start, off_t offset, int length, int dummy)
{
	return get__netinfo(&ptcp_prot, buffer,0, start, offset, length);
}

int
ptcp_snmp_get_info(char *buffer, char **start, off_t offset, int length, int dummy)
{
	int len;

	len = sprintf (buffer,
		"PTcp: RtoAlgorithm RtoMin RtoMax MaxConn ActiveOpens PassiveOpens AttemptFails EstabResets CurrEstab InSegs OutSegs RetransSegs InErrs OutRsts\n"
		"PTcp: %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu\n",
		    ptcp_statistics.TcpRtoAlgorithm, ptcp_statistics.TcpRtoMin,
		    ptcp_statistics.TcpRtoMax, ptcp_statistics.TcpMaxConn,
		    ptcp_statistics.TcpActiveOpens, ptcp_statistics.TcpPassiveOpens,
		    ptcp_statistics.TcpAttemptFails, ptcp_statistics.TcpEstabResets,
		    ptcp_statistics.TcpCurrEstab, ptcp_statistics.TcpInSegs,
		    ptcp_statistics.TcpOutSegs, ptcp_statistics.TcpRetransSegs,
		    ptcp_statistics.TcpInErrs, ptcp_statistics.TcpOutRsts);
	
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

static struct proc_dir_entry proc_net_ptcp = {
	0, 4, "ptcp",
	S_IFREG | S_IRUGO, 1, 0, 0,
	0, &proc_net_inode_operations,
	ptcp_get_info
};

static struct proc_dir_entry proc_net_ptcp_snmp = {
	0, 9, "ptcp_snmp",
	S_IFREG | S_IRUGO, 1, 0, 0,
	0, &proc_net_inode_operations,
	ptcp_snmp_get_info
};
#endif

extern struct net_proto_family inet_family_ops;

static int ptcp_rcv_drop(struct sk_buff *skb, unsigned short len)
{
  /* drop everything we receive while shutting down; pray for retransmission */
  kfree_skb(skb);
  return 0;
}

extern int
init_module(void)
{
  ptcp_v4_init(&inet_family_ops);
  alternate_tcp_prot = &ptcp_prot;
  alternate_tcp_rcv = &receive_segment__Base__Input;
  alternate_tcp_err = &ptcp_v4_err;
#ifdef CONFIG_PROC_FS
  proc_net_register(&proc_net_ptcp);
  proc_net_register(&proc_net_ptcp_snmp);
#endif
  return 0;
}

extern void
cleanup_module(void)
{
#ifdef CONFIG_PROC_FS
  proc_net_unregister(proc_net_ptcp.low_ino);
  proc_net_unregister(proc_net_ptcp_snmp.low_ino);
#endif
  alternate_tcp_prot = 0;	/* don't add any new sockets */
  alternate_tcp_rcv = ptcp_rcv_drop; /* drop unknown packets while we shift
				       our sockets into normal ones */
  if (ptcp_slow_timer.prev)
    del_timer(&ptcp_slow_timer);
  ptcp_v4_cleanup();
  alternate_tcp_rcv = 0;	/* now, all our sockets should be gone */
  alternate_tcp_err = 0;
}
