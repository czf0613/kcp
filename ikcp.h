//=====================================================================
//
// KCP - A Better ARQ Protocol Implementation
// skywind3000 (at) gmail.com, 2010-2011
//
// Features:
// + Average RTT reduce 30% - 40% vs traditional ARQ like tcp.
// + Maximum RTT reduce three times vs tcp.
// + Lightweight, distributed as a single source file.
//
//=====================================================================
#ifndef __IKCP_H__
#define __IKCP_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

//=====================================================================
// QUEUE DEFINITION
//=====================================================================
#ifndef __IQUEUE_DEF__
#define __IQUEUE_DEF__

struct IQUEUEHEAD
{
	struct IQUEUEHEAD *next, *prev;
};

//---------------------------------------------------------------------
// queue init
//---------------------------------------------------------------------

void iqueue_init(struct IQUEUEHEAD *ptr)
{
	ptr->next = ptr;
	ptr->prev = ptr;
}

#define IOFFSETOF(TYPE, MEMBER) ((size_t)(&((TYPE *)0)->MEMBER))
#define ICONTAINEROF(ptr, type, member) ((type *)(((char *)ptr) - IOFFSETOF(type, member)))
#define iqueue_entry(ptr, type, member) ICONTAINEROF(ptr, type, member)

//---------------------------------------------------------------------
// queue operation
//---------------------------------------------------------------------

void iqueue_add(struct IQUEUEHEAD *node, struct IQUEUEHEAD *head)
{
	node->prev = head;
	node->next = head->next;
	head->next->prev = node;
	head->next = node;
}

void iqueue_add_tail(struct IQUEUEHEAD *node, struct IQUEUEHEAD *head)
{
	node->prev = head->prev;
	node->next = head;
	head->prev->next = node;
	head->prev = node;
}

void iqueue_del(struct IQUEUEHEAD *entry)
{
	entry->next->prev = entry->prev;
	entry->prev->next = entry->next;
	entry->next = NULL;
	entry->prev = NULL;
}

void iqueue_del_init(struct IQUEUEHEAD *entry)
{
	iqueue_del(entry);
	iqueue_init(entry);
}

bool iqueue_is_empty(const struct IQUEUEHEAD *entry)
{
	return entry->next == entry;
}

#endif

//---------------------------------------------------------------------
// BYTE ORDER
//---------------------------------------------------------------------
#ifndef IWORDS_BIG_ENDIAN

#ifdef _BIG_ENDIAN_
#if _BIG_ENDIAN_
#define IWORDS_BIG_ENDIAN 1
#endif
#endif

#ifndef IWORDS_BIG_ENDIAN
#if defined(__hppa__) ||                                           \
	defined(__m68k__) || defined(mc68000) || defined(_M_M68K) ||   \
	(defined(__MIPS__) && defined(__MIPSEB__)) ||                  \
	defined(__ppc__) || defined(__POWERPC__) || defined(_M_PPC) || \
	defined(__sparc__) || defined(__powerpc__) ||                  \
	defined(__mc68000__) || defined(__s390x__) || defined(__s390__)
#define IWORDS_BIG_ENDIAN 1
#endif
#endif

#ifndef IWORDS_BIG_ENDIAN
#define IWORDS_BIG_ENDIAN 0
#endif

#endif

//=====================================================================
// SEGMENT
//=====================================================================
struct IKCPSEG
{
	struct IQUEUEHEAD node;
	uint32_t conv;
	uint32_t cmd;
	uint32_t frg;
	uint32_t wnd;
	uint32_t ts;
	uint32_t sn;
	uint32_t una;
	uint32_t len;
	uint32_t resendts;
	uint32_t rto;
	uint32_t fastack;
	uint32_t xmit;
	uint8_t data[1];
};

//---------------------------------------------------------------------
// IKCPCB
//---------------------------------------------------------------------
struct IKCPCB
{
	uint32_t conv, mtu, mss, state;
	uint32_t snd_una, snd_nxt, rcv_nxt;
	uint32_t ts_recent, ts_lastack, ssthresh;
	int32_t rx_rttval, rx_srtt, rx_rto, rx_minrto;
	uint32_t snd_wnd, rcv_wnd, rmt_wnd, cwnd, probe;
	uint32_t current, interval, ts_flush, xmit;
	uint32_t nrcv_buf, nsnd_buf;
	uint32_t nrcv_que, nsnd_que;
	uint32_t nodelay, updated;
	uint32_t ts_probe, probe_wait;
	uint32_t dead_link, incr;
	struct IQUEUEHEAD snd_queue;
	struct IQUEUEHEAD rcv_queue;
	struct IQUEUEHEAD snd_buf;
	struct IQUEUEHEAD rcv_buf;
	uint32_t *acklist;
	uint32_t ackcount;
	uint32_t ackblock;
	void *user;
	uint8_t *buffer;
	int fastresend;
	int fastlimit;
	int nocwnd, stream;
	int logmask;
	int (*output)(const uint8_t *buf, int len, struct IKCPCB *kcp, void *user);
	void (*writelog)(const char *log, struct IKCPCB *kcp, void *user);
};

typedef struct IKCPCB ikcpcb;

#define IKCP_LOG_OUTPUT 1
#define IKCP_LOG_INPUT 2
#define IKCP_LOG_SEND 4
#define IKCP_LOG_RECV 8
#define IKCP_LOG_IN_DATA 16
#define IKCP_LOG_IN_ACK 32
#define IKCP_LOG_IN_PROBE 64
#define IKCP_LOG_IN_WINS 128
#define IKCP_LOG_OUT_DATA 256
#define IKCP_LOG_OUT_ACK 512
#define IKCP_LOG_OUT_PROBE 1024
#define IKCP_LOG_OUT_WINS 2048

#ifdef __cplusplus
extern "C"
{
#endif

	//---------------------------------------------------------------------
	// interface
	//---------------------------------------------------------------------

	// create a new kcp control object, 'conv' must equal in two endpoint
	// from the same connection. 'user' will be passed to the output callback
	// output callback can be setup like this: 'kcp->output = my_udp_output'
	ikcpcb *ikcp_create(uint32_t conv, void *user);

	// release kcp control object
	void ikcp_release(ikcpcb *kcp);

	// set output callback, which will be invoked by kcp
	void ikcp_setoutput(ikcpcb *kcp, int (*output)(const uint8_t *buf, int len,
												   ikcpcb *kcp, void *user));

	// user/upper level recv: returns size, returns below zero for EAGAIN
	int ikcp_recv(ikcpcb *kcp, uint8_t *buffer, int len);

	// user/upper level send, returns below zero for error
	int ikcp_send(ikcpcb *kcp, const uint8_t *buffer, int len);

	// update state (call it repeatedly, every 10ms-100ms), or you can ask
	// ikcp_check when to call it again (without ikcp_input/_send calling).
	// 'current' - current timestamp in millisec.
	void ikcp_update(ikcpcb *kcp, uint32_t current);

	// Determine when should you invoke ikcp_update:
	// returns when you should invoke ikcp_update in millisec, if there
	// is no ikcp_input/_send calling. you can call ikcp_update in that
	// time, instead of call update repeatly.
	// Important to reduce unnacessary ikcp_update invoking. use it to
	// schedule ikcp_update (eg. implementing an epoll-like mechanism,
	// or optimize ikcp_update when handling massive kcp connections)
	uint32_t ikcp_check(const ikcpcb *kcp, uint32_t current);

	// when you received a low level packet (eg. UDP packet), call it
	int ikcp_input(ikcpcb *kcp, const uint8_t *data, long size);

	// flush pending data
	void ikcp_flush(ikcpcb *kcp);

	// check the size of next message in the recv queue
	int ikcp_peeksize(const ikcpcb *kcp);

	// change MTU size, default is 1400
	int ikcp_setmtu(ikcpcb *kcp, int mtu);

	// set maximum window size: sndwnd=32, rcvwnd=32 by default
	int ikcp_wndsize(ikcpcb *kcp, int sndwnd, int rcvwnd);

	// get how many packet is waiting to be sent
	int ikcp_waitsnd(const ikcpcb *kcp);

	// fastest: ikcp_nodelay(kcp, 1, 20, 2, 1)
	// nodelay: 0:disable(default), 1:enable
	// interval: internal update timer interval in millisec, default is 100ms
	// resend: 0:disable fast resend(default), 1:enable fast resend
	// nc: 0:normal congestion control(default), 1:disable congestion control
	int ikcp_nodelay(ikcpcb *kcp, int nodelay, int interval, int resend, int nc);

	void ikcp_log(ikcpcb *kcp, int mask, const char *fmt, ...);

	// setup allocator
	void ikcp_allocator(void *(*new_malloc)(size_t), void (*new_free)(void *));

	// read conv
	uint32_t ikcp_getconv(const void *ptr);

#ifdef __cplusplus
}
#endif

#endif
