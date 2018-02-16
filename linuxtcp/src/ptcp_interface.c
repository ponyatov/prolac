/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		Implementation of the Transmission Control Protocol(TCP).
 *
 * Version:	$Id: ptcp_interface.c,v 1.1 1999/09/05 04:49:50 eddietwo Exp $
 *
 *		IPv4 specific functions
 *
 *
 *		code split from:
 *		linux/ipv4/tcp.c
 *		linux/ipv4/tcp_input.c
 *		linux/ipv4/tcp_output.c
 *
 *		See tcp.c for author information
 *
 *	This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

/*
 * Changes:
 *		David S. Miller	:	New socket lookup architecture.
 *					This code is dedicated to John Dyson.
 *		David S. Miller :	Change semantics of established hash,
 *					half is devoted to TIME_WAIT sockets
 *					and the rest go in the other half.
 *		Andi Kleen :		Add support for syncookies and fixed
 *					some bugs: ip options weren't passed to
 *					the TCP layer, missed a check for an ACK bit.
 *		Andi Kleen :		Implemented fast path mtu discovery.
 *	     				Fixed many serious bugs in the
 *					open_request handling and moved
 *					most of it into the af independent code.
 *					Added tail drop and some other bugfixes.
 *					Added new listen sematics.
 *		Mike McLagan	:	Routing by source
 *	Juan Jose Ciarlante:		ip_dynaddr bits
 *		Andi Kleen:		various fixes.
 *	Vitaly E. Lavrov	:	Transparent proxy revived after year coma.
 *	Andi Kleen		:	Fix new listen.
 *	Andi Kleen		:	Fix accept error reporting.
 */

#include <linux/config.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/random.h>
#include <linux/init.h>
#include <linux/ipsec.h>

#include <net/icmp.h>
#include "ptcp.h"
#include "ptcp_prolac.h"
#include <net/ipv6.h>

#include <asm/segment.h>

#include <linux/inet.h>
#include <linux/stddef.h>

#define tcp_v4_get_port		(tcp_prot.get_port)

extern int sysctl_tcp_timestamps;
extern int sysctl_tcp_window_scaling;
extern int sysctl_tcp_sack;
extern int sysctl_tcp_syncookies;
extern int sysctl_ip_dynaddr;
extern __u32 sysctl_wmem_max;
extern __u32 sysctl_rmem_max;

/* Check TCP sequence numbers in ICMP packets. */
#define ICMP_MIN_LENGTH 8

/* Socket used for sending RSTs */ 	
struct inode ptcp_inode;
struct socket *ptcp_socket=&ptcp_inode.u.socket_i;

static void ptcp_v4_send_reset(struct sk_buff *skb);

void ptcp_v4_send_check(struct sock *sk, struct tcphdr *th, int len, 
		       struct sk_buff *skb);

/* This is for sockets with full identity only.  Sockets here will always
 * be without wildcards and will have the following invariant:
 *          TCP_ESTABLISHED <= sk->state < TCP_CLOSE
 *
 * First half of the table is for sockets not in TIME_WAIT, second half
 * is for TIME_WAIT sockets only.
 */
struct sock *ptcp_established_hash[TCP_HTABLE_SIZE];

/* Ok, let's try this, I give up, we do need a local binding
 * TCP hash as well as the others for fast bind/connect.
 */
/* struct tcp_bind_bucket *tcp_bound_hash[TCP_BHTABLE_SIZE]; */

/* All sockets in TCP_LISTEN state will be in here.  This is the only table
 * where wildcard'd TCP sockets can exist.  Hash function here is just local
 * port number.
 */
struct sock *ptcp_listening_hash[TCP_LHTABLE_SIZE];

/* Register cache. */
struct sock *ptcp_regs[TCP_NUM_REGS];

/*
 * This array holds the first and last local port number.
 * For high-usage systems, use sysctl to change this to
 * 32768-61000
 */
extern int sysctl_local_port_range[2]; /* = { 1024, 4999 } */

static __inline__ int ptcp_hashfn(__u32 laddr, __u16 lport,
				 __u32 faddr, __u16 fport)
{
	return ((laddr ^ lport) ^ (faddr ^ fport)) & ((TCP_HTABLE_SIZE/2) - 1);
}

static __inline__ int ptcp_sk_hashfn(struct sock *sk)
{
	__u32 laddr = sk->rcv_saddr;
	__u16 lport = sk->num;
	__u32 faddr = sk->daddr;
	__u16 fport = sk->dport;

	return ptcp_hashfn(laddr, lport, faddr, fport);
}

static __inline__ void __ptcp_v4_hash(struct sock *sk)
{
	struct sock **skp;

	if(sk->state == TCP_LISTEN)
		skp = &ptcp_listening_hash[tcp_sk_listen_hashfn(sk)];
	else
		skp = &ptcp_established_hash[(sk->hashent = ptcp_sk_hashfn(sk))];

	if((sk->next = *skp) != NULL)
		(*skp)->pprev = &sk->next;
	*skp = sk;
	sk->pprev = skp;
}

static void ptcp_v4_hash(struct sock *sk)
{
	if (sk->state != TCP_CLOSE) {
		SOCKHASH_LOCK();
		__ptcp_v4_hash(sk);
		SOCKHASH_UNLOCK();
	}
}

static void ptcp_v4_unhash(struct sock *sk)
{
	SOCKHASH_LOCK();
	if(sk->pprev) {
		if(sk->next)
			sk->next->pprev = sk->pprev;
		*sk->pprev = sk->next;
		sk->pprev = NULL;
		ptcp_reg_zap(sk);
		if (sk->prev)	/* special case: switching over to normal TCP -- don't want to remove from port hash */
			__tcp_put_port(sk);
	}
	SOCKHASH_UNLOCK();
}

/* Don't inline this cruft.  Here are some nice properties to
 * exploit here.  The BSD API does not allow a listening TCP
 * to specify the remote port nor the remote address for the
 * connection.  So always assume those are both wildcarded
 * during the search since they can never be otherwise.
 */
static struct sock *ptcp_v4_lookup_listener(u32 daddr, unsigned short hnum, int dif)
{
	struct sock *sk;
	struct sock *result = NULL;
	int score, hiscore;

	hiscore=0;
	for(sk = ptcp_listening_hash[tcp_lhashfn(hnum)]; sk; sk = sk->next) {
		if(sk->num == hnum) {
			__u32 rcv_saddr = sk->rcv_saddr;

			score = 1;
			if(rcv_saddr) {
				if (rcv_saddr != daddr)
					continue;
				score++;
			}
			if (sk->bound_dev_if) {
				if (sk->bound_dev_if != dif)
					continue;
				score++;
			}
			if (score == 3)
				return sk;
			if (score > hiscore) {
				hiscore = score;
				result = sk;
			}
		}
	}
	return result;
}

/* Sockets in TCP_CLOSE state are _always_ taken out of the hash, so
 * we need not check it for TCP lookups anymore, thanks Alexey. -DaveM
 * It is assumed that this code only gets called from within NET_BH.
 */
static inline struct sock *__ptcp_v4_lookup(struct tcphdr *th,
					   u32 saddr, u16 sport,
					   u32 daddr, u16 dport, int dif)
{
	TCP_V4_ADDR_COOKIE(acookie, saddr, daddr)
	__u16 hnum = ntohs(dport);
	__u32 ports = TCP_COMBINED_PORTS(sport, hnum);
	struct sock *sk;
	int hash;

	/* Check TCP register quick cache first. */
	sk = PTCP_RHASH(sport);
	if(sk && TCP_IPV4_MATCH(sk, acookie, saddr, daddr, ports, dif))
		goto hit;

	/* Optimize here for direct hit, only listening connections can
	 * have wildcards anyways.
	 */
	hash = ptcp_hashfn(daddr, hnum, saddr, sport);
	for(sk = ptcp_established_hash[hash]; sk; sk = sk->next) {
		if(TCP_IPV4_MATCH(sk, acookie, saddr, daddr, ports, dif)) {
			if (sk->state == TCP_ESTABLISHED)
				PTCP_RHASH(sport) = sk;
			goto hit; /* You sunk my battleship! */
		}
	}
	/* Must check for a TIME_WAIT'er before going to listener hash. */
	for(sk = ptcp_established_hash[hash+(TCP_HTABLE_SIZE/2)]; sk; sk = sk->next)
		if(TCP_IPV4_MATCH(sk, acookie, saddr, daddr, ports, dif))
			goto hit;
	sk = ptcp_v4_lookup_listener(daddr, hnum, dif);
hit:
	return sk;
}

__inline__ struct sock *ptcp_v4_lookup(u32 saddr, u16 sport, u32 daddr, u16 dport, int dif)
{
	return __ptcp_v4_lookup(0, saddr, sport, daddr, dport, dif);
}

#ifdef CONFIG_IP_TRANSPARENT_PROXY
/* Cleaned up a little and adapted to new bind bucket scheme.
 * Oddly, this should increase performance here for
 * transparent proxy, as tests within the inner loop have
 * been eliminated. -DaveM
 */
static struct sock *ptcp_v4_proxy_lookup(unsigned short num, unsigned long raddr,
					unsigned short rnum, unsigned long laddr,
					struct device *dev, unsigned short pnum,
					int dif)
{
	struct sock *s, *result = NULL;
	int badness = -1;
	u32 paddr = 0;
	unsigned short hnum = ntohs(num);
	unsigned short hpnum = ntohs(pnum);
	int firstpass = 1;

	if(dev && dev->ip_ptr) {
		struct in_device *idev = dev->ip_ptr;

		if(idev->ifa_list)
			paddr = idev->ifa_list->ifa_local;
	}

	/* This code must run only from NET_BH. */
	{
		struct tcp_bind_bucket *tb = tcp_bound_hash[tcp_bhashfn(hnum)];
		for( ; (tb && tb->port != hnum); tb = tb->next)
			;
		if(tb == NULL)
			goto next;
		s = tb->owners;
	}
pass2:
	for(; s; s = s->bind_next) {
		int score = 0;
		if(s->rcv_saddr) {
			if((s->num != hpnum || s->rcv_saddr != paddr) &&
			   (s->num != hnum || s->rcv_saddr != laddr))
				continue;
			score++;
		}
		if(s->daddr) {
			if(s->daddr != raddr)
				continue;
			score++;
		}
		if(s->dport) {
			if(s->dport != rnum)
				continue;
			score++;
		}
		if(s->bound_dev_if) {
			if(s->bound_dev_if != dif)
				continue;
			score++;
		}
		if(score == 4 && s->num == hnum) {
			result = s;
			goto gotit;
		} else if(score > badness && (s->num == hpnum || s->rcv_saddr)) {
			result = s;
			badness = score;
		}
	}
next:
	if(firstpass--) {
		struct tcp_bind_bucket *tb = tcp_bound_hash[tcp_bhashfn(hpnum)];
		for( ; (tb && tb->port != hpnum); tb = tb->next)
			;
		if(tb) {
			s = tb->owners;
			goto pass2;
		}
	}
gotit:
	return result;
}
#endif /* CONFIG_IP_TRANSPARENT_PROXY */

static inline __u32 ptcp_v4_init_sequence(struct sock *sk, struct sk_buff *skb)
{
	return secure_tcp_sequence_number(sk->saddr, sk->daddr,
					  skb->h.th->dest,
					  skb->h.th->source);
}

/* Check that a TCP address is unique, don't allow multiple
 * connects to/from the same address.  Actually we can optimize
 * quite a bit, since the socket about to connect is still
 * in TCP_CLOSE, a tcp_bind_bucket for the local port he will
 * use will exist, with a NULL owners list.  So check for that.
 * The good_socknum and verify_bind scheme we use makes this
 * work.
 */
static int ptcp_v4_unique_address(struct sock *sk)
{
	struct tcp_bind_bucket *tb;
	unsigned short snum = sk->num;
	int retval = 1;

	/* Freeze the hash while we snoop around. */
	SOCKHASH_LOCK();
	tb = tcp_bound_hash[tcp_bhashfn(snum)];
	for(; tb; tb = tb->next) {
		if(tb->port == snum && tb->owners != NULL) {
			/* Almost certainly the re-use port case, search the real hashes
			 * so it actually scales.
			 */
			sk = __ptcp_v4_lookup(NULL, sk->daddr, sk->dport,
					     sk->rcv_saddr, htons(snum),
					     sk->bound_dev_if);
			if((sk != NULL) && (sk->state != TCP_LISTEN))
				retval = 0;
			break;
		}
	}
	SOCKHASH_UNLOCK();
	return retval;
}

/* This will initiate an outgoing connection. */
int ptcp_v4_connect(struct sock *sk, struct sockaddr *uaddr, int addr_len)
{
	struct tcp_opt *tp = &(sk->tp_pinfo.af_tcp);
	struct sockaddr_in *usin = (struct sockaddr_in *) uaddr;
	struct sk_buff *buff;
	struct rtable *rt;
	u32 daddr, nexthop;
	int tmp;

	if (sk->state != TCP_CLOSE) 
		return(-EISCONN);

	/* Don't allow a double connect. */
	if (sk->daddr)
		return -EINVAL;

	if (addr_len < sizeof(struct sockaddr_in))
		return(-EINVAL);

	if (usin->sin_family != AF_INET) {
		static int complained;
		if (usin->sin_family)
			return(-EAFNOSUPPORT);
		if (!complained++)
			printk(KERN_DEBUG "%s forgot to set AF_INET in " __FUNCTION__ "\n", current->comm);
	}

	nexthop = daddr = usin->sin_addr.s_addr;
	if (sk->opt && sk->opt->srr) {
		if (daddr == 0)
			return -EINVAL;
		nexthop = sk->opt->faddr;
	}

	tmp = ip_route_connect(&rt, nexthop, sk->saddr,
			       RT_TOS(sk->ip_tos)|RTO_CONN|sk->localroute, sk->bound_dev_if);
	if (tmp < 0)
		return tmp;

	if (rt->rt_flags&(RTCF_MULTICAST|RTCF_BROADCAST)) {
		ip_rt_put(rt);
		return -ENETUNREACH;
	}

	dst_release(xchg(&sk->dst_cache, rt));

	buff = sock_wmalloc(sk, (MAX_HEADER + sk->prot->max_header),
			    0, GFP_KERNEL);

	if (buff == NULL)
		return -ENOBUFS;

	/* Socket has no identity, so lock_sock() is useless.  Also
	 * since state==TCP_CLOSE (checked above) the socket cannot
	 * possibly be in the hashes.  TCP hash locking is only
	 * needed while checking quickly for a unique address.
	 * However, the socket does need to be (and is) locked
	 * in ptcp_connect().
	 * Perhaps this addresses all of ANK's concerns. 8-)  -DaveM
	 */
	sk->dport = usin->sin_port;
	sk->daddr = rt->rt_dst;
	if (sk->opt && sk->opt->srr)
		sk->daddr = daddr;
	if (!sk->saddr)
		sk->saddr = rt->rt_src;
	sk->rcv_saddr = sk->saddr;

	if (!ptcp_v4_unique_address(sk)) {
		kfree_skb(buff);
		sk->daddr = 0;
		return -EADDRNOTAVAIL;
	}

	tp->write_seq = secure_tcp_sequence_number(sk->saddr, sk->daddr,
						   sk->sport, usin->sin_port);

	tp->ext_header_len = 0;
	if (sk->opt)
		tp->ext_header_len = sk->opt->optlen;

	/* Reset mss clamp */
	tp->mss_clamp = ~0;

	if (!ip_dont_fragment(sk, &rt->u.dst) &&
	    rt->u.dst.pmtu > 576 && rt->rt_dst != rt->rt_gateway) {
		/* Clamp mss at maximum of 536 and user_mss.
		   Probably, user ordered to override tiny segment size
		   in gatewayed case.
		 */
		tp->mss_clamp = max(tp->user_mss, 536);
	}

	ptcp_connect(sk, buff, rt->u.dst.pmtu);
	return 0;
}

static int ptcp_v4_sendmsg(struct sock *sk, struct msghdr *msg, int len)
{
	int retval = -EINVAL;

	/* Do sanity checking for sendmsg/sendto/send. */
	if (msg->msg_flags & ~(MSG_OOB|MSG_DONTROUTE|MSG_DONTWAIT|MSG_NOSIGNAL))
		goto out;
	if (msg->msg_name) {
		struct sockaddr_in *addr=(struct sockaddr_in *)msg->msg_name;

		if (msg->msg_namelen < sizeof(*addr))
			goto out;
		if (addr->sin_family && addr->sin_family != AF_INET)
			goto out;
		retval = -ENOTCONN;
		if(sk->state == TCP_CLOSE)
			goto out;
		retval = -EISCONN;
		if (addr->sin_port != sk->dport)
			goto out;
		if (addr->sin_addr.s_addr != sk->daddr)
			goto out;
	}
	retval = ptcp_do_sendmsg(sk, msg);

out:
	return retval;
}


/*
 * Do a linear search in the socket open_request list. 
 * This should be replaced with a global hash table.
 */
static struct open_request *ptcp_v4_search_req(struct tcp_opt *tp, 
					      struct iphdr *iph,
					      struct tcphdr *th,
					      struct open_request **prevp)
{
	struct open_request *req, *prev;  
	__u16 rport = th->source; 

	/*	assumption: the socket is not in use.
	 *	as we checked the user count on tcp_rcv and we're
	 *	running from a soft interrupt.
	 */
	prev = (struct open_request *) (&tp->syn_wait_queue); 
	for (req = prev->dl_next; req; req = req->dl_next) {
		if (req->af.v4_req.rmt_addr == iph->saddr &&
		    req->af.v4_req.loc_addr == iph->daddr &&
		    req->rmt_port == rport
#ifdef CONFIG_IP_TRANSPARENT_PROXY
		    && req->lcl_port == th->dest
#endif
		    ) {
			*prevp = prev; 
			return req; 
		}
		prev = req; 
	}
	return NULL; 
}


/* 
 * This routine does path mtu discovery as defined in RFC1191.
 */
static inline void do_pmtu_discovery(struct sock *sk, struct iphdr *ip, unsigned mtu)
{
	struct tcp_opt *tp = &sk->tp_pinfo.af_tcp;

	if (atomic_read(&sk->sock_readers))
		return;

	/* Don't interested in TCP_LISTEN and open_requests (SYN-ACKs
	 * send out by Linux are always <576bytes so they should go through
	 * unfragmented).
	 */
	if (sk->state == TCP_LISTEN)
		return; 

	/* We don't check in the destentry if pmtu discovery is forbidden
	 * on this route. We just assume that no packet_to_big packets
	 * are send back when pmtu discovery is not active.
     	 * There is a small race when the user changes this flag in the
	 * route, but I think that's acceptable.
	 */
	if (sk->dst_cache == NULL)
		return;
	ip_rt_update_pmtu(sk->dst_cache, mtu);
	if (sk->ip_pmtudisc != IP_PMTUDISC_DONT &&
	    tp->pmtu_cookie > sk->dst_cache->pmtu) {
		ptcp_sync_mss(sk, sk->dst_cache->pmtu);

		/* Resend the TCP packet because it's  
		 * clear that the old packet has been
		 * dropped. This is the new "fast" path mtu
		 * discovery.
		 */
		ptcp_simple_retransmit(sk);
	} /* else let the usual retransmit timer handle it */
}

/*
 * This routine is called by the ICMP module when it gets some
 * sort of error condition.  If err < 0 then the socket should
 * be closed and the error returned to the user.  If err > 0
 * it's just the icmp type << 8 | icmp code.  After adjustment
 * header points to the first 8 bytes of the tcp header.  We need
 * to find the appropriate port.
 *
 * The locking strategy used here is very "optimistic". When
 * someone else accesses the socket the ICMP is just dropped
 * and for some paths there is no check at all.
 * A more general error queue to queue errors for later handling
 * is probably better.
 *
 * sk->err and sk->err_soft should be atomic_t.
 */

void ptcp_v4_err(struct sk_buff *skb, unsigned char *dp, int len)
{
	struct iphdr *iph = (struct iphdr*)dp;
	struct tcphdr *th; 
	struct tcp_opt *tp;
	int type = skb->h.icmph->type;
	int code = skb->h.icmph->code;
#if ICMP_MIN_LENGTH < 14
	int no_flags = 0;
#else
#define no_flags 0
#endif
	struct sock *sk;
	__u32 seq;
	int err;

	if (len < (iph->ihl << 2) + ICMP_MIN_LENGTH) { 
		icmp_statistics.IcmpInErrors++; 
		return;
	}
#if ICMP_MIN_LENGTH < 14
	if (len < (iph->ihl << 2) + 14)
		no_flags = 1;
#endif

	th = (struct tcphdr*)(dp+(iph->ihl<<2));

	sk = ptcp_v4_lookup(iph->daddr, th->dest, iph->saddr, th->source, skb->dev->ifindex);
	if (sk == NULL || sk->state == TCP_TIME_WAIT) {
		icmp_statistics.IcmpInErrors++;
		return; 
	}

	tp = &sk->tp_pinfo.af_tcp;
	seq = ntohl(th->seq);
	if (sk->state != TCP_LISTEN && !between(seq, tp->snd_una, tp->snd_nxt)) {
		net_statistics.OutOfWindowIcmps++;
		return; 
	}

	switch (type) {
	case ICMP_SOURCE_QUENCH:
#ifndef OLD_SOURCE_QUENCH /* This is deprecated */
		tp->snd_ssthresh = ptcp_recalc_ssthresh(tp);
		tp->snd_cwnd = tp->snd_ssthresh;
		tp->snd_cwnd_cnt = 0;
		tp->high_seq = tp->snd_nxt;
#endif
		return;
	case ICMP_PARAMETERPROB:
		err = EPROTO;
		break; 
	case ICMP_DEST_UNREACH:
		if (code > NR_ICMP_UNREACH)
			return;

		if (code == ICMP_FRAG_NEEDED) { /* PMTU discovery (RFC1191) */
			do_pmtu_discovery(sk, iph, ntohs(skb->h.icmph->un.frag.mtu));
			return;
		}

		err = icmp_err_convert[code].errno;
		break;
	case ICMP_TIME_EXCEEDED:
		err = EHOSTUNREACH;
		break;
	default:
		return;
	}

	switch (sk->state) {
		struct open_request *req, *prev;
	case TCP_LISTEN:
		/* Prevent race conditions with accept() - 
		 * ICMP is unreliable. 
		 */
		if (atomic_read(&sk->sock_readers)) {
			net_statistics.LockDroppedIcmps++;
			 /* If too many ICMPs get dropped on busy
			  * servers this needs to be solved differently.
			  */
			return;
		}

		/* The final ACK of the handshake should be already 
		 * handled in the new socket context, not here.
		 * Strictly speaking - an ICMP error for the final
		 * ACK should set the opening flag, but that is too
		 * complicated right now. 
		 */ 
		if (!no_flags && !th->syn && !th->ack)
			return;

		req = ptcp_v4_search_req(tp, iph, th, &prev); 
		if (!req)
			return;
		if (seq != req->snt_isn) {
			net_statistics.OutOfWindowIcmps++;
			return;
		}
		if (req->sk) {	
			/* 
			 * Already in ESTABLISHED and a big socket is created,
			 * set error code there.
			 * The error will _not_ be reported in the accept(),
			 * but only with the next operation on the socket after
			 * accept. 
			 */
			sk = req->sk;
		} else {
			/* 
			 * Still in SYN_RECV, just remove it silently.
			 * There is no good way to pass the error to the newly
			 * created socket, and POSIX does not want network
			 * errors returned from accept(). 
			 */ 
			tp->syn_backlog--;
			ptcp_synq_unlink(tp, req, prev);
			req->class->destructor(req);
			ptcp_openreq_free(req);
			return; 
		}
		break;
	case TCP_SYN_SENT:
	case TCP_SYN_RECV:  /* Cannot happen */ 
		if (!no_flags && !th->syn)
			return;
		ptcp_statistics.TcpAttemptFails++;
		sk->err = err;
		sk->zapped = 1;
		mb();
		sk->error_report(sk);
		return;
	}

	/* If we've already connected we will keep trying
	 * until we time out, or the user gives up.
	 *
	 * rfc1122 4.2.3.9 allows to consider as hard errors
	 * only PROTO_UNREACH and PORT_UNREACH (well, FRAG_FAILED too,
	 * but it is obsoleted by pmtu discovery).
	 *
	 * Note, that in modern internet, where routing is unreliable
	 * and in each dark corner broken firewalls sit, sending random
	 * errors ordered by their masters even this two messages finally lose
	 * their original sense (even Linux sends invalid PORT_UNREACHs)
	 *
	 * Now we are in compliance with RFCs.
	 *							--ANK (980905)
	 */

	if (sk->ip_recverr) {
		/* This code isn't serialized with the socket code */
		/* ANK (980927) ... which is harmless now,
		   sk->err's may be safely lost.
		 */
		sk->err = err;
		mb(); 
		sk->error_report(sk);		/* Wake people up to see the error (see connect in sock.c) */
	} else	{ /* Only an error on timeout */
		sk->err_soft = err;
		mb(); 
	}
}

/* This routine computes an IPv4 TCP checksum. */
void ptcp_v4_send_check(struct sock *sk, struct tcphdr *th, int len, 
		       struct sk_buff *skb)
{
	th->check = 0;
	th->check = tcp_v4_check(th, len, sk->saddr, sk->daddr,
				 csum_partial((char *)th, th->doff<<2, skb->csum));
}

/*
 *	This routine will send an RST to the other tcp.
 *
 *	Someone asks: why I NEVER use socket parameters (TOS, TTL etc.)
 *		      for reset.
 *	Answer: if a packet caused RST, it is not for a socket
 *		existing in our system, if it is matched to a socket,
 *		it is just duplicate segment or bug in other side's TCP.
 *		So that we build reply only basing on parameters
 *		arrived with segment.
 *	Exception: precedence violation. We do not implement it in any case.
 */

static void ptcp_v4_send_reset(struct sk_buff *skb)
{
	struct tcphdr *th = skb->h.th;
	struct tcphdr rth;
	struct ip_reply_arg arg;

	/* Never send a reset in response to a reset. */
	if (th->rst)
		return;

	if (((struct rtable*)skb->dst)->rt_type != RTN_LOCAL) {
#ifdef CONFIG_IP_TRANSPARENT_PROXY
		if (((struct rtable*)skb->dst)->rt_type == RTN_UNICAST)
			icmp_send(skb, ICMP_DEST_UNREACH,
				  ICMP_PORT_UNREACH, 0);
#endif
		return;
	}

	/* Swap the send and the receive. */
	memset(&rth, 0, sizeof(struct tcphdr)); 
	rth.dest = th->source;
	rth.source = th->dest; 
	rth.doff = sizeof(struct tcphdr)/4;
	rth.rst = 1;

	if (th->ack) {
		rth.seq = th->ack_seq;
	} else {
		rth.ack = 1;
		rth.ack_seq = th->syn ? htonl(ntohl(th->seq)+1) : th->seq;
	}

	memset(&arg, 0, sizeof arg); 
	arg.iov[0].iov_base = (unsigned char *)&rth; 
	arg.iov[0].iov_len  = sizeof rth;
	arg.csum = csum_tcpudp_nofold(skb->nh.iph->daddr, 
				      skb->nh.iph->saddr, /*XXX*/
				      sizeof(struct tcphdr),
				      IPPROTO_TCP,
				      0); 
	arg.n_iov = 1;
	arg.csumoffset = offsetof(struct tcphdr, check) / 2; 

	ip_send_reply(ptcp_socket->sk, skb, &arg, sizeof rth);

	ptcp_statistics.TcpOutSegs++;
	ptcp_statistics.TcpOutRsts++;
}

/* 
 *	Send an ACK for a socket less packet (needed for time wait) 
 *
 *  FIXME: Does not echo timestamps yet.
 *
 *  Assumes that the caller did basic address and flag checks.
 */
static void ptcp_v4_send_ack(struct sk_buff *skb, __u32 seq, __u32 ack)
{
	struct tcphdr *th = skb->h.th;
	struct tcphdr rth;
	struct ip_reply_arg arg;

	/* Swap the send and the receive. */
	memset(&rth, 0, sizeof(struct tcphdr)); 
	rth.dest = th->source;
	rth.source = th->dest; 
	rth.doff = sizeof(struct tcphdr)/4;

	rth.seq = seq;
	rth.ack_seq = ack; 
	rth.ack = 1;

	memset(&arg, 0, sizeof arg); 
	arg.iov[0].iov_base = (unsigned char *)&rth; 
	arg.iov[0].iov_len  = sizeof rth;
	arg.csum = csum_tcpudp_nofold(skb->nh.iph->daddr, 
				      skb->nh.iph->saddr, /*XXX*/
				      sizeof(struct tcphdr),
				      IPPROTO_TCP,
				      0); 
	arg.n_iov = 1;
	arg.csumoffset = offsetof(struct tcphdr, check) / 2; 

	ip_send_reply(ptcp_socket->sk, skb, &arg, sizeof rth);

	ptcp_statistics.TcpOutSegs++;
}


#ifdef CONFIG_IP_TRANSPARENT_PROXY

/*
   Seems, I never wrote nothing more stupid.
   I hope Gods will forgive me, but I cannot forgive myself 8)
                                                --ANK (981001)
 */

static struct sock *ptcp_v4_search_proxy_openreq(struct sk_buff *skb)
{
	struct iphdr *iph = skb->nh.iph;
	struct tcphdr *th = (struct tcphdr *)(skb->nh.raw + iph->ihl*4);
	struct sock *sk;
	int i;

	for (i=0; i<TCP_LHTABLE_SIZE; i++) {
		for(sk = ptcp_listening_hash[i]; sk; sk = sk->next) {
			struct open_request *dummy;
			if (ptcp_v4_search_req(&sk->tp_pinfo.af_tcp, iph,
					      th, &dummy) &&
			    (!sk->bound_dev_if ||
			     sk->bound_dev_if == skb->dev->ifindex))
				return sk;
		}
	}
	return NULL;
}

/*
 *	Check whether a received TCP packet might be for one of our
 *	connections.
 */

int ptcp_chkaddr(struct sk_buff *skb)
{
	struct iphdr *iph = skb->nh.iph;
	struct tcphdr *th = (struct tcphdr *)(skb->nh.raw + iph->ihl*4);
	struct sock *sk;

	sk = ptcp_v4_lookup(iph->saddr, th->source, iph->daddr,
			   th->dest, skb->dev->ifindex);

	if (!sk)
		return ptcp_v4_search_proxy_openreq(skb) != NULL;

	if (sk->state == TCP_LISTEN) {
		struct open_request *dummy;
		if (ptcp_v4_search_req(&sk->tp_pinfo.af_tcp, skb->nh.iph,
				      th, &dummy) &&
		    (!sk->bound_dev_if ||
		     sk->bound_dev_if == skb->dev->ifindex))
			return 1;
	}

	/* 0 means accept all LOCAL addresses here, not all the world... */

	if (sk->rcv_saddr == 0)
		return 0;

	return 1;
}
#endif

/*
 *	Send a SYN-ACK after having received an ACK. 
 *	This still operates on a open_request only, not on a big
 *	socket.
 */ 
static void ptcp_v4_send_synack(struct sock *sk, struct open_request *req)
{
	struct rtable *rt;
	struct ip_options *opt;
	struct sk_buff * skb;
	int mss;

	/* First, grab a route. */
	opt = req->af.v4_req.opt;
	if(ip_route_output(&rt, ((opt && opt->srr) ?
				 opt->faddr :
				 req->af.v4_req.rmt_addr),
			   req->af.v4_req.loc_addr,
			   RT_TOS(sk->ip_tos) | RTO_CONN | sk->localroute,
			   sk->bound_dev_if)) {
		ip_statistics.IpOutNoRoutes++;
		return;
	}
	if(opt && opt->is_strictroute && rt->rt_dst != rt->rt_gateway) {
		ip_rt_put(rt);
		ip_statistics.IpOutNoRoutes++;
		return;
	}

	mss = rt->u.dst.pmtu - sizeof(struct iphdr) - sizeof(struct tcphdr);

	skb = ptcp_make_synack(sk, &rt->u.dst, req, mss);
	if (skb) {
		struct tcphdr *th = skb->h.th;

#ifdef CONFIG_IP_TRANSPARENT_PROXY
		th->source = req->lcl_port; /* LVE */
#endif

		th->check = tcp_v4_check(th, skb->len,
					 req->af.v4_req.loc_addr, req->af.v4_req.rmt_addr,
					 csum_partial((char *)th, skb->len, skb->csum));

		ip_build_and_send_pkt(skb, sk, req->af.v4_req.loc_addr,
				      req->af.v4_req.rmt_addr, req->af.v4_req.opt);
	}
	ip_rt_put(rt);
}

/*
 *	IPv4 open_request destructor.
 */ 
static void ptcp_v4_or_free(struct open_request *req)
{
	if(!req->sk && req->af.v4_req.opt)
		kfree_s(req->af.v4_req.opt, optlength(req->af.v4_req.opt));
}

static inline void syn_flood_warning(struct sk_buff *skb)
{
	static unsigned long warntime;
	
	if (jiffies - warntime > HZ*60) {
		warntime = jiffies;
		printk(KERN_INFO 
		       "possible SYN flooding on port %d. Sending cookies.\n",  
		       ntohs(skb->h.th->dest));
	}
}

/* 
 * Save and compile IPv4 options into the open_request if needed. 
 */
static inline struct ip_options * 
ptcp_v4_save_options(struct sock *sk, struct sk_buff *skb)
{
	struct ip_options *opt = &(IPCB(skb)->opt);
	struct ip_options *dopt = NULL; 

	if (opt && opt->optlen) {
		int opt_size = optlength(opt); 
		dopt = kmalloc(opt_size, GFP_ATOMIC);
		if (dopt) {
			if (ip_options_echo(dopt, skb)) {
				kfree_s(dopt, opt_size);
				dopt = NULL;
			}
		}
	}
	return dopt;
}

/* 
 * Maximum number of SYN_RECV sockets in queue per LISTEN socket.
 * One SYN_RECV socket costs about 80bytes on a 32bit machine.
 * It would be better to replace it with a global counter for all sockets
 * but then some measure against one socket starving all other sockets
 * would be needed.
 */
int sysctl_max_syn_backlog = 128; 

struct or_calltable or_ipv4 = {
	ptcp_v4_send_synack,
	ptcp_v4_or_free,
	ptcp_v4_send_reset
};

#define BACKLOG(sk) ((sk)->tp_pinfo.af_tcp.syn_backlog) /* lvalue! */
#define BACKLOGMAX(sk) sysctl_max_syn_backlog

int ptcp_v4_conn_request(struct sock *sk, struct sk_buff *skb, __u32 isn)
{
	struct tcp_opt tp;
	struct open_request *req;
	struct tcphdr *th = skb->h.th;
	__u32 saddr = skb->nh.iph->saddr;
	__u32 daddr = skb->nh.iph->daddr;
#ifdef CONFIG_SYN_COOKIES
	int want_cookie = 0;
#else
#define want_cookie 0 /* Argh, why doesn't gcc optimize this :( */
#endif

	/* If the socket is dead, don't accept the connection.	*/
	if (sk->dead) 
		goto dead; 

	/* Never answer to SYNs send to broadcast or multicast */
	if (((struct rtable *)skb->dst)->rt_flags & 
	    (RTCF_BROADCAST|RTCF_MULTICAST))
		goto drop; 

	/* XXX: Check against a global syn pool counter. */
	if (BACKLOG(sk) > BACKLOGMAX(sk)) {
#ifdef CONFIG_SYN_COOKIES
		if (sysctl_tcp_syncookies) {
			syn_flood_warning(skb);
			want_cookie = 1; 
		} else
#endif
		goto drop;
	} else { 
		if (isn == 0)
			isn = ptcp_v4_init_sequence(sk, skb);
		BACKLOG(sk)++;
	}

	req = ptcp_openreq_alloc();
	if (req == NULL) {
		goto dropbacklog;
	}

	req->rcv_wnd = 0;		/* So that ptcp_send_synack() knows! */

	req->rcv_isn = TCP_SKB_CB(skb)->seq;
 	tp.tstamp_ok = tp.sack_ok = tp.wscale_ok = tp.snd_wscale = 0;

	tp.mss_clamp = 65535;
	ptcp_parse_options(NULL, th, &tp, want_cookie);
	if (tp.mss_clamp == 65535)
		tp.mss_clamp = 576 - sizeof(struct iphdr) - sizeof(struct iphdr);

	if (sk->tp_pinfo.af_tcp.user_mss && sk->tp_pinfo.af_tcp.user_mss < tp.mss_clamp)
		tp.mss_clamp = sk->tp_pinfo.af_tcp.user_mss;
	req->mss = tp.mss_clamp;

	if (tp.saw_tstamp)
		req->ts_recent = tp.rcv_tsval;
	req->tstamp_ok = tp.tstamp_ok;
	req->sack_ok = 0;
	req->snd_wscale = tp.snd_wscale;
	req->wscale_ok = tp.wscale_ok;
	req->rmt_port = th->source;
#ifdef CONFIG_IP_TRANSPARENT_PROXY
	req->lcl_port = th->dest ; /* LVE */
#endif
	req->af.v4_req.loc_addr = daddr;
	req->af.v4_req.rmt_addr = saddr;

	/* Note that we ignore the isn passed from the TIME_WAIT
	 * state here. That's the price we pay for cookies.
	 */
	if (want_cookie)
		isn = cookie_v4_init_sequence(sk, skb, &req->mss);

	req->snt_isn = isn;

	req->af.v4_req.opt = ptcp_v4_save_options(sk, skb);

	req->class = &or_ipv4;
	req->retrans = 0;
	req->sk = NULL;

	ptcp_v4_send_synack(sk, req);

	if (want_cookie) {
		if (req->af.v4_req.opt)
			kfree(req->af.v4_req.opt);
		ptcp_v4_or_free(req); 
	   	ptcp_openreq_free(req); 
	} else {
		req->expires = jiffies + TCP_TIMEOUT_INIT;
		ptcp_inc_slow_timer(TCP_SLT_SYNACK);
		ptcp_synq_queue(&sk->tp_pinfo.af_tcp, req);
	}

	return 0;

dead:
	SOCK_DEBUG(sk, "Reset on %p: Connect on dead socket.\n",sk);
	ptcp_statistics.TcpAttemptFails++;
	return -ENOTCONN; /* send reset */

dropbacklog:
	if (!want_cookie) 
		BACKLOG(sk)--;
drop:
	ptcp_statistics.TcpAttemptFails++;
	return 0;
}

/* This is not only more efficient than what we used to do, it eliminates
 * a lot of code duplication between IPv4/IPv6 SYN recv processing. -DaveM
 *
 * This function wants to be moved to a common for IPv[46] file. --ANK
 */
struct sock *ptcp_create_openreq_child(struct sock *sk, struct open_request *req, struct sk_buff *skb)
{
	struct sock *newsk = sk_alloc(PF_INET, GFP_ATOMIC, 0);

	if(newsk != NULL) {
		struct tcp_opt *newtp;
#ifdef CONFIG_FILTER
		struct sk_filter *filter;
#endif

		memcpy(newsk, sk, sizeof(*newsk));
		newsk->sklist_next = NULL;
		newsk->state = TCP_SYN_RECV;

		/* Clone the TCP header template */
		newsk->dport = req->rmt_port;

		atomic_set(&newsk->sock_readers, 0);
		atomic_set(&newsk->rmem_alloc, 0);
		skb_queue_head_init(&newsk->receive_queue);
		atomic_set(&newsk->wmem_alloc, 0);
		skb_queue_head_init(&newsk->write_queue);
		atomic_set(&newsk->omem_alloc, 0);

		newsk->done = 0;
		newsk->proc = 0;
		skb_queue_head_init(&newsk->back_log);
		skb_queue_head_init(&newsk->error_queue);
#ifdef CONFIG_FILTER
		if ((filter = newsk->filter) != NULL)
			sk_filter_charge(newsk, filter);
#endif

		/* Now setup tcp_opt */
		newtp = &(newsk->tp_pinfo.af_tcp);
		newtp->pred_flags = 0;
		newtp->rcv_nxt = req->rcv_isn + 1;
		newtp->snd_nxt = req->snt_isn + 1;
		newtp->snd_una = req->snt_isn + 1;
		newtp->srtt = 0;
		newtp->ato = 0;
		newtp->snd_wl1 = req->rcv_isn;
		newtp->snd_wl2 = req->snt_isn;

		/* RFC1323: The window in SYN & SYN/ACK segments
		 * is never scaled.
		 */
		newtp->snd_wnd = ntohs(skb->h.th->window);

		newtp->max_window = newtp->snd_wnd;
		newtp->pending = 0;
		newtp->retransmits = 0;
		newtp->last_ack_sent = req->rcv_isn + 1;
		newtp->backoff = 0;
		newtp->mdev = TCP_TIMEOUT_INIT;

		/* So many TCP implementations out there (incorrectly) count the
		 * initial SYN frame in their delayed-ACK and congestion control
		 * algorithms that we must have the following bandaid to talk
		 * efficiently to them.  -DaveM
		 */
		newtp->snd_cwnd = 2;

		newtp->rto = TCP_TIMEOUT_INIT;
		newtp->packets_out = 0;
		newtp->fackets_out = 0;
		newtp->retrans_out = 0;
		newtp->high_seq = 0;
		newtp->snd_ssthresh = 0x7fffffff;
		newtp->snd_cwnd_cnt = 0;
		newtp->dup_acks = 0;
		newtp->delayed_acks = 0;
		init_timer(&newtp->retransmit_timer);
		newtp->retransmit_timer.function = &ptcp_retransmit_timer;
		newtp->retransmit_timer.data = (unsigned long) newsk;
		init_timer(&newtp->delack_timer);
		newtp->delack_timer.function = &ptcp_delack_timer;
		newtp->delack_timer.data = (unsigned long) newsk;
		skb_queue_head_init(&newtp->out_of_order_queue);
		newtp->send_head = newtp->retrans_head = NULL;
		newtp->rcv_wup = req->rcv_isn + 1;
		newtp->write_seq = req->snt_isn + 1;
		newtp->copied_seq = req->rcv_isn + 1;

		newtp->saw_tstamp = 0;
		newtp->mss_clamp = req->mss;

		init_timer(&newtp->probe_timer);
		newtp->probe_timer.function = &ptcp_probe_timer;
		newtp->probe_timer.data = (unsigned long) newsk;
		newtp->probes_out = 0;
		newtp->syn_seq = req->rcv_isn;
		newtp->fin_seq = req->rcv_isn;
		newtp->urg_data = 0;
		ptcp_synq_init(newtp);
		newtp->syn_backlog = 0;
		if (skb->len >= 536)
			newtp->last_seg_size = skb->len; 

		/* Back to base struct sock members. */
		newsk->err = 0;
		newsk->ack_backlog = 0;
		newsk->max_ack_backlog = SOMAXCONN;
		newsk->priority = 0;

		/* IP layer stuff */
		newsk->timeout = 0;
		init_timer(&newsk->timer);
		newsk->timer.function = &net_timer;
		newsk->timer.data = (unsigned long) newsk;
		newsk->socket = NULL;

		newtp->tstamp_ok = req->tstamp_ok;
		newtp->sack_ok = 0;
		newtp->window_clamp = req->window_clamp;
		newtp->rcv_wnd = req->rcv_wnd;
		newtp->wscale_ok = req->wscale_ok;
		if (newtp->wscale_ok) {
			newtp->snd_wscale = req->snd_wscale;
			newtp->rcv_wscale = req->rcv_wscale;
		} else {
			newtp->snd_wscale = newtp->rcv_wscale = 0;
			newtp->window_clamp = min(newtp->window_clamp,65535);
		}
		if (newtp->tstamp_ok) {
			newtp->ts_recent = req->ts_recent;
			newtp->ts_recent_stamp = tcp_time_stamp;
			newtp->tcp_header_len = sizeof(struct tcphdr) + TCPOLEN_TSTAMP_ALIGNED;
		} else {
			newtp->tcp_header_len = sizeof(struct tcphdr);
		}
	}
	return newsk;
}

/* 
 * The three way handshake has completed - we got a valid synack - 
 * now create the new socket. 
 */
struct sock * ptcp_v4_syn_recv_sock(struct sock *sk, struct sk_buff *skb,
				   struct open_request *req,
				   struct dst_entry *dst)
{
	struct ip_options *opt = req->af.v4_req.opt;
	struct tcp_opt *newtp;
	struct sock *newsk;

	if (sk->ack_backlog > sk->max_ack_backlog)
		goto exit; /* head drop */
	if (dst == NULL) { 
		struct rtable *rt;
		
		if (ip_route_output(&rt,
			opt && opt->srr ? opt->faddr : req->af.v4_req.rmt_addr,
			req->af.v4_req.loc_addr, sk->ip_tos|RTO_CONN, 0))
			return NULL;
	        dst = &rt->u.dst;
	}
#ifdef CONFIG_IP_TRANSPARENT_PROXY
	/* The new socket created for transparent proxy may fall
	 * into a non-existed bind bucket because sk->num != newsk->num.
	 * Ensure existance of the bucket now. The placement of the check
	 * later will require to destroy just created newsk in the case of fail.
	 * 1998/04/22 Andrey V. Savochkin <saw@msu.ru>
	 */
	if (__tcp_bucket_check(ntohs(skb->h.th->dest)))
		goto exit;
#endif

	newsk = ptcp_create_openreq_child(sk, req, skb);
	if (!newsk) 
		goto exit;

	sk->tp_pinfo.af_tcp.syn_backlog--;
	sk->ack_backlog++;

	newsk->dst_cache = dst;

	newtp = &(newsk->tp_pinfo.af_tcp);
	newsk->daddr = req->af.v4_req.rmt_addr;
	newsk->saddr = req->af.v4_req.loc_addr;
	newsk->rcv_saddr = req->af.v4_req.loc_addr;
#ifdef CONFIG_IP_TRANSPARENT_PROXY
	newsk->num = ntohs(skb->h.th->dest);
	newsk->sport = req->lcl_port;
#endif
	newsk->opt = req->af.v4_req.opt;
	newtp->ext_header_len = 0;
	if (newsk->opt)
		newtp->ext_header_len = newsk->opt->optlen;

	ptcp_sync_mss(newsk, dst->pmtu);
	newtp->rcv_mss = newtp->mss_clamp;

	/* It would be better to use newtp->mss_clamp here */
	if (newsk->rcvbuf < (3 * newtp->pmtu_cookie))
		newsk->rcvbuf = min ((3 * newtp->pmtu_cookie), sysctl_rmem_max);
	if (newsk->sndbuf < (3 * newtp->pmtu_cookie))
		newsk->sndbuf = min ((3 * newtp->pmtu_cookie), sysctl_wmem_max);
 
	/* We run in BH processing itself or within a BH atomic
	 * sequence (backlog) so no locking is needed.
	 */
	__ptcp_v4_hash(newsk);
	__tcp_inherit_port(sk, newsk);
	__add_to_prot_sklist(newsk);

	sk->data_ready(sk, 0); /* Deliver SIGIO */ 

	return newsk;

exit:
	dst_release(dst);
	return NULL;
}

static void ptcp_v4_rst_req(struct sock *sk, struct sk_buff *skb)
{
	struct tcp_opt *tp = &sk->tp_pinfo.af_tcp;
	struct open_request *req, *prev;

	req = ptcp_v4_search_req(tp,skb->nh.iph, skb->h.th, &prev);
	if (!req)
		return;
	/* Sequence number check required by RFC793 */
	if (before(TCP_SKB_CB(skb)->seq, req->rcv_isn) ||
	    after(TCP_SKB_CB(skb)->seq, req->rcv_isn+1))
		return;
	ptcp_synq_unlink(tp, req, prev);
	(req->sk ? sk->ack_backlog : tp->syn_backlog)--;
	req->class->destructor(req);
	ptcp_openreq_free(req); 

	net_statistics.EmbryonicRsts++;
}

/* Check for embryonic sockets (open_requests) We check packets with
 * only the SYN bit set against the open_request queue too: This
 * increases connection latency a bit, but is required to detect
 * retransmitted SYNs.  
 */
static inline struct sock *ptcp_v4_hnd_req(struct sock *sk,struct sk_buff *skb)
{
	struct tcphdr *th = skb->h.th; 
	u32 flg = ((u32 *)th)[3]; 

	/* Check for RST */
	if (flg & __constant_htonl(0x00040000)) {
		ptcp_v4_rst_req(sk, skb);
		return NULL;
	}

	/* Check for SYN|ACK */
	if (flg & __constant_htonl(0x00120000)) {
		struct open_request *req, *dummy; 
		struct tcp_opt *tp = &(sk->tp_pinfo.af_tcp);

		/* Find possible connection requests. */
		req = ptcp_v4_search_req(tp, skb->nh.iph, th, &dummy); 
		if (req) {
			sk = ptcp_check_req(sk, skb, req);
		}
#ifdef CONFIG_SYN_COOKIES
		else if (flg == __constant_htonl(0x00120000))  {
			sk = cookie_v4_check(sk, skb, &(IPCB(skb)->opt));
		}
#endif
	}
	return sk; 
}

static void __ptcp_v4_rehash(struct sock *sk)
{
	struct sock **skp = &ptcp_established_hash[(sk->hashent = ptcp_sk_hashfn(sk))];

	SOCKHASH_LOCK();
	if(sk->pprev) {
		if(sk->next)
			sk->next->pprev = sk->pprev;
		*sk->pprev = sk->next;
		sk->pprev = NULL;
		ptcp_reg_zap(sk);
	}
	if((sk->next = *skp) != NULL)
		(*skp)->pprev = &sk->next;
	*skp = sk;
	sk->pprev = skp;
	SOCKHASH_UNLOCK();
}

int ptcp_v4_rebuild_header(struct sock *sk)
{
	struct rtable *rt = (struct rtable *)sk->dst_cache;
	__u32 new_saddr;
        int want_rewrite = sysctl_ip_dynaddr && sk->state == TCP_SYN_SENT;

	if(rt == NULL)
		return 0;

	/* Force route checking if want_rewrite.
	 * The idea is good, the implementation is disguisting.
	 * Well, if I made bind on this socket, you cannot randomly ovewrite
	 * its source address. --ANK
	 */
	if (want_rewrite) {
		int tmp;
		struct rtable *new_rt;
		__u32 old_saddr = rt->rt_src;

		/* Query new route using another rt buffer */
		tmp = ip_route_connect(&new_rt, rt->rt_dst, 0,
					RT_TOS(sk->ip_tos)|sk->localroute,
					sk->bound_dev_if);

		/* Only useful if different source addrs */
		if (tmp == 0) {
			/*
			 *	Only useful if different source addrs
			 */
			if (new_rt->rt_src != old_saddr ) {
				dst_release(sk->dst_cache);
				sk->dst_cache = &new_rt->u.dst;
				rt = new_rt;
				goto do_rewrite;
			} 
			dst_release(&new_rt->u.dst);
		}
	}
	if (rt->u.dst.obsolete) {
		int err;
		err = ip_route_output(&rt, rt->rt_dst, rt->rt_src, rt->key.tos|RTO_CONN, rt->key.oif);
		if (err) {
			sk->err_soft=-err;
			sk->error_report(sk);
			return -1;
		}
		dst_release(xchg(&sk->dst_cache, &rt->u.dst));
	}

	return 0;

do_rewrite:
	new_saddr = rt->rt_src;
                
	/* Ouch!, this should not happen. */
	if (!sk->saddr || !sk->rcv_saddr) {
		printk(KERN_WARNING "ptcp_v4_rebuild_header(): not valid sock addrs: "
		       "saddr=%08lX rcv_saddr=%08lX\n",
		       ntohl(sk->saddr), 
		       ntohl(sk->rcv_saddr));
		return 0;
	}

	if (new_saddr != sk->saddr) {
		if (sysctl_ip_dynaddr > 1) {
			printk(KERN_INFO "ptcp_v4_rebuild_header(): shifting sk->saddr "
			       "from %d.%d.%d.%d to %d.%d.%d.%d\n",
			       NIPQUAD(sk->saddr), 
			       NIPQUAD(new_saddr));
		}

		sk->saddr = new_saddr;
		sk->rcv_saddr = new_saddr;

		/* XXX The only one ugly spot where we need to
		 * XXX really change the sockets identity after
		 * XXX it has entered the hashes. -DaveM
		 */
		__ptcp_v4_rehash(sk);
	} 
        
	return 0;
}

static struct sock * ptcp_v4_get_sock(struct sk_buff *skb, struct tcphdr *th)
{
	return ptcp_v4_lookup(skb->nh.iph->saddr, th->source,
			     skb->nh.iph->daddr, th->dest, skb->dev->ifindex);
}

static void v4_addr2sockaddr(struct sock *sk, struct sockaddr * uaddr)
{
	struct sockaddr_in *sin = (struct sockaddr_in *) uaddr;

	sin->sin_family		= AF_INET;
	sin->sin_addr.s_addr	= sk->daddr;
	sin->sin_port		= sk->dport;
}

struct tcp_func ptcp_ipv4_specific = {
	ip_queue_xmit,		/* queue_xmit */
	ptcp_v4_send_check,	/* send_check */
	ptcp_v4_rebuild_header,	/* rebuild_header */
	ptcp_v4_conn_request,	/* conn_request */
	ptcp_v4_syn_recv_sock,	/* syn_recv_sock */
	ptcp_v4_get_sock,	/* get_sock */
	sizeof(struct iphdr),	/* net_header_len */

	ip_setsockopt,		/* setsockopt */
	ip_getsockopt,		/* getsockopt */
	v4_addr2sockaddr,	/* addr2sockaddr */
	sizeof(struct sockaddr_in)
};

/* NOTE: A lot of things set to zero explicitly by call to
 *       sk_alloc() so need not be done here.
 */
static int ptcp_v4_init_sock(struct sock *sk)
{
	struct tcp_opt *tp = &(sk->tp_pinfo.af_tcp);

	skb_queue_head_init(&tp->out_of_order_queue);
	ptcp_init_xmit_timers(sk);

	tp->rto  = TCP_TIMEOUT_INIT;		/*TCP_WRITE_TIME*/
	tp->mdev = TCP_TIMEOUT_INIT;
	tp->mss_clamp = ~0;
      
	/* So many TCP implementations out there (incorrectly) count the
	 * initial SYN frame in their delayed-ACK and congestion control
	 * algorithms that we must have the following bandaid to talk
	 * efficiently to them.  -DaveM
	 */
	tp->snd_cwnd = 2;

	/* See draft-stevens-tcpca-spec-01 for discussion of the
	 * initialization of these values.
	 */
	tp->snd_cwnd_cnt = 0;
	tp->snd_ssthresh = 0x7fffffff;	/* Infinity */

	sk->state = TCP_CLOSE;
	sk->max_ack_backlog = SOMAXCONN;
	tp->rcv_mss = 536; 

	sk->write_space = ptcp_write_space; 

	/* Init SYN queue. */
	ptcp_synq_init(tp);

	sk->tp_pinfo.af_tcp.af_specific = &ptcp_ipv4_specific;

	return 0;
}

static int ptcp_v4_destroy_sock(struct sock *sk)
{
	struct tcp_opt *tp = &(sk->tp_pinfo.af_tcp);
	struct sk_buff *skb;

	ptcp_clear_xmit_timers(sk);

	if (sk->keepopen)
		ptcp_dec_slow_timer(TCP_SLT_KEEPALIVE);

	/* Cleanup up the write buffer. */
  	while((skb = __skb_dequeue(&sk->write_queue)) != NULL)
		kfree_skb(skb);

	/* Cleans up our, hopefuly empty, out_of_order_queue. */
  	while((skb = __skb_dequeue(&tp->out_of_order_queue)) != NULL)
		kfree_skb(skb);

	/* Clean up a referenced TCP bind bucket, this only happens if a
	 * port is allocated for a socket, but it never fully connects.
	 */
	if(sk->prev != NULL)
		tcp_put_port(sk);

	return 0;
}

struct proto ptcp_prot = {
	(struct sock *)&ptcp_prot,	/* sklist_next */
	(struct sock *)&ptcp_prot,	/* sklist_prev */
	ptcp_close,			/* close */
	ptcp_v4_connect,		/* connect */
	ptcp_accept,			/* accept */
	NULL,				/* retransmit */
	ptcp_write_wakeup,		/* write_wakeup */
	ptcp_read_wakeup,		/* read_wakeup */
	ptcp_poll,			/* poll */
	ptcp_ioctl,			/* ioctl */
	ptcp_v4_init_sock,		/* init */
	ptcp_v4_destroy_sock,		/* destroy */
	ptcp_shutdown,			/* shutdown */
	ptcp_setsockopt,		/* setsockopt */
	ptcp_getsockopt,		/* getsockopt */
	ptcp_v4_sendmsg,		/* sendmsg */
	ptcp_recvmsg,			/* recvmsg */
	NULL,				/* bind */
	receive_segment_backlog__Base__Input, /* backlog_rcv */
	ptcp_v4_hash,			/* hash */
	ptcp_v4_unhash,			/* unhash */
	0, /* initialized below */	/* get_port */
	128,				/* max_header */
	0,				/* retransmits */
	"Prolac TCP",			/* name */
	0,				/* inuse */
	0				/* highestinuse */
};


__initfunc(void ptcp_v4_init(struct net_proto_family *ops))
{
	int err;

	ptcp_prot.get_port = tcp_prot.get_port;
	
	ptcp_inode.i_mode = S_IFSOCK;
	ptcp_inode.i_sock = 1;
	ptcp_inode.i_uid = 0;
	ptcp_inode.i_gid = 0;

	ptcp_socket->inode = &ptcp_inode;
	ptcp_socket->state = SS_UNCONNECTED;
	ptcp_socket->type=SOCK_RAW;

	if ((err=ops->create(ptcp_socket, IPPROTO_TCP))<0)
		panic("Failed to create the TCP control socket.\n");
	ptcp_socket->sk->allocation=GFP_ATOMIC;
	ptcp_socket->sk->num = 256;		/* Don't receive any data */
	ptcp_socket->sk->ip_ttl = MAXTTL;
}


void
ptcp_socket_to_tcp(struct sock *sk)
{
  unsigned long flags;
  save_flags(flags);
  cli();
  
  if (sk->state == TCP_TIME_WAIT) {
    struct tcp_tw_bucket *tw = (struct tcp_tw_bucket *)sk;
    ptcp_tw_deschedule(tw);
    ptcp_timewait_kill(tw);
    printk("<1>killing %p\n", tw);
    
  } else {
    struct sock *prev = sk->prev;
    sk->prev = NULL;
    /* remove from our world */
    del_from_prot_sklist(sk);
    sk->prot->unhash(sk);
    sk->prev = prev;
    
    /* put into their world */
    tcp_v4_steal_sock(sk);
  }
  
  restore_flags(flags);
}

void
ptcp_v4_cleanup(void)
{
  /* sk_free(ptcp_socket->sk); */ /* XXX WhAT SHOULD WE DO WITH THIS?S???? */
  while (ptcp_prot.sklist_next != (struct sock *)&ptcp_prot) {
    ptcp_socket_to_tcp(ptcp_prot.sklist_next);
  }
  if (ptcp_prot.inuse != 0)
    printk("<1> ptcp_prot.inuse == %d\n", ptcp_prot.inuse);
}
