/*
 *  queue.c -- queue stuff
 *
 *  part of TAYGA <http://www.litech.org/tayga/>
 *  Copyright (C) 2019 Christian Ruehringer
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#include <tayga.h>

#include <apr-1/apu.h>
#include <apr-1/apr_errno.h>
#include <apr-1/apr_pools.h>
#include <apr-1/apr_queue.h>
#include <apr-1/apr_general.h>
#include <apr-1/apr_thread_proc.h>

#define QUEUE_SIZE      1024 * 1024
#define QUEUE_WORKERS   8

static apr_queue_t *worker_queue;
static apr_pool_t *mp;
static apr_thread_mutex_t *mutex;

static void queue_handle(struct pkt *p) {
    switch (p->proto) {
        case ETH_P_IP:
            handle_ip4(p);
            break;

        case ETH_P_IPV6:
            handle_ip6(p);
            break;

        default:
            slog(LOG_WARNING, "Dropping unknown proto %04x from "
                    "tun device\n", p->proto);
            break;
    }
    
    free(p);
}

static void *APR_THREAD_FUNC doit(apr_thread_t *thd, void *data) {
    while (1) {
        struct pkt *p;
        
        apr_status_t res = apr_queue_pop(worker_queue, (void **)&p);
        
        if (res == APR_SUCCESS && p) {
            queue_handle(p);
        }
    }

    return NULL;
}

/*void queue_lock(void) {
    apr_thread_mutex_lock(mutex);
}

void queue_unlock(void) {
    apr_thread_mutex_unlock(mutex);
}
 */

void queue_init(void) {
    apr_thread_t * thd_arr[QUEUE_WORKERS];
    apr_threadattr_t *thd_attr;

    apr_initialize();
    apr_pool_create(&mp, NULL);

    apr_queue_create(&worker_queue, QUEUE_SIZE, mp);

    apr_thread_mutex_create(&mutex, APR_THREAD_MUTEX_NESTED, mp);
    
    apr_threadattr_create(&thd_attr, mp);

    for (int i = 0; i < QUEUE_WORKERS; i++) {
        apr_thread_create(&thd_arr[i], thd_attr, doit, NULL, mp);
    }
}

void queue_push(struct pkt *p) {
    apr_queue_push (worker_queue, (void *)p);
}
