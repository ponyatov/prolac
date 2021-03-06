/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		Implementation of the Transmission Control Protocol(TCP).
 *
 * Version:	$Id: ptcp_timer.c,v 1.1 1999/09/05 04:49:51 eddietwo Exp $
 *
 * Authors:	Ross Biro, <bir7@leland.Stanford.Edu>
 *		Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 *		Mark Evans, <evansmp@uhura.aston.ac.uk>
 *		Corey Minyard <wf-rch!minyard@relay.EU.net>
 *		Florian La Roche, <flla@stud.uni-sb.de>
 *		Charles Hedrick, <hedrick@klinzhai.rutgers.edu>
 *		Linus Torvalds, <torvalds@cs.helsinki.fi>
 *		Alan Cox, <gw4pts@gw4pts.ampr.org>
 *		Matthew Dillon, <dillon@apollo.west.oic.com>
 *		Arnt Gulbrandsen, <agulbra@nvg.unit.no>
 *		Jorge Cwik, <jorge@laser.satlink.net>
 */

#include "ptcp.h"
#include "ptcp_prolac.h"

extern int sysctl_tcp_syn_retries; /* = TCP_SYN_RETRIES */
extern int sysctl_tcp_keepalive_time; /* = TCP_KEEPALIVE_TIME */
extern int sysctl_tcp_keepalive_probes; /* = TCP_KEEPALIVE_PROBES */
extern int sysctl_tcp_retries1; /* = TCP_RETR1 */
extern int sysctl_tcp_retries2; /* = TCP_RETR2 */

static void ptcp_sltimer_handler(unsigned long);
static void ptcp_syn_recv_timer(unsigned long);
static void ptcp_keepalive(unsigned long data);
static void ptcp_twkill(unsigned long);

struct timer_list	ptcp_slow_timer = {
	NULL, NULL,
	0, 0,
	ptcp_sltimer_handler,
};


struct tcp_sl_timer ptcp_slt_array[TCP_SLT_MAX] = {
	{ATOMIC_INIT(0), TCP_SYNACK_PERIOD, 0, ptcp_syn_recv_timer},/* SYNACK	*/
	{ATOMIC_INIT(0), TCP_KEEPALIVE_PERIOD, 0, ptcp_keepalive},  /* KEEPALIVE	*/
	{ATOMIC_INIT(0), TCP_TWKILL_PERIOD, 0, ptcp_twkill}         /* TWKILL	*/
};

const char timer_bug_msg[] = KERN_DEBUG "tcpbug: unknown timer value\n";

/*
 * Using different timers for retransmit, delayed acks and probes
 * We may wish use just one timer maintaining a list of expire jiffies 
 * to optimize.
 */

void ptcp_init_xmit_timers(struct sock *sk)
{
	init_timer(&sk->tp_pinfo.af_tcp.retransmit_timer);
	sk->tp_pinfo.af_tcp.retransmit_timer.function=&ptcp_retransmit_timer;
	sk->tp_pinfo.af_tcp.retransmit_timer.data = (unsigned long) sk;
	
	init_timer(&sk->tp_pinfo.af_tcp.delack_timer);
	sk->tp_pinfo.af_tcp.delack_timer.function=&ptcp_delack_timer;
	sk->tp_pinfo.af_tcp.delack_timer.data = (unsigned long) sk;

	init_timer(&sk->tp_pinfo.af_tcp.probe_timer);
	sk->tp_pinfo.af_tcp.probe_timer.function=&ptcp_probe_timer;
	sk->tp_pinfo.af_tcp.probe_timer.data = (unsigned long) sk;
}

/*
 *	Reset the retransmission timer
 */
 
void ptcp_reset_xmit_timer(struct sock *sk, int what, unsigned long when)
{
	struct tcp_opt *tp = &sk->tp_pinfo.af_tcp;

	switch (what) {
	case TIME_RETRANS:
		/* When seting the transmit timer the probe timer 
		 * should not be set.
		 * The delayed ack timer can be set if we are changing the
		 * retransmit timer when removing acked frames.
		 */
		if(tp->probe_timer.prev)
			del_timer(&tp->probe_timer);
		mod_timer(&tp->retransmit_timer, jiffies+when);
		break;

	case TIME_DACK:
		mod_timer(&tp->delack_timer, jiffies+when);
		break;

	case TIME_PROBE0:
		mod_timer(&tp->probe_timer, jiffies+when);
		break;	

	case TIME_WRITE:
		printk(KERN_DEBUG "bug: ptcp_reset_xmit_timer TIME_WRITE\n");
		break;

	default:
		printk(KERN_DEBUG "bug: unknown timer value\n");
	};
}

void ptcp_clear_xmit_timers(struct sock *sk)
{	
	struct tcp_opt *tp = &sk->tp_pinfo.af_tcp;

	if(tp->retransmit_timer.prev)
		del_timer(&tp->retransmit_timer);
	if(tp->delack_timer.prev)
		del_timer(&tp->delack_timer);
	if(tp->probe_timer.prev)
		del_timer(&tp->probe_timer);
}

static int ptcp_write_err(struct sock *sk, int force)
{
	sk->err = sk->err_soft ? sk->err_soft : ETIMEDOUT;
	sk->error_report(sk);
	
	ptcp_clear_xmit_timers(sk);
	
	/* Time wait the socket. */
	if (!force && ((1<<sk->state) & (TCPF_FIN_WAIT1|TCPF_FIN_WAIT2|TCPF_CLOSING))) {
		ptcp_time_wait(sk);
	} else {
		/* Clean up time. */
		ptcp_set_state(sk, TCP_CLOSE);
		return 0;
	}
	return 1;
}

/* A write timeout has occurred. Process the after effects. */
static int ptcp_write_timeout(struct sock *sk)
{
	struct tcp_opt *tp = &(sk->tp_pinfo.af_tcp);

	/* Look for a 'soft' timeout. */
	if ((sk->state == TCP_ESTABLISHED &&
	     tp->retransmits && (tp->retransmits % TCP_QUICK_TRIES) == 0) ||
	    (sk->state != TCP_ESTABLISHED && tp->retransmits > sysctl_tcp_retries1)) {
		dst_negative_advice(&sk->dst_cache);
	}
	
	/* Have we tried to SYN too many times (repent repent 8)) */
	if(tp->retransmits > sysctl_tcp_syn_retries && sk->state==TCP_SYN_SENT) {
		ptcp_write_err(sk, 1);
		/* Don't FIN, we got nothing back */
		return 0;
	}

	/* Has it gone just too far? */
	if (tp->retransmits > sysctl_tcp_retries2) 
		return ptcp_write_err(sk, 0);

	return 1;
}

void ptcp_delack_timer(unsigned long data)
{
	struct sock *sk = (struct sock*)data;

	if(!sk->zapped &&
	   sk->tp_pinfo.af_tcp.delayed_acks &&
	   sk->state != TCP_CLOSE) {
		/* If socket is currently locked, defer the ACK. */
		if (!atomic_read(&sk->sock_readers))
			ptcp_send_ack(sk);
		else
			ptcp_send_delayed_ack(&(sk->tp_pinfo.af_tcp), HZ/10);
	}
}

void ptcp_probe_timer(unsigned long data)
{
	struct sock *sk = (struct sock*)data;
	struct tcp_opt *tp = &sk->tp_pinfo.af_tcp;

	if(sk->zapped) 
		return;
	
	if (atomic_read(&sk->sock_readers)) {
		/* Try again later. */
		ptcp_reset_xmit_timer(sk, TIME_PROBE0, HZ/5);
		return;
	}

	/* *WARNING* RFC 1122 forbids this 
	 * It doesn't AFAIK, because we kill the retransmit timer -AK
	 * FIXME: We ought not to do it, Solaris 2.5 actually has fixing
	 * this behaviour in Solaris down as a bug fix. [AC]
	 */
	if (tp->probes_out > sysctl_tcp_retries2) {
		if(sk->err_soft)
			sk->err = sk->err_soft;
		else
			sk->err = ETIMEDOUT;
		sk->error_report(sk);

		if ((1<<sk->state) & (TCPF_FIN_WAIT1|TCPF_FIN_WAIT2|TCPF_CLOSING)) {
			/* Time wait the socket. */
			ptcp_time_wait(sk);
		} else {
			/* Clean up time. */
			ptcp_set_state(sk, TCP_CLOSE);
		}
	} else {
		/* Only send another probe if we didn't close things up. */
		ptcp_send_probe0(sk);
	}
}

static __inline__ int ptcp_keepopen_proc(struct sock *sk)
{
	int res = 0;

	if ((1<<sk->state) & (TCPF_ESTABLISHED|TCPF_CLOSE_WAIT|TCPF_FIN_WAIT2)) {
		struct tcp_opt *tp = &sk->tp_pinfo.af_tcp;
		__u32 elapsed = tcp_time_stamp - tp->rcv_tstamp;

		if (elapsed >= sysctl_tcp_keepalive_time) {
			if (tp->probes_out > sysctl_tcp_keepalive_probes) {
				if(sk->err_soft)
					sk->err = sk->err_soft;
				else
					sk->err = ETIMEDOUT;

				ptcp_set_state(sk, TCP_CLOSE);
				sk->shutdown = SHUTDOWN_MASK;
				if (!sk->dead)
					sk->state_change(sk);
			} else {
				tp->probes_out++;
				tp->pending = TIME_KEEPOPEN;
				ptcp_write_wakeup(sk);
				res = 1;
			}
		}
	}
	return res;
}

/* Kill off TIME_WAIT sockets once their lifetime has expired. */
int ptcp_tw_death_row_slot = 0;
static struct tcp_tw_bucket *ptcp_tw_death_row[TCP_TWKILL_SLOTS] =
	{ NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };

extern void ptcp_timewait_kill(struct tcp_tw_bucket *tw);

static void ptcp_twkill(unsigned long data)
{
	struct tcp_tw_bucket *tw;
	int killed = 0;

	tw = ptcp_tw_death_row[ptcp_tw_death_row_slot];
	ptcp_tw_death_row[ptcp_tw_death_row_slot] = NULL;
	while(tw != NULL) {
		struct tcp_tw_bucket *next = tw->next_death;

		ptcp_timewait_kill(tw);
		killed++;
		tw = next;
	}
	if(killed != 0) {
		struct tcp_sl_timer *slt = (struct tcp_sl_timer *)data;
		atomic_sub(killed, &slt->count);
	}
	ptcp_tw_death_row_slot =
	  ((ptcp_tw_death_row_slot + 1) & (TCP_TWKILL_SLOTS - 1));
}

/* These are always called from BH context.  See callers in
 * tcp_input.c to verify this.
 */
void ptcp_tw_schedule(struct tcp_tw_bucket *tw)
{
	int slot = (ptcp_tw_death_row_slot - 1) & (TCP_TWKILL_SLOTS - 1);
	struct tcp_tw_bucket **tpp = &ptcp_tw_death_row[slot];

	if((tw->next_death = *tpp) != NULL)
		(*tpp)->pprev_death = &tw->next_death;
	*tpp = tw;
	tw->pprev_death = tpp;

	tw->death_slot = slot;

	ptcp_inc_slow_timer(TCP_SLT_TWKILL);
}

/* Happens rarely if at all, no care about scalability here. */
void ptcp_tw_reschedule(struct tcp_tw_bucket *tw)
{
	struct tcp_tw_bucket **tpp;
	int slot;

	if(tw->next_death)
		tw->next_death->pprev_death = tw->pprev_death;
	*tw->pprev_death = tw->next_death;
	tw->pprev_death = NULL;

	slot = (ptcp_tw_death_row_slot - 1) & (TCP_TWKILL_SLOTS - 1);
	tpp = &ptcp_tw_death_row[slot];
	if((tw->next_death = *tpp) != NULL)
		(*tpp)->pprev_death = &tw->next_death;
	*tpp = tw;
	tw->pprev_death = tpp;

	tw->death_slot = slot;
	/* Timer was incremented when we first entered the table. */
}

/* This is for handling early-kills of TIME_WAIT sockets. */
void ptcp_tw_deschedule(struct tcp_tw_bucket *tw)
{
	if(tw->next_death)
		tw->next_death->pprev_death = tw->pprev_death;
	*tw->pprev_death = tw->next_death;
	tw->pprev_death = NULL;
	ptcp_dec_slow_timer(TCP_SLT_TWKILL);
}

/*
 *	Check all sockets for keepalive timer
 *	Called every 75 seconds
 *	This timer is started by af_inet init routine and is constantly
 *	running.
 *
 *	It might be better to maintain a count of sockets that need it using
 *	setsockopt/tcp_destroy_sk and only set the timer when needed.
 */

/*
 *	don't send over 5 keepopens at a time to avoid burstiness 
 *	on big servers [AC]
 */
#define MAX_KA_PROBES	5

extern int sysctl_tcp_max_ka_probes; /* = MAX_KA_PROBES */

/* Keepopen's are only valid for "established" TCP's, nicely our listener
 * hash gets rid of most of the useless testing, so we run through a couple
 * of the established hash chains each clock tick.  -DaveM
 *
 * And now, even more magic... TIME_WAIT TCP's cannot have keepalive probes
 * going off for them, so we only need check the first half of the established
 * hash table, even less testing under heavy load.
 *
 * I _really_ would rather do this by adding a new timer_struct to struct sock,
 * and this way only those who set the keepalive option will get the overhead.
 * The idea is you set it for 2 hours when the sock is first connected, when it
 * does fire off (if at all, most sockets die earlier) you check for the keepalive
 * option and also if the sock has been idle long enough to start probing.
 */
static void ptcp_keepalive(unsigned long data)
{
	static int chain_start = 0;
	int count = 0;
	int i;
	
	for(i = chain_start; i < (chain_start + ((TCP_HTABLE_SIZE/2) >> 2)); i++) {
		struct sock *sk = ptcp_established_hash[i];
		while(sk) {
			if(!atomic_read(&sk->sock_readers) && sk->keepopen) {
				count += ptcp_keepopen_proc(sk);
				if(count == sysctl_tcp_max_ka_probes)
					goto out;
			}
			sk = sk->next;
		}
	}
out:
	chain_start = ((chain_start + ((TCP_HTABLE_SIZE/2)>>2)) &
		       ((TCP_HTABLE_SIZE/2) - 1));
}

/*
 *	The TCP retransmit timer. This lacks a few small details.
 *
 *	1. 	An initial rtt timeout on the probe0 should cause what we can
 *		of the first write queue buffer to be split and sent.
 *	2.	On a 'major timeout' as defined by RFC1122 we shouldn't report
 *		ETIMEDOUT if we know an additional 'soft' error caused this.
 *		tcp_err should save a 'soft error' for us.
 *	[Unless someone has broken it then it does, except for one 2.0 
 *	broken case of a send when the route/device is directly unreachable,
 *	and we error but should retry! - FIXME] [AC]
 */

void ptcp_retransmit_timer(unsigned long data)
{
	struct sock *sk = (struct sock*)data;
	struct tcp_opt *tp = &sk->tp_pinfo.af_tcp;

	/* We are reset. We will send no more retransmits. */
	if(sk->zapped) {
		ptcp_clear_xmit_timer(sk, TIME_RETRANS);
		return;
	}

	if (atomic_read(&sk->sock_readers)) {
		/* Try again later */  
		ptcp_reset_xmit_timer(sk, TIME_RETRANS, HZ/20);
		return;
	}

	/* Clear delay ack timer. */
	ptcp_clear_xmit_timer(sk, TIME_DACK);

	/* Retransmission. */
	tp->retrans_head = NULL;
	tp->rexmt_done = 0;
	tp->fackets_out = 0;
	tp->retrans_out = 0;
	if (tp->retransmits == 0) {
		/* Remember window where we lost:
		 * "one half of the current window but at least 2 segments"
		 *
		 * Here "current window" means the effective one, which
		 * means it must be an accurate representation of our current
		 * sending rate _and_ the snd_wnd.
		 */
		tp->snd_ssthresh = ptcp_recalc_ssthresh(tp);
		tp->snd_cwnd_cnt = 0;
		tp->snd_cwnd = 1;
	}

	tp->retransmits++;

	tp->dup_acks = 0;
	tp->high_seq = tp->snd_nxt;
	ptcp_retransmit_skb(sk, skb_peek(&sk->write_queue));

	/* Increase the timeout each time we retransmit.  Note that
	 * we do not increase the rtt estimate.  rto is initialized
	 * from rtt, but increases here.  Jacobson (SIGCOMM 88) suggests
	 * that doubling rto each time is the least we can get away with.
	 * In KA9Q, Karn uses this for the first few times, and then
	 * goes to quadratic.  netBSD doubles, but only goes up to *64,
	 * and clamps at 1 to 64 sec afterwards.  Note that 120 sec is
	 * defined in the protocol as the maximum possible RTT.  I guess
	 * we'll have to use something other than TCP to talk to the
	 * University of Mars.
	 *
	 * PAWS allows us longer timeouts and large windows, so once
	 * implemented ftp to mars will work nicely. We will have to fix
	 * the 120 second clamps though!
	 */
	tp->backoff++;
	tp->rto = min(tp->rto << 1, 120*HZ);
	ptcp_reset_xmit_timer(sk, TIME_RETRANS, tp->rto);

	ptcp_write_timeout(sk);
}

/*
 *	Slow timer for SYN-RECV sockets
 */

/* This now scales very nicely. -DaveM */
static void ptcp_syn_recv_timer(unsigned long data)
{
	struct sock *sk;
	unsigned long now = jiffies;
	int i;

	for(i = 0; i < TCP_LHTABLE_SIZE; i++) {
		sk = ptcp_listening_hash[i];

		while(sk) {
			struct tcp_opt *tp = &sk->tp_pinfo.af_tcp;
			
			/* TCP_LISTEN is implied. */
			if (!atomic_read(&sk->sock_readers) && tp->syn_wait_queue) {
				struct open_request *prev = (struct open_request *)(&tp->syn_wait_queue);
				struct open_request *req = tp->syn_wait_queue;
				do {
					struct open_request *conn;
				  
					conn = req;
					req = req->dl_next;

					if (conn->sk ||
					    ((long)(now - conn->expires)) <= 0) {
						prev = conn; 
						continue; 
					}

					ptcp_synq_unlink(tp, conn, prev);
					if (conn->retrans >= sysctl_tcp_retries1) {
#ifdef TCP_DEBUG
						printk(KERN_DEBUG "syn_recv: "
						       "too many retransmits\n");
#endif
						(*conn->class->destructor)(conn);
						ptcp_dec_slow_timer(TCP_SLT_SYNACK);
						tp->syn_backlog--;
						ptcp_openreq_free(conn);

						if (!tp->syn_wait_queue)
							break;
					} else {
						unsigned long timeo;
						struct open_request *op; 

						(*conn->class->rtx_syn_ack)(sk, conn);

						conn->retrans++;
#ifdef TCP_DEBUG
						printk(KERN_DEBUG "syn_ack rtx %d\n",
						       conn->retrans);
#endif
						timeo = min((TCP_TIMEOUT_INIT 
							     << conn->retrans),
							    120*HZ);
						conn->expires = now + timeo;
						op = prev->dl_next; 
						ptcp_synq_queue(tp, conn);
						if (op != prev->dl_next)
							prev = prev->dl_next;
					}
					/* old prev still valid here */
				} while (req);
			}
			sk = sk->next;
		}
	}
}

void ptcp_sltimer_handler(unsigned long data)
{
	struct tcp_sl_timer *slt = ptcp_slt_array;
	unsigned long next = ~0UL;
	unsigned long now = jiffies;
	int i;

	for (i=0; i < TCP_SLT_MAX; i++, slt++) {
		if (atomic_read(&slt->count)) {
			long trigger;

			trigger = slt->period - ((long)(now - slt->last));

			if (trigger <= 0) {
				(*slt->handler)((unsigned long) slt);
				slt->last = now;
				trigger = slt->period;
			}

			/* Only reschedule if some events remain. */
			if (atomic_read(&slt->count))
				next = min(next, trigger);
		}
	}
	if (next != ~0UL)
		mod_timer(&ptcp_slow_timer, (now + next));
}

void __ptcp_inc_slow_timer(struct tcp_sl_timer *slt)
{
	unsigned long now = jiffies;
	unsigned long when;

	slt->last = now;

	when = now + slt->period;

	if (ptcp_slow_timer.prev) {
		if ((long)(ptcp_slow_timer.expires - when) >= 0)
			mod_timer(&ptcp_slow_timer, when);
	} else {
		ptcp_slow_timer.expires = when;
		add_timer(&ptcp_slow_timer);
	}
}
