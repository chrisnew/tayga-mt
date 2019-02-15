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

#define QUEUE_SIZE      1024 * 1024 * 128
#define QUEUE_WORKERS   8

extern struct config *gcfg;

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
}

static void read_from_tun(struct pkt *p) {
    int ret;
    // struct pkt *p = (struct pkt *) malloc(sizeof (struct pkt) + gcfg->recv_buf_size);

    ret = read(gcfg->tun_fd, p->recv_buf, gcfg->recv_buf_size);
    if (ret < 0) {
        if (errno == EAGAIN)
            goto error_return;
        slog(LOG_ERR, "received error when reading from tun "
                "device: %s\n", strerror(errno));
        goto error_return;
    }
    if (ret < sizeof (struct tun_pi)) {
        slog(LOG_WARNING, "short read from tun device "
                "(%d bytes)\n", ret);
        goto error_return;
    }
    if (ret == gcfg->recv_buf_size) {
        slog(LOG_WARNING, "dropping oversized packet\n");
        goto error_return;
    }

    struct tun_pi *pi = (struct tun_pi *) p->recv_buf;

    memset(p, 0, sizeof (struct pkt));
    p->data = p->recv_buf + sizeof (struct tun_pi);
    p->data_len = ret - sizeof (struct tun_pi);
    p->proto = ntohs(pi->proto);

    queue_handle(p);
    return;

error_return:
    return;
}

static void *APR_THREAD_FUNC doit(apr_thread_t *thd, void *data) {
    struct pollfd pollfds[1];
    
    memset(pollfds, 0, sizeof(pollfds));
    
    pollfds[0].fd = gcfg->tun_fd;
    pollfds[0].events = POLLIN;
    
    while (1) {
        int ret = poll(pollfds, 1, POOL_CHECK_INTERVAL * 1000);
        
        if (pollfds[1].revents) {
            read_from_tun((struct pkt *) data);
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

//    apr_queue_create(&worker_queue, QUEUE_SIZE, mp);

//    apr_thread_mutex_create(&mutex, APR_THREAD_MUTEX_NESTED, mp);

    apr_threadattr_create(&thd_attr, mp);

    for (int i = 0; i < QUEUE_WORKERS; i++) {
        struct pkt *p = (struct pkt *) malloc(sizeof (struct pkt) + gcfg->recv_buf_size);
        
        apr_thread_create(&thd_arr[i], thd_attr, doit, p, mp);
    }
}

void queue_push(struct pkt *p) {
    apr_queue_push(worker_queue, (void *) p);
}

void queue_notify(void) {
    apr_queue_push(worker_queue, NULL);
}
