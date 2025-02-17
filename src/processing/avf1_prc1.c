/*------------------------------------------------------------------------------
--  Solar Orbiter's Low Frequency Receiver Flight Software (LFR FSW),
--  This file is a part of the LFR FSW
--  Copyright (C) 2012-2018, Plasma Physics Laboratory - CNRS
--
--  This program is free software; you can redistribute it and/or modify
--  it under the terms of the GNU General Public License as published by
--  the Free Software Foundation; either version 2 of the License, or
--  (at your option) any later version.
--
--  This program is distributed in the hope that it will be useful,
--  but WITHOUT ANY WARRANTY; without even the implied warranty of
--  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
--  GNU General Public License for more details.
--
--  You should have received a copy of the GNU General Public License
--  along with this program; if not, write to the Free Software
--  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
-------------------------------------------------------------------------------*/
/*--                  Author : Paul Leroy
--                   Contact : Alexis Jeandet
--                      Mail : alexis.jeandet@lpp.polytechnique.fr
----------------------------------------------------------------------------*/

/** Functions related to data processing.
 *
 * @file
 * @author P. LEROY
 *
 * These function are related to data processing, i.e. spectral matrices averaging and basic
 * parameters computation.
 *
 */

#include "avf1_prc1.h"
#include "fsw_compile_warnings.h"
#include "fsw_debug.h"

typedef struct
{
    unsigned int norm_bp1;
    unsigned int norm_bp2;
    unsigned int norm_asm;
    unsigned int burst_sbm_bp1;
    unsigned int burst_sbm_bp2;
    unsigned int burst_bp1;
    unsigned int burst_bp2;
    unsigned int sbm2_bp1;
    unsigned int sbm2_bp2;
} nb_sm_before_bp_asm_f1;

DISABLE_MISSING_FIELD_INITIALIZER_WARNING
nb_sm_before_bp_asm_f1 nb_sm_before_f1 = { 0 };

//***
// F1
ring_node_asm asm_ring_norm_f1[NB_RING_NODES_ASM_NORM_F1] = { {0} };
ring_node_asm asm_ring_burst_sbm_f1[NB_RING_NODES_ASM_BURST_SBM_F1] = { {0} };
ring_node ring_to_send_asm_f1[NB_RING_NODES_ASM_F1] = { {0} };
ENABLE_MISSING_FIELD_INITIALIZER_WARNING

int buffer_asm_f1[NB_RING_NODES_ASM_F1 * TOTAL_SIZE_SM] = { 0 };

float asm_f1_patched_norm[TOTAL_SIZE_SM] = { 0 };
float asm_f1_patched_burst_sbm[TOTAL_SIZE_SM] = { 0 };
float asm_f1_reorganized[TOTAL_SIZE_SM] = { 0 };

float compressed_sm_norm_f1[TOTAL_SIZE_COMPRESSED_ASM_NORM_F1] = { 0 };
float compressed_sm_sbm_f1[TOTAL_SIZE_COMPRESSED_ASM_SBM_F1] = { 0 };


//************
// RTEMS TASKS

rtems_task avf1_task(rtems_task_argument lfrRequestedMode)
{

    rtems_event_set event_out;
    rtems_status_code status;
    rtems_id queue_id_prc1;
    asm_msg msgForPRC;
    ring_node* nodeForAveraging;
    ring_node* ring_node_tab[NB_SM_BEFORE_AVF0_F1];
    ring_node_asm* current_ring_node_asm_burst_sbm_f1;
    ring_node_asm* current_ring_node_asm_norm_f1;

    unsigned int nb_norm_bp1;
    unsigned int nb_norm_bp2;
    unsigned int nb_norm_asm;
    unsigned int nb_sbm_bp1;
    unsigned int nb_sbm_bp2;

    event_out = EVENT_SETS_NONE_PENDING;
    queue_id_prc1 = RTEMS_ID_NONE;

    nb_norm_bp1 = 0;
    nb_norm_bp2 = 0;
    nb_norm_asm = 0;
    nb_sbm_bp1 = 0;
    nb_sbm_bp2 = 0;

    reset_nb_sm_f1(lfrRequestedMode); // reset the sm counters that drive the BP and ASM
                                      // computations / transmissions
    ASM_generic_init_ring(asm_ring_norm_f1, NB_RING_NODES_ASM_NORM_F1);
    ASM_generic_init_ring(asm_ring_burst_sbm_f1, NB_RING_NODES_ASM_BURST_SBM_F1);
    current_ring_node_asm_norm_f1 = asm_ring_norm_f1;
    current_ring_node_asm_burst_sbm_f1 = asm_ring_burst_sbm_f1;

    BOOT_PRINTF("in AVF1 *** lfrRequestedMode = %d\n", (int)lfrRequestedMode);

    status = get_message_queue_id_prc1(&queue_id_prc1);
    DEBUG_CHECK_STATUS(status);
    if (status != RTEMS_SUCCESSFUL)
    {
        LFR_PRINTF("in AVF1 *** ERR get_message_queue_id_prc1 %d\n", status);
    }

    while (1)
    {
        status = rtems_event_receive(
            RTEMS_EVENT_0, RTEMS_WAIT, RTEMS_NO_TIMEOUT, &event_out); // wait for an RTEMS_EVENT0
        DEBUG_CHECK_STATUS(status);

        //****************************************
        // initialize the mesage for the MATR task
        msgForPRC.norm = current_ring_node_asm_norm_f1;
        msgForPRC.burst_sbm = current_ring_node_asm_burst_sbm_f1;
        msgForPRC.event
            = EVENT_SETS_NONE_PENDING; // this composite event will be sent to the PRC1 task
        //
        //****************************************

        nodeForAveraging = getRingNodeForAveraging(1);
        DEBUG_CHECK_PTR(nodeForAveraging);
        for (int i = NB_SM_BEFORE_AVF0_F1; i > 0; i--)
        {
            ring_node_tab[i - 1] = nodeForAveraging;
            nodeForAveraging = nodeForAveraging->previous;
        }

        // compute the average and store it in the averaged_sm_f1 buffer
        SM_average(current_ring_node_asm_norm_f1->matrix,
            current_ring_node_asm_burst_sbm_f1->matrix, ring_node_tab, nb_norm_bp1, nb_sbm_bp1,
            &msgForPRC, 1, ASM_F1_INDICE_START,
            ASM_F1_INDICE_START + ASM_F1_KEEP_BINS); // 1 => frequency channel 1

        // update nb_average
        nb_norm_bp1 = nb_norm_bp1 + NB_SM_BEFORE_AVF0_F1;
        nb_norm_bp2 = nb_norm_bp2 + NB_SM_BEFORE_AVF0_F1;
        nb_norm_asm = nb_norm_asm + NB_SM_BEFORE_AVF0_F1;
        nb_sbm_bp1 = nb_sbm_bp1 + NB_SM_BEFORE_AVF0_F1;
        nb_sbm_bp2 = nb_sbm_bp2 + NB_SM_BEFORE_AVF0_F1;

        if (nb_sbm_bp1 == nb_sm_before_f1.burst_sbm_bp1)
        {
            nb_sbm_bp1 = 0;
            // set another ring for the ASM storage
            current_ring_node_asm_burst_sbm_f1 = current_ring_node_asm_burst_sbm_f1->next;
            DEBUG_CHECK_PTR(current_ring_node_asm_burst_sbm_f1);
            if (lfrCurrentMode == LFR_MODE_BURST)
            {
                msgForPRC.event = msgForPRC.event | RTEMS_EVENT_BURST_BP1_F1;
            }
            else if (lfrCurrentMode == LFR_MODE_SBM2)
            {
                msgForPRC.event = msgForPRC.event | RTEMS_EVENT_SBM_BP1_F1;
            }
        }

        if (nb_sbm_bp2 == nb_sm_before_f1.burst_sbm_bp2)
        {
            nb_sbm_bp2 = 0;
            if (lfrCurrentMode == LFR_MODE_BURST)
            {
                msgForPRC.event = msgForPRC.event | RTEMS_EVENT_BURST_BP2_F1;
            }
            else if (lfrCurrentMode == LFR_MODE_SBM2)
            {
                msgForPRC.event = msgForPRC.event | RTEMS_EVENT_SBM_BP2_F1;
            }
        }

        if (nb_norm_bp1 == nb_sm_before_f1.norm_bp1)
        {
            nb_norm_bp1 = 0;
            // set another ring for the ASM storage
            current_ring_node_asm_norm_f1 = current_ring_node_asm_norm_f1->next;
            DEBUG_CHECK_PTR(current_ring_node_asm_norm_f1);
            if ((lfrCurrentMode == LFR_MODE_NORMAL) || (lfrCurrentMode == LFR_MODE_SBM1)
                || (lfrCurrentMode == LFR_MODE_SBM2))
            {
                msgForPRC.event = msgForPRC.event | RTEMS_EVENT_NORM_BP1_F1;
            }
        }

        if (nb_norm_bp2 == nb_sm_before_f1.norm_bp2)
        {
            nb_norm_bp2 = 0;
            if ((lfrCurrentMode == LFR_MODE_NORMAL) || (lfrCurrentMode == LFR_MODE_SBM1)
                || (lfrCurrentMode == LFR_MODE_SBM2))
            {
                msgForPRC.event = msgForPRC.event | RTEMS_EVENT_NORM_BP2_F1;
            }
        }

        if (nb_norm_asm == nb_sm_before_f1.norm_asm)
        {
            nb_norm_asm = 0;
            if ((lfrCurrentMode == LFR_MODE_NORMAL) || (lfrCurrentMode == LFR_MODE_SBM1)
                || (lfrCurrentMode == LFR_MODE_SBM2))
            {
                msgForPRC.event = msgForPRC.event | RTEMS_EVENT_NORM_ASM_F1;
            }
        }

        //*************************
        // send the message to PRC
        if (msgForPRC.event != EVENT_SETS_NONE_PENDING)
        {
            status
                = rtems_message_queue_send(queue_id_prc1, (char*)&msgForPRC, sizeof (asm_msg));
            DEBUG_CHECK_STATUS(status);
        }

        if (status != RTEMS_SUCCESSFUL)
        {
            LFR_PRINTF("in AVF1 *** Error sending message to PRC1, code %d\n", status);
        }
    }
}

rtems_task prc1_task(rtems_task_argument lfrRequestedMode)
{
    char incomingData[MSG_QUEUE_SIZE_SEND]; // incoming data buffer
    size_t size; // size of the incoming TC packet
    asm_msg* incomingMsg;
    //
    rtems_status_code status;
    rtems_id queue_id_send;
    rtems_id queue_id_q_p1;
    bp_packet_with_spare __attribute__((aligned(4))) packet_norm_bp1;
    bp_packet __attribute__((aligned(4))) packet_norm_bp2;
    bp_packet __attribute__((aligned(4))) packet_sbm_bp1;
    bp_packet __attribute__((aligned(4))) packet_sbm_bp2;
    ring_node* current_ring_node_to_send_asm_f1;
    float nbSMInASMNORM=0;
    float nbSMInASMSBM=0;

    size = 0;
    queue_id_send = RTEMS_ID_NONE;
    queue_id_q_p1 = RTEMS_ID_NONE;
    memset(&packet_norm_bp1, 0, sizeof(bp_packet_with_spare));
    memset(&packet_norm_bp2, 0, sizeof(bp_packet));
    memset(&packet_sbm_bp1, 0, sizeof(bp_packet));
    memset(&packet_sbm_bp2, 0, sizeof(bp_packet));

    // init the ring of the averaged spectral matrices which will be transmitted to the DPU
    init_ring(
        ring_to_send_asm_f1, NB_RING_NODES_ASM_F1, (volatile int*)buffer_asm_f1, TOTAL_SIZE_SM);
    current_ring_node_to_send_asm_f1 = ring_to_send_asm_f1;
    DEBUG_CHECK_PTR(current_ring_node_to_send_asm_f1);

    //*************
    // NORM headers
    BP_init_header_with_spare(&packet_norm_bp1, APID_TM_SCIENCE_NORMAL_BURST, SID_NORM_BP1_F1,
        PACKET_LENGTH_TM_LFR_SCIENCE_NORM_BP1_F1, NB_BINS_COMPRESSED_SM_F1);
    BP_init_header(&packet_norm_bp2, APID_TM_SCIENCE_NORMAL_BURST, SID_NORM_BP2_F1,
        PACKET_LENGTH_TM_LFR_SCIENCE_NORM_BP2_F1, NB_BINS_COMPRESSED_SM_F1);

    //***********************
    // BURST and SBM2 headers
    if (lfrRequestedMode == LFR_MODE_BURST)
    {
        BP_init_header(&packet_sbm_bp1, APID_TM_SCIENCE_NORMAL_BURST, SID_BURST_BP1_F1,
            PACKET_LENGTH_TM_LFR_SCIENCE_SBM_BP1_F1, NB_BINS_COMPRESSED_SM_SBM_F1);
        BP_init_header(&packet_sbm_bp2, APID_TM_SCIENCE_NORMAL_BURST, SID_BURST_BP2_F1,
            PACKET_LENGTH_TM_LFR_SCIENCE_SBM_BP2_F1, NB_BINS_COMPRESSED_SM_SBM_F1);
    }
    else if (lfrRequestedMode == LFR_MODE_SBM2)
    {
        BP_init_header(&packet_sbm_bp1, APID_TM_SCIENCE_SBM1_SBM2, SID_SBM2_BP1_F1,
            PACKET_LENGTH_TM_LFR_SCIENCE_SBM_BP1_F1, NB_BINS_COMPRESSED_SM_SBM_F1);
        BP_init_header(&packet_sbm_bp2, APID_TM_SCIENCE_SBM1_SBM2, SID_SBM2_BP2_F1,
            PACKET_LENGTH_TM_LFR_SCIENCE_SBM_BP2_F1, NB_BINS_COMPRESSED_SM_SBM_F1);
    }
    else
    {
        LFR_PRINTF("in PRC1 *** lfrRequestedMode is %d, several headers not initialized\n",
            (unsigned int)lfrRequestedMode);
    }

    status = get_message_queue_id_send(&queue_id_send);
    DEBUG_CHECK_STATUS(status);
    if (status != RTEMS_SUCCESSFUL)
    {
        LFR_PRINTF("in PRC1 *** ERR get_message_queue_id_send %d\n", status);
    }
    status = get_message_queue_id_prc1(&queue_id_q_p1);
    DEBUG_CHECK_STATUS(status);
    if (status != RTEMS_SUCCESSFUL)
    {
        LFR_PRINTF("in PRC1 *** ERR get_message_queue_id_prc1 %d\n", status);
    }

    BOOT_PRINTF("in PRC1 *** lfrRequestedMode = %d\n", (int)lfrRequestedMode);

    while (1)
    {
        status = rtems_message_queue_receive(queue_id_q_p1, incomingData,
            &size, //************************************
            RTEMS_WAIT, RTEMS_NO_TIMEOUT); // wait for a message coming from AVF0
        DEBUG_CHECK_STATUS(status);

        incomingMsg = (asm_msg*)incomingData;
        DEBUG_CHECK_PTR(incomingMsg);

        //***********
        //***********
        // BURST SBM2
        //***********
        //***********
        if ((incomingMsg->event & RTEMS_EVENT_BURST_BP1_F1)
            || (incomingMsg->event & RTEMS_EVENT_SBM_BP1_F1))
        {
            // ASM_patch( incomingMsg->burst_sbm->matrix, asm_f1_patched_burst_sbm );
            SM_calibrate_and_reorder_f1(incomingMsg->burst_sbm->matrix, asm_f1_patched_burst_sbm);
            nbSMInASMSBM = incomingMsg->numberOfSMInASMSBM;
            // 1)  compress the matrix for Basic Parameters calculation
            ASM_compress_divide_and_mask(asm_f1_patched_burst_sbm, compressed_sm_sbm_f1,
                nbSMInASMSBM, NB_BINS_COMPRESSED_SM_SBM_F1, NB_BINS_TO_AVERAGE_ASM_SBM_F1,
                ASM_F1_INDICE_START, CHANNELF1);
            // 2) compute the BP1 set
            //            BP1_set( compressed_sm_sbm_f1, k_coeff_intercalib_f1_sbm,
            //            NB_BINS_COMPRESSED_SM_SBM_F1, packet_sbm_bp1.data );
            compute_BP1(compressed_sm_sbm_f1, NB_BINS_COMPRESSED_SM_SBM_F1, packet_sbm_bp1.data);
            // 3) send the BP1 set
            set_time(packet_sbm_bp1.time, (unsigned char*)&incomingMsg->coarseTimeSBM);
            set_time(packet_sbm_bp1.acquisitionTime, (unsigned char*)&incomingMsg->coarseTimeSBM);
            packet_sbm_bp1.pa_bia_status_info = pa_bia_status_info;
            packet_sbm_bp1.sy_lfr_common_parameters
                = parameter_dump_packet.sy_lfr_common_parameters;
            BP_send_s1_s2((char*)&packet_sbm_bp1, queue_id_send,
                PACKET_LENGTH_TM_LFR_SCIENCE_SBM_BP1_F1 + PACKET_LENGTH_DELTA);
            // 4) compute the BP2 set if needed
            if ((incomingMsg->event & RTEMS_EVENT_BURST_BP2_F1)
                || (incomingMsg->event & RTEMS_EVENT_SBM_BP2_F1))
            {
                // 1) compute the BP2 set
                compute_BP2(
                    compressed_sm_sbm_f1, NB_BINS_COMPRESSED_SM_SBM_F1, packet_sbm_bp2.data);
                // 2) send the BP2 set
                set_time(packet_sbm_bp2.time, (unsigned char*)&incomingMsg->coarseTimeSBM);
                set_time(
                    packet_sbm_bp2.acquisitionTime, (unsigned char*)&incomingMsg->coarseTimeSBM);
                packet_sbm_bp2.pa_bia_status_info = pa_bia_status_info;
                packet_sbm_bp2.sy_lfr_common_parameters
                    = parameter_dump_packet.sy_lfr_common_parameters;
                BP_send_s1_s2((char*)&packet_sbm_bp2, queue_id_send,
                    PACKET_LENGTH_TM_LFR_SCIENCE_SBM_BP2_F1 + PACKET_LENGTH_DELTA);
            }
        }

        //*****
        //*****
        // NORM
        //*****
        //*****
        if ((incomingMsg->event & RTEMS_EVENT_NORM_BP1_F1)
            || (incomingMsg->event & RTEMS_EVENT_NORM_ASM_F1))
        {
            // ASM_patch( incomingMsg->norm->matrix,      asm_f1_patched_norm      );
            SM_calibrate_and_reorder_f1(incomingMsg->norm->matrix, asm_f1_patched_norm);
            nbSMInASMNORM = incomingMsg->numberOfSMInASMNORM;
        }

        if (incomingMsg->event & RTEMS_EVENT_NORM_BP1_F1)
        {
            // 1)  compress the matrix for Basic Parameters calculation
            ASM_compress_divide_and_mask(asm_f1_patched_norm, compressed_sm_norm_f1, nbSMInASMNORM,
                NB_BINS_COMPRESSED_SM_F1, NB_BINS_TO_AVERAGE_ASM_F1, ASM_F1_INDICE_START,
                CHANNELF1);
            // 2) compute the BP1 set
            // BP1_set(compressed_sm_norm_f1, k_coeff_intercalib_f1_norm, NB_BINS_COMPRESSED_SM_F1,
            //   packet_norm_bp1.data);
            compute_BP1(compressed_sm_norm_f1, NB_BINS_COMPRESSED_SM_F1, packet_norm_bp1.data);
            // 3) send the BP1 set
            set_time(packet_norm_bp1.time, (unsigned char*)&incomingMsg->coarseTimeNORM);
            set_time(packet_norm_bp1.acquisitionTime, (unsigned char*)&incomingMsg->coarseTimeNORM);
            packet_norm_bp1.pa_bia_status_info = pa_bia_status_info;
            packet_norm_bp1.sy_lfr_common_parameters
                = parameter_dump_packet.sy_lfr_common_parameters;
            BP_send((char*)&packet_norm_bp1, queue_id_send,
                PACKET_LENGTH_TM_LFR_SCIENCE_NORM_BP1_F1 + PACKET_LENGTH_DELTA);
            if (incomingMsg->event & RTEMS_EVENT_NORM_BP2_F1)
            {
                // 1) compute the BP2 set
                compute_BP2(compressed_sm_norm_f1, NB_BINS_COMPRESSED_SM_F1, packet_norm_bp2.data);
                // 2) send the BP2 set
                set_time(packet_norm_bp2.time, (unsigned char*)&incomingMsg->coarseTimeNORM);
                set_time(
                    packet_norm_bp2.acquisitionTime, (unsigned char*)&incomingMsg->coarseTimeNORM);
                packet_norm_bp2.pa_bia_status_info = pa_bia_status_info;
                packet_norm_bp2.sy_lfr_common_parameters
                    = parameter_dump_packet.sy_lfr_common_parameters;
                BP_send((char*)&packet_norm_bp2, queue_id_send,
                    PACKET_LENGTH_TM_LFR_SCIENCE_NORM_BP2_F1 + PACKET_LENGTH_DELTA);
            }
        }

        if (incomingMsg->event & RTEMS_EVENT_NORM_ASM_F1)
        {
            // 1) reorganize the ASM and divide
            ASM_divide(asm_f1_patched_norm,
                (float*)current_ring_node_to_send_asm_f1->buffer_address, nbSMInASMNORM,
                ASM_F1_INDICE_START, ASM_F1_INDICE_START + ASM_F1_KEEP_BINS);
            current_ring_node_to_send_asm_f1->coarseTime = incomingMsg->coarseTimeNORM;
            current_ring_node_to_send_asm_f1->fineTime = incomingMsg->fineTimeNORM;
            current_ring_node_to_send_asm_f1->sid = SID_NORM_ASM_F1;

            // 3) send the spectral matrix packets
            status = rtems_message_queue_send(
                queue_id_send, &current_ring_node_to_send_asm_f1, sizeof(ring_node*));
            DEBUG_CHECK_STATUS(status);

            // change asm ring node
            current_ring_node_to_send_asm_f1 = current_ring_node_to_send_asm_f1->next;
        }

        update_queue_max_count(queue_id_q_p1, &hk_lfr_q_p1_fifo_size_max);
    }
}

//**********
// FUNCTIONS

void reset_nb_sm_f1(unsigned char lfrMode)
{
    nb_sm_before_f1.norm_bp1 = parameter_dump_packet.sy_lfr_n_bp_p0 * NB_SM_PER_S_F1;
    nb_sm_before_f1.norm_bp2 = parameter_dump_packet.sy_lfr_n_bp_p1 * NB_SM_PER_S_F1;
    nb_sm_before_f1.norm_asm = ((parameter_dump_packet.sy_lfr_n_asm_p[0] * 256)
                                   + parameter_dump_packet.sy_lfr_n_asm_p[1])
        * NB_SM_PER_S_F1;
    nb_sm_before_f1.sbm2_bp1 = parameter_dump_packet.sy_lfr_s2_bp_p0 * NB_SM_PER_S_F1;
    nb_sm_before_f1.sbm2_bp2 = parameter_dump_packet.sy_lfr_s2_bp_p1 * NB_SM_PER_S_F1;
    nb_sm_before_f1.burst_bp1 = parameter_dump_packet.sy_lfr_b_bp_p0 * NB_SM_PER_S_F1;
    nb_sm_before_f1.burst_bp2 = parameter_dump_packet.sy_lfr_b_bp_p1 * NB_SM_PER_S_F1;

    if (lfrMode == LFR_MODE_SBM2)
    {
        nb_sm_before_f1.burst_sbm_bp1 = nb_sm_before_f1.sbm2_bp1;
        nb_sm_before_f1.burst_sbm_bp2 = nb_sm_before_f1.sbm2_bp2;
    }
    else if (lfrMode == LFR_MODE_BURST)
    {
        nb_sm_before_f1.burst_sbm_bp1 = nb_sm_before_f1.burst_bp1;
        nb_sm_before_f1.burst_sbm_bp2 = nb_sm_before_f1.burst_bp2;
    }
    else
    {
        nb_sm_before_f1.burst_sbm_bp1 = nb_sm_before_f1.burst_bp1;
        nb_sm_before_f1.burst_sbm_bp2 = nb_sm_before_f1.burst_bp2;
    }
}
