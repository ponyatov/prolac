#include <linux/skbuff.h>
#include <net/tcp.h>
#include <stddef.h>

extern int printf(const char *, ...);

int
main(int c, char **v)
{
  int i = offsetof(struct sk_buff, cb);

  printf("Base.Segment\n");
  printf("field _next :> *Segment @ %d;\n", offsetof(struct sk_buff, next));
  printf("field _prev :> *Segment @ %d;\n", offsetof(struct sk_buff, prev));
  printf("field _list :> *SegmentList @ %d;\n", offsetof(struct sk_buff, list));
  printf("field _sk :> *Socket @ %d;\n", offsetof(struct sk_buff, sk));
  printf("field _tcp_header :> *TCP @ %d;\n", offsetof(struct sk_buff, h.th));
  printf("field _ip_header :> *IP using all @ %d;\n", offsetof(struct sk_buff, nh.iph));
  printf("\n");
  printf("field _seq :> seqint @ %d;\n", i + offsetof(struct tcp_skb_cb, seq));
  printf("field _end_seq :> seqint @ %d;\n", i + offsetof(struct tcp_skb_cb, end_seq));
  printf("field _flags :> TCPFlags using all @ %d;\n", i + offsetof(struct tcp_skb_cb, flags));
  printf("field _urg_ptr :> ushort @ %d;\n", i + offsetof(struct tcp_skb_cb, urg_ptr));
  printf("field _ack_seq :> seqint @ %d;\n", i + offsetof(struct tcp_skb_cb, ack_seq));
  printf("\n");
  printf("field _len :> uint @ %d;\n", offsetof(struct sk_buff, len));
  printf("field _csum :> uint @ %d;\n", offsetof(struct sk_buff, csum));
  printf("field _used :> /*volatile*/ char @ %d;\n", offsetof(struct sk_buff, used));
  printf("field _pkt_type :> uchar @ %d;\n", offsetof(struct sk_buff, pkt_type));
  printf("field _ip_summed :> uchar @ %d;\n", offsetof(struct sk_buff, ip_summed));
  printf("field _head :> *uchar @ %d;\n", offsetof(struct sk_buff, head));
  printf("field _data :> *uchar @ %d;\n", offsetof(struct sk_buff, data));
  printf("field _tail :> *uchar @ %d;\n", offsetof(struct sk_buff, tail));
  printf("field _end :> *uchar @ %d;\n", offsetof(struct sk_buff, end));

  printf("\nSegmentList\n");
  printf("field _next :> *Segment @ %d;\n", offsetof(struct sk_buff_head, next));
  printf("field _prev :> *Segment @ %d;\n", offsetof(struct sk_buff_head, prev));
  printf("field _qlen :> uint @ %d;\n", offsetof(struct sk_buff_head, qlen));
  
  printf("\nBase.Socket\n");
  i = offsetof(struct sock, tp_pinfo.af_tcp);
  printf("field _local_port :> ushort @ %d;\n", offsetof(struct sock, sport));
  printf("field _remote_port :> ushort @ %d;\n", offsetof(struct sock, dport));
  printf("field _rcv_buf :> int @ %d;\n", offsetof(struct sock, rcvbuf));
  printf("field _sklist_next :> *TopSocket @ %d;\n", offsetof(struct sock, sklist_next));
  printf("field _sklist_prev :> *TopSocket @ %d;\n", offsetof(struct sock, sklist_prev));
  printf("field _rcv_queue :> SegmentList @ %d;\n", offsetof(struct sock, receive_queue));
  printf("field _snd_queue :> SegmentList @ %d;\n", offsetof(struct sock, write_queue));
  printf("field _back_log :> SegmentList @ %d;\n", offsetof(struct sock, back_log));

  printf("\nBase.TCB\n");
  i = offsetof(struct sock, tp_pinfo.af_tcp);
  printf("field _rcv_nxt :> seqint @ %d;\n", i + offsetof(struct tcp_opt, rcv_nxt));
  printf("field _snd_nxt :> seqint @ %d;\n", i + offsetof(struct tcp_opt, snd_nxt));
  printf("field _snd_una :> seqint @ %d;\n", i + offsetof(struct tcp_opt, snd_una));
  printf("field _snd_max :> seqint @ %d;\n", i + offsetof(struct tcp_opt, write_seq));
  printf("field _saddr :> seqint @ %d;\n", offsetof(struct sock, saddr));
  printf("field _daddr :> seqint @ %d;\n", offsetof(struct sock, daddr));
  printf("field _local_port :> ushort @ %d;\n", offsetof(struct sock, sport));
  printf("field _remote_port :> ushort @ %d;\n", offsetof(struct sock, dport));
  printf("field _state :> /*volatile*/ TCPState using all @ %d;\n", offsetof(struct sock, state));
  printf("field _irs :> seqint @ %d;\n", i + offsetof(struct tcp_opt, syn_seq));

  printf("\nRetransmitM.TCB\n");
  printf("field _out_of_order_queue :> SegmentList @ %d;\n", i + offsetof(struct tcp_opt, out_of_order_queue));
  
  printf("\nWindowM.TCB\n");
  i = offsetof(struct sock, tp_pinfo.af_tcp);
  printf("field _snd_wnd :> uint @ %d;\n", i + offsetof(struct tcp_opt, snd_wnd));
  printf("field _snd_wnd_seqno :> seqint @ %d;\n", i + offsetof(struct tcp_opt, snd_wl1));
  printf("field _snd_wnd_ackno :> seqint @ %d;\n", i + offsetof(struct tcp_opt, snd_wl2));
  printf("field _snd_wnd_max :> uint @ %d;\n", i + offsetof(struct tcp_opt, max_window));
  printf("field _rcv_wnd :> uint @ %d;\n", i + offsetof(struct tcp_opt, rcv_wnd));
  printf("field _rcv_wnd_seqno :> uint @ %d;\n", i + offsetof(struct tcp_opt, rcv_wup));
  
  printf("\nFastRetransmit.TCB\n");
  i = offsetof(struct sock, tp_pinfo.af_tcp);
  printf("field _duplicate_acks :> uchar @ %d;\n", i + offsetof(struct tcp_opt, dup_acks));

  printf("\nSlowStart.TCB\n");
  i = offsetof(struct sock, tp_pinfo.af_tcp);
  printf("field _snd_ssthresh :> uint @ %d;\n", i + offsetof(struct tcp_opt, snd_ssthresh));
  printf("field _snd_cwnd :> uint @ %d;\n", i + offsetof(struct tcp_opt, snd_cwnd));
  printf("field _snd_cwnd_fraction :> ushort @ %d;\n", i + offsetof(struct tcp_opt, snd_cwnd_cnt));

  printf("\nMSS.TCB\n");
  printf("field _device_mtu_cache :> uint @ %d;\n", i + offsetof(struct tcp_opt, pmtu_cookie));
  printf("field _mss_clamp :> ushort @ %d;\n", i + offsetof(struct tcp_opt, mss_clamp));
  printf("field _mss_cache :> ushort @ %d;\n", i + offsetof(struct tcp_opt, mss_cache));
  printf("field _user_mss :> ushort @ %d;\n", i + offsetof(struct tcp_opt, user_mss));

  printf("\nRTTSmoothM.TCB\n");
  printf("field _scaled_rtt :> uint @ %d;\n", i + offsetof(struct tcp_opt, srtt));
  printf("field _scaled_rtt_variance :> uint @ %d;\n", i + offsetof(struct tcp_opt, mdev));
  
  /* Pad things that we need */
  printf("\nMSS.Socket\n");
  printf("field _padding_1 :> *void @ %d;\n", offsetof(struct sock, dst_cache));

  printf("\nLinuxXtra.TCB\n");
  printf("field _sack_ok :> char @ %d;\n", i + offsetof(struct tcp_opt, sack_ok));
  printf("field _sack_above_padding :> uint @ %d;\n", i + offsetof(struct tcp_opt, probe_timer));

  return 0;
}
