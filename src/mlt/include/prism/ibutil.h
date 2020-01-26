#ifndef PRISM_IBUTIL_H_
#define PRISM_IBUTIL_H_

#include <infiniband/verbs.h>
#include <stdio.h>

namespace prism {

namespace ibutil {

inline const char *ibv_wr_opcode_string(int opcode)
{
    switch (opcode) {
    case IBV_WR_RDMA_WRITE:
        return "IBV_WR_RDMA_WRITE";
    case IBV_WR_RDMA_WRITE_WITH_IMM:
        return "IBV_WR_RDMA_WRITE_WITH_IMM";
    case IBV_WR_SEND:
        return "IBV_WR_SEND";
    case IBV_WR_SEND_WITH_IMM:
        return "IBV_WR_SEND_WITH_IMM";
    case IBV_WR_RDMA_READ:
        return "IBV_WR_RDMA_READ";
    case IBV_WR_ATOMIC_CMP_AND_SWP:
        return "IBV_WR_ATOMIC_CMP_AND_SWP";
    case IBV_WR_ATOMIC_FETCH_AND_ADD:
        return "IBV_WR_ATOMIC_FETCH_AND_ADD";
    default:
        return "Unknown-wr-opcode";
    }

}

inline const char *ibv_wc_opcode_string(int opcode)
{
    switch (opcode) {
    case IBV_WC_SEND:
        return "IBV_WC_SEND";
    case IBV_WC_RDMA_WRITE:
        return "IBV_WC_RDMA_WRITE";
    case IBV_WC_RDMA_READ:
        return "IBV_WC_RDMA_READ";
    case IBV_WC_COMP_SWAP:
        return "IBV_WC_COMP_SWAP";
    case IBV_WC_FETCH_ADD:
        return "IBV_WC_FETCH_ADD";
    case IBV_WC_BIND_MW:
        return "IBV_WC_BIND_MW";

        //// recv-side: inbound completion
    case IBV_WC_RECV:
        return "IBV_WC_RECV";
    case IBV_WC_RECV_RDMA_WITH_IMM:
        return "IBV_WC_RECV_RDMA_WITH_IMM";
    default:
        return "Unknow-wc-opcode";
    }
}

inline const char *ibv_mtu_string(enum ibv_mtu mtu)
{
    switch (mtu) {
    case IBV_MTU_256:
        return "IBV_MTU_256";
    case IBV_MTU_512:
        return "IBV_MTU_512";
    case IBV_MTU_1024:
        return "IBV_MTU_1024";
    case IBV_MTU_2048:
        return "IBV_MTU_2048";
    case IBV_MTU_4096:
        return "IBV_MTU_4096";
    default:
        return "unknown mut";
    }
/* 147 enum ibv_mtu {
 148     IBV_MTU_256  = 1,
 149     IBV_MTU_512  = 2,
 150     IBV_MTU_1024 = 3,
 151     IBV_MTU_2048 = 4,
 152     IBV_MTU_4096 = 5
 153 };    */
}

inline const char *ibv_port_state_string(enum ibv_port_state state)
{
    return ibv_port_state_str(state);

    switch (state) {
    case IBV_PORT_NOP:
        return "IBV_PORT_NOP";
    case IBV_PORT_DOWN:
        return "IBV_PORT_DOWN";
    case IBV_PORT_INIT:
        return "IBV_PORT_INIT";
    case IBV_PORT_ARMED:
        return "IBV_PORT_ARMED";
    case IBV_PORT_ACTIVE:
        return "IBV_PORT_ACTIVE";
    case IBV_PORT_ACTIVE_DEFER:
        return "IBV_PORT_ACTIVE_DEFER";
    default:
        return "unknown port state";
    }
}

const char *ibv_width_string(uint8_t width)
{
    switch (width) {
    case 1:
        return "1";
    case 2:
        return "4";
    case 4:
        return "8";
    case 8:
        return "12";
    default:
        return "invalid width";
    }
}

const char *ibv_speed_string(uint8_t speed)
{
    switch (speed) {
    case 1:
        return "2.5 Gbps";
    case 2:
        return "5.0 Gbps";
    case 4:
    case 8:
        return "10.0 Gbps";
    case 16:
        return "14.0 Gbps";
    case 32:
        return "25.0 Gbps";
    default:
        return "invalid speed";
    }
}

const char *ibv_vl_string(uint8_t vl_num)
{
    switch (vl_num) {
    case 1:
        return "1";
    case 2:
        return "2";
    case 3:
        return "4";
    case 4:
        return "8";
    case 5:
        return "15";
    default:
        return "invalid value";
    }
}

inline const char *ibv_port_phy_state_string(uint8_t phys_state)
{
    switch (phys_state) {
    case 1:
        return "SLEEP";
    case 2:
        return "POLLING";
    case 3:
        return "DISABLED";
    case 4:
        return "PORT_CONFIGURATION TRAINNING";
    case 5:
        return "LINK_UP";
    case 6:
        return "LINK_ERROR_RECOVERY";
    case 7:
        return "PHY TEST";
    default:
        return "invalid physical state";
    }
}

void dump_wc(struct ibv_wc *wc)
{
    printf("CQ::  wr_id=0x%lx, wc_opcode=%s, wc_status=%s, wc_flag=0x%x\n", wc->wr_id, ibv_wc_opcode_string(wc->opcode), ibv_wc_status_str(wc->status), wc->wc_flags);
    printf("      byte_len=%u, immdata=%u, qp_num=0x%x, src_qp=%u\n", wc->byte_len, wc->imm_data, wc->qp_num, wc->src_qp);
}

void dump_send_wr(struct ibv_send_wr *wr)
{
    printf("SQ WR::  wr_id=0x%lx, opcode=%s, num_sge=%d, send_flags=%u, immdata=%u\n", wr->wr_id, ibv_wr_opcode_string(wr->opcode), wr->num_sge, wr->send_flags, wr->imm_data);

    if (wr->opcode == IBV_WR_RDMA_WRITE) {

    } else if (wr->opcode == IBV_WR_RDMA_WRITE_WITH_IMM) {

    } else if (wr->opcode == IBV_WR_RDMA_READ) {

    } else if (wr->opcode == IBV_WR_ATOMIC_CMP_AND_SWP) {

    } else if (wr->opcode == IBV_WR_ATOMIC_FETCH_AND_ADD) {

    }

}

void dump_ibv_device_attr(struct ibv_device_attr *attr)
{
    printf("\n==== device attr ====\n");
    printf("\tfw_ver: %s\n", attr->fw_ver);
    printf("\tnode_guid: 0x%llx\n", (unsigned long long)attr->node_guid);
    printf("\tsys_image_guid: 0x%llx\n", (unsigned long long)attr->sys_image_guid);
    printf("\tmax_mr_size: 0x%lx\n", attr->max_mr_size);    /* Largest contiguous block that can be registered */
    printf("\tpage_size_cap: %lu\n", attr->page_size_cap);  /* Supported memory shift sizes */
    printf("\thw_ver:  %u\n", attr->hw_ver);
    printf("\tmax_qp:  %d\n", attr->max_qp);
    printf("\tmax_qp_wr:  %d\n", attr->max_qp_wr);
    printf("\tmax_sge:  %d\n", attr->max_sge);
    printf("\tmax_cq:  %d\n", attr->max_cq);
    printf("\tmax_cqe:  %d\n", attr->max_cqe);
    printf("\tmax_mr:  %d\n", attr->max_mr);
    printf("\tmax_pd:  %d\n", attr->max_pd);
    printf("\tmax_qp_rd_atom:  %d\n", attr->max_qp_rd_atom);    /* Maximum number of RDMA Read & Atomic operations that can be outstanding per QP */
    printf("\tmax_res_rd_atom:  %d\n", attr->max_res_rd_atom);  /* Maximum number of resources used for RDMA Read & Atomic operations by this HCA as the Target */
    printf("\tmax_qp_init_rd_atom:  %d\n", attr->max_qp_init_rd_atom);  /* Maximum depth per QP for initiation of RDMA Read & Atomic operations */
    printf("\tphys_port_cnt:  %d\n", attr->phys_port_cnt);
    printf("\t\n");
    printf("==============\n");
    //////////

}

void dump_ibv_port_attr(struct ibv_port_attr *attr)
{
    printf("\n==========  port attr ==========\n");
    printf("\tstate: %d  (%s)\n", attr->state, ibv_port_state_string(attr->state));
    printf("\tmax_mtu:  %s\n", ibv_mtu_string(attr->max_mtu));
    printf("\tactive_mtu:  %s\n", ibv_mtu_string(attr->active_mtu));
    printf("\tgid_tbl_len:  %d\n", attr->gid_tbl_len);  /* Length of source GID table */
    printf("\tport_cap_flags:  0x %x\n", attr->port_cap_flags);
    printf("\tmax_msg_sz:  %u\n", attr->max_msg_sz);
    printf("\tlid:  %d\n", attr->lid);  /* Base port LID */
    printf("\tsm_lid:  %d\n", attr->sm_lid);
    printf("\tlmc:  %d\n", attr->lmc);  /* LMC of LID */
    printf("\tmax_vl_num:  %d (%s)\n", attr->max_vl_num, ibv_vl_string(attr->max_vl_num));  /* Maximum number of VLs */
    printf("\tsm_sl:  %d\n", attr->sm_sl);  /* SM service level */
    printf("\tactive_width:  %d (%s)\n", attr->active_width, ibv_width_string(attr->active_width)); /* Currently active link width */
    printf("\tactive_speed:  %d (%s)\n", attr->active_speed, ibv_speed_string(attr->active_speed));
    printf("\tphys_state:  %d (%s)\n", attr->phys_state, ibv_port_phy_state_string(attr->phys_state));
    printf("==============\n");

}


}

}  // namespace prism

#endif  // PRISM_IBUTIL_H_