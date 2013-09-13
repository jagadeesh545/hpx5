#ifndef VERBS_CONNECT_H
#define VERBS_CONNECT_H

#include "verbs.h"

typedef struct verbs_cnct_ctx_t {
	char                      *ib_dev;
	int                        ib_port;
	struct ibv_context        *ib_context;
	struct ibv_pd             *ib_pd;
	struct ibv_cq             *ib_cq;
	struct ibv_srq            *ib_srq;
    struct ibv_comp_channel   *ib_cc;
	int                        ib_lid;
	ProcessInfo               *verbs_processes;

	struct rdma_event_channel *cm_channel;
	struct rdma_cm_id         *cm_id;
	void                      *local_priv;
	void                      *remote_priv;
	uint64_t                   local_priv_size;
	uint64_t                   remote_priv_size;

	int                        tx_depth;
	int                        rx_depth;
} verbs_cnct_ctx;

typedef struct verbs_cnct_info_t {
	int lid;
	int qpn;
	int psn;
    struct in_addr ip;
} verbs_cnct_info;

int __verbs_init_context(verbs_cnct_ctx *ctx);
int __verbs_connect_peers(verbs_cnct_ctx *crx);

#endif
