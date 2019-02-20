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

#define MAX_EVENTS      1
#define QUEUE_WORKERS   8

extern struct config    *gcfg;

static apr_thread_t     *thd_arr[QUEUE_WORKERS];
static apr_threadattr_t *thd_attr;
static struct pkt       *p[QUEUE_WORKERS];

static apr_pool_t       *mp;

static bool             is_running = false;

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
    int ret = read(gcfg->tun_fd, p->recv_buf, gcfg->recv_buf_size);

    if (ret < 0) {
        if (errno == EAGAIN) {
            return;
        }

        slog(LOG_ERR, "received error when reading from tun "
                "device: %s\n", strerror(errno));
        return;
    }

    if (ret < sizeof (struct tun_pi)) {
        slog(LOG_WARNING, "short read from tun device "
                "(%d bytes)\n", ret);
        return;
    }

    if (ret == gcfg->recv_buf_size) {
        slog(LOG_WARNING, "dropping oversized packet\n");
        return;
    }

    struct tun_pi *pi = (struct tun_pi *) p->recv_buf;

    memset(p, 0, sizeof (struct pkt));
    p->data = p->recv_buf + sizeof (struct tun_pi);
    p->data_len = ret - sizeof (struct tun_pi);
    p->proto = ntohs(pi->proto);

    queue_handle(p);
}

static void *APR_THREAD_FUNC queue_thread_handler(apr_thread_t *thd, void *data) {
    struct pollfd pollfds[1];
    
    memset(pollfds, 0, sizeof(pollfds));
    
    pollfds[0].fd = gcfg->tun_fd;
    pollfds[0].events = POLLIN;
    
    while (is_running) {
        int ret = poll(pollfds, 1, 1000);
        
        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            
            slog(LOG_ERR, "poll returned error %s\n",
            strerror(errno));
            exit(1);
        }
        
        if (pollfds[0].revents) {
            read_from_tun((struct pkt *) data);
        }
    }
    
    apr_thread_exit(thd, APR_SUCCESS);

    return NULL;
}

void queue_init(void) {
    apr_initialize();
    apr_pool_create(&mp, NULL);

    apr_threadattr_create(&thd_attr, mp);

    is_running = true;
    
    for (int i = 0; i < QUEUE_WORKERS; i++) {
        p[i] = (struct pkt *) malloc(sizeof (struct pkt) + gcfg->recv_buf_size);
        
        apr_thread_create(&thd_arr[i], thd_attr, queue_thread_handler, p[i], mp);
    }
}

void queue_shutdown(void) {
    if (!is_running) {
        return;
    }
    
    is_running = false;
    
    for (int i = 0; i < QUEUE_WORKERS; i++) {
        apr_status_t status;
        apr_thread_join(&status, thd_arr[i]);
        
        free(p[i]);
    }
}
