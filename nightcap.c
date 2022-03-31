/*
    Copyright (C) Corporation 9 Limited - All Rights Reserved
    Unauthorized copying of this file, via any medium is strictly prohibited
    Proprietary and confidential
*/

#include "nightcap.h"

static const char * TAG = "nightcap";

// ==== utils ====

// quantizes a time to the nearest multiple of snap, rounding down
uint32_t nightcap_floorsnap(uint32_t time, uint32_t snap) {
    return time - time % snap;
}

// quantizes a time to the nearest multiple of snap, rounding up. if time is already a multiple of snap, return time + snap.
uint32_t nightcap_ceilsnap(uint32_t time, uint32_t snap) {
    return time - time % snap + snap;
}

// schedules an event on a chain to run at a certain time
bool nightcap_schedule(nightcap_chain_t * chain, uint32_t time,
    void (*callback)(nightcap_event_t *), bool repeating) {
    
    nightcap_event_t * evt = nightcap_chain_get_unqueued_event(chain);

    if (evt == NULL) return false;

    nightcap_event_init(evt, time, callback, repeating);
    nightcap_chain_queue(chain, evt);

    return true;
}

// reschedules an event to run at a certain time on a chain
void nightcap_reschedule(nightcap_chain_t * chain, nightcap_event_t * evt, 
    uint32_t time) {
    nightcap_event_reinit(evt, time);
    nightcap_chain_queue(chain, evt);
}

// deep sleep until the next event, minus padding in microseconds
void nightcap_sleep_until_next_event(nightcap_chain_t * chain, uint64_t padding) {

    if (chain->head == NULL) {
        ESP_LOGE(TAG, "no events scheduled, not sleeping!");
        return;
    }

    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);

    uint64_t epoch_us = (uint64_t)tv_now.tv_sec * NIGHTCAP_S_TO_US + (uint64_t)tv_now.tv_usec;
    uint64_t target_epoch_us = (uint64_t)chain->head->time * NIGHTCAP_S_TO_US - padding;

    uint64_t sleep_time;

    if (target_epoch_us < epoch_us) {
        ESP_LOGW(TAG, "target time already passed, sleeping immediately");
        sleep_time = 0;
    } else {
        sleep_time = target_epoch_us - epoch_us;
    }

    ESP_LOGI(TAG, "sleeping %llu us until next event... (%llu to %llu)", 
        sleep_time, epoch_us, target_epoch_us);

    ESP_ERROR_CHECK( esp_sleep_enable_timer_wakeup(sleep_time) );
    esp_deep_sleep_start();
}

// ==== event ====

// clears and initializes a nightcap event
void nightcap_event_init(nightcap_event_t * evt, uint32_t time, 
    void (*callback)(nightcap_event_t *), bool repeating) {
    evt->flags.queued = true;
    evt->flags.repeating = repeating;
    evt->time = time;
    evt->callback = callback;
    evt->next = NULL;
}

// initializes a repeating nightcap event. to be used to reschedule during a callback.
void nightcap_event_reinit(nightcap_event_t * evt, uint32_t time) {
    evt->flags.queued = true;
    evt->time = time;
    evt->next = NULL;
}

// ==== chain ====

void nightcap_chain_init(nightcap_chain_t * chain) {
    memset(chain, 0, sizeof(nightcap_chain_t));
}

// gets a free event on a chain. if null, there are no free events (chain is full).
nightcap_event_t * nightcap_chain_get_unqueued_event(nightcap_chain_t * chain) {
    for (size_t i = 0; i < NIGHTCAP_MAX_EVENTS; i++) {
        if (!(chain->events[i].flags.queued)) return &(chain->events[i]);
    }
    return NULL;
}

// count how many events are currently queued for the nightcap scheduler.
size_t nightcap_chain_get_queued_event_count(nightcap_chain_t * chain) {
    size_t counter = 0;
    for (size_t i = 0; i < NIGHTCAP_MAX_EVENTS; i++) {
        counter += chain->events[i].flags.queued;
    }
    return counter;
}

// adds an event to a nightcap chain.
void nightcap_chain_queue(nightcap_chain_t * chain, nightcap_event_t * evt) {

    // if the current event comes before the head or the head is null, make the current event the head
    if (chain->head == NULL || evt->time < chain->head->time) {
        ESP_LOGI(TAG, "queued event at time %u (new head)", evt->time);
        evt->next = chain->head;
        chain->head = evt;
        chain->_mut_flag = true;
        return;
    }
    
    nightcap_event_t * traversal_ptr = chain->head;

    while (traversal_ptr->next != NULL) {
        // if the event comes after the traversal event, add the event after the traversal event.
        if (evt->time > traversal_ptr->time && evt->time < traversal_ptr->next->time) {
            ESP_LOGI(TAG, "queued event at time %u (insert)", evt->time);
            evt->next = traversal_ptr->next;
            break;
        }

        traversal_ptr = traversal_ptr->next; // advance the pointer
    }

    traversal_ptr->next = evt;
    chain->_mut_flag = true;
}

// dequeues all events that are overdue.
void nightcap_chain_dequeue(nightcap_chain_t * chain, uint32_t time) {
    while (chain->head != NULL && time >= chain->head->time) {
        nightcap_event_t * evt = chain->head;

        chain->head = chain->head->next; // advance the pointer

        chain->_mut_flag = false; // clear the mutated flag

        evt->callback(evt); // run the event
        evt->flags.queued &= evt->flags.repeating; // if the event repeats, don't clear the queued flag.

        if (chain->_mut_flag) { // if the chain was changed,
            // we should do something...
        }
    }
}
