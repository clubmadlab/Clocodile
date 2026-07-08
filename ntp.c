/****************************************************************************
* FILE:      ntp.c															*
* CONTENTS:  Network Time Protocol routines									*
* UPDATED:   28/05/26														*
****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "lwip/dns.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"

#include "common.h"
#include "main.h"
#include "ntp.h"


//---------------------------------------------------------------------------
// constants
//---------------------------------------------------------------------------

#define NTP_SERVER "pool.ntp.org"
#define NTP_MSG_LEN 48
#define NTP_PORT 123

#define NTP_RESEND_TIME_S 10

// seconds between 1 Jan 1900 and 1 Jan 1970
#define NTP_DELTA 2208988800


//---------------------------------------------------------------------------
// linkage
//---------------------------------------------------------------------------

extern uint32_t NTPUpdatePeriod;


//---------------------------------------------------------------------------
// variables
//---------------------------------------------------------------------------

typedef struct
{
	ip_addr_t ntp_server_address;
	struct udp_pcb* ntp_pcb;
	async_at_time_worker_t request_worker;
	async_at_time_worker_t resend_worker;
} NTP_T;

static bool NTP_enabled = true;
static NTP_T* NTP_state = NULL;


//---------------------------------------------------------------------------
// functions
//---------------------------------------------------------------------------

static void ntp_result(NTP_T* state, int status, time_t* result)
{
	if (status == 0 && result)
	{
		struct tm* utc = gmtime(result);
		SetTime(utc->tm_hour, utc->tm_min, utc->tm_sec);
		AdjustTime(0, 0, 1);
		printf("Received NTP UTC response: %02d/%02d/%04d %02d:%02d:%02d\n", utc->tm_mday, utc->tm_mon + 1,
		  utc->tm_year + 1900, utc->tm_hour, utc->tm_min, utc->tm_sec);
	}

	async_context_remove_at_time_worker(cyw43_arch_async_context(), &state->resend_worker);

	if (!NTP_enabled || NTPUpdatePeriod == 0) return;

	uint32_t period = status == 0 ? NTPUpdatePeriod : NTP_RESEND_TIME_S;

	hard_assert(async_context_add_at_time_worker_in_ms(cyw43_arch_async_context(), &state->request_worker, period * 1000));

	printf("Next request in %ds.\n", period);
}

// make an NTP request
static void ntp_request(NTP_T* state)
{
	cyw43_arch_lwip_begin();

	struct pbuf* p = pbuf_alloc(PBUF_TRANSPORT, NTP_MSG_LEN, PBUF_RAM);
	uint8_t* req = (uint8_t*) p->payload;
	memset(req, 0, NTP_MSG_LEN);
	req[0] = 0x1b;
	udp_sendto(state->ntp_pcb, p, &state->ntp_server_address, NTP_PORT);
	pbuf_free(p);

	cyw43_arch_lwip_end();
}

// callback with a DNS result
static void ntp_dns_found(const char* hostname, const ip_addr_t* ipaddr, void* arg)
{
    NTP_T* state = (NTP_T*) arg;

    if (ipaddr)
	{
        state->ntp_server_address = *ipaddr;
        printf("NTP address %s\n", ipaddr_ntoa(ipaddr));
        ntp_request(state);
    }
	else
	{
        printf("NTP DNS request failed.\n");
        ntp_result(state, -1, NULL);
    }
}

// NTP data received
static void ntp_recv(void* arg, struct udp_pcb* pcb, struct pbuf* p, const ip_addr_t* addr, u16_t port)
{
	NTP_T* state = (NTP_T*) arg;
	uint8_t mode = pbuf_get_at(p, 0) & 0x7;
	uint8_t stratum = pbuf_get_at(p, 1);

	if (ip_addr_cmp(addr, &state->ntp_server_address) && port == NTP_PORT && p->tot_len == NTP_MSG_LEN &&
	  mode == 0x4 && stratum != 0)
	{
		uint8_t seconds_buf[4] = {0};
		pbuf_copy_partial(p, seconds_buf, sizeof(seconds_buf), 40);
		uint32_t seconds_since_1900 = seconds_buf[0] << 24 | seconds_buf[1] << 16 | seconds_buf[2] << 8 | seconds_buf[3];
		uint32_t seconds_since_1970 = seconds_since_1900 - NTP_DELTA;
		time_t epoch = seconds_since_1970;
		ntp_result(state, 0, &epoch);
	}
	else
	{
		#ifdef DEBUG
		printf("Invalid NTP response.\n");
		#endif
		ntp_result(state, -1, NULL);
	}

	pbuf_free(p);
}

// make an NTP request
static void request_worker_fn(__unused async_context_t* context, async_at_time_worker_t* worker)
{
	if (!NTP_enabled) return;

	NTP_T* state = (NTP_T*) worker->user_data;

	if (!NTP_enabled) return;

	hard_assert(async_context_add_at_time_worker_in_ms(cyw43_arch_async_context(), &state->resend_worker,
	  NTP_RESEND_TIME_S * 1000));

	int err = dns_gethostbyname(NTP_SERVER, &state->ntp_server_address, ntp_dns_found, state);
	if (err == ERR_OK)
	{
		ntp_request(state);
	}
	else if (err != ERR_INPROGRESS)
	{
		printf("DNS request failed.\n");
		ntp_result(state, -1, NULL);
	}
}

// resend an NTP request if it appears to be lost
static void resend_worker_fn(__unused async_context_t* context, async_at_time_worker_t* worker)
{
	if (!NTP_enabled) return;

	NTP_T* state = (NTP_T*) worker->user_data;
	printf("NTP request failed.\n");
	ntp_result(state, -1, NULL);
}


// NTP initialisation
void InitNTP()
{
	NTP_state = (NTP_T*) calloc(1, sizeof(NTP_T));
	if (!NTP_state)
	{
		#ifdef DEBUG
		printf("Failed to allocate state.\n");
		#endif
		return;
	}

	NTP_state->ntp_pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
	if (!NTP_state->ntp_pcb)
	{
		#ifdef DEBUG
		printf("Failed to create PCB.\n");
		#endif
		free(NTP_state);
		return;
	}

	udp_recv(NTP_state->ntp_pcb, ntp_recv, NTP_state);
	NTP_state->request_worker.do_work = request_worker_fn;
	NTP_state->request_worker.user_data = NTP_state;
	NTP_state->resend_worker.do_work = resend_worker_fn;
	NTP_state->resend_worker.user_data = NTP_state;
}


// enables NTP
void EnableNTP()
{
	if (NTP_state == NULL) return;

	NTP_enabled = true;

	async_context_remove_at_time_worker(cyw43_arch_async_context(), &NTP_state->request_worker);

	hard_assert(async_context_add_at_time_worker_in_ms(cyw43_arch_async_context(), &NTP_state->request_worker, 0));
}

// disables NTP
void DisableNTP()
{
	NTP_enabled = false;
}


// sets the NTP update period in seconds
void SetNTPUpdatePeriod(uint32_t secs)
{
	NTPUpdatePeriod = secs;

	SaveSettings();

	printf("NTP update period set to: %d seconds\n", NTPUpdatePeriod);

	if (NTPUpdatePeriod == 0) DisableNTP(); else EnableNTP();
}
