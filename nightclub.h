/*
    Copyright (C) Corporation 9 Limited - All Rights Reserved
    Unauthorized copying of this file, via any medium is strictly prohibited
    Proprietary and confidential
*/

#ifndef _NIGHTCAP_H_
#define _NIGHTCAP_H_

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <sys/time.h>

#include "esp_sleep.h"
#include "esp_log.h"

#define NIGHTCAP_MAX_EVENTS (16)

#define NIGHTCAP_S_TO_US (1000000ull)
#define NIGHTCAP_S_TO_MS (1000ull)
#define NIGHTCAP_MS_TO_US (1000ull)

typedef struct _nightcap_flags_t nightcap_flags_t;
typedef struct _nightcap_event_t nightcap_event_t;
typedef struct _nightcap_chain_t nightcap_chain_t;

typedef struct _nightcap_flags_t {
    uint8_t queued : 1; // set when the event is to be executed
    uint8_t repeating : 1; // set when the event shouldn't be dequeued after it fires
} nightcap_event_flags_t;

typedef struct _nightcap_event_t {
    nightcap_event_flags_t flags;
    const char * name;
    uint32_t time;
    void (*callback)(nightcap_event_t *);
    void * params;
    nightcap_event_t * next;
} nightcap_event_t;

typedef struct _nightcap_chain_t {
    uint8_t _mut_flag : 1; // set this bit when the chain is updated in a callback
    nightcap_event_t * head; // a pointer to the latest event to be executed
    nightcap_event_t events[NIGHTCAP_MAX_EVENTS];
} nightcap_chain_t;

// ==== utils ====
uint32_t nightcap_floorsnap(uint32_t time, uint32_t snap);
uint32_t nightcap_ceilsnap(uint32_t time, uint32_t snap);

bool nightcap_schedule(nightcap_chain_t * chain, uint32_t time,
    void (*callback)(nightcap_event_t *), bool repeating);

void nightcap_reschedule(nightcap_chain_t * chain, nightcap_event_t * event, 
    uint32_t time);

void nightcap_sleep_until_next_event(nightcap_chain_t * chain, uint64_t padding);

// ==== event ====
void nightcap_event_init(nightcap_event_t * evt, uint32_t time, 
    void (*callback)(nightcap_event_t *), bool repeating);
void nightcap_event_reinit(nightcap_event_t * evt, uint32_t time);

// ==== chain ====
void nightcap_chain_init(nightcap_chain_t * chain);

nightcap_event_t * nightcap_chain_get_unqueued_event(nightcap_chain_t * chain);
size_t nightcap_chain_get_queued_event_count(nightcap_chain_t * chain);
void nightcap_chain_queue(nightcap_chain_t * chain, nightcap_event_t * evt);
void nightcap_chain_dequeue(nightcap_chain_t * chain, uint32_t time);

#endif
