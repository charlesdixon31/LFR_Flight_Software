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

#include "avf2_prc2.h"
#include "fsw_compile_warnings.h"
#include "fsw_debug.h"
#include "mitigations/PAS_filtering.h"

typedef struct
{
    unsigned int norm_bp1;
    unsigned int norm_bp2;
    unsigned int norm_asm;
} nb_sm_before_bp_asm_f2;

DISABLE_MISSING_FIELD_INITIALIZER_WARNING
nb_sm_before_bp_asm_f2 nb_sm_before_f2 = { 0 };

//***
// F2
ring_node_asm asm_ring_norm_f2[NB_RING_NODES_ASM_NORM_F2] = { { 0 } };
ring_node ring_to_send_asm_f2[NB_RING_NODES_ASM_F2] = { { 0 } };
ENABLE_MISSING_FIELD_INITIALIZER_WARNING

int buffer_asm_f2[NB_RING_NODES_ASM_F2 * TOTAL_SIZE_SM] = { 0 };

float asm_f2_patched_norm[TOTAL_SIZE_SM] = { 0 };
float asm_f2_reorganized[TOTAL_SIZE_SM] = { 0 };

float compressed_sm_norm_f2[TOTAL_SIZE_COMPRESSED_ASM_NORM_F2] = { 0 };


//************
// RTEMS TASKS

//***
// F2
rtems_task avf2_task(rtems_task_argument argument)
{
    IGNORE_UNUSED_PARAMETER(argument);
    rtems_event_set event_out;
    rtems_status_code status;
    rtems_id queue_id_prc2;
    asm_msg msgForPRC;
    ring_node* nodeForAveraging;
    ring_node_asm* current_ring_node_asm_norm_f2;

    unsigned int nb_norm_bp1;
    unsigned int nb_norm_bp2;
    unsigned int nb_norm_asm;

    event_out = EVENT_SETS_NONE_PENDING;
    queue_id_prc2 = RTEMS_ID_NONE;
    nb_norm_bp1 = 0;
    nb_norm_bp2 = 0;
    nb_norm_asm = 0;

    reset_nb_sm_f2(); // reset the sm counters that drive the BP and ASM computations /
                      // transmissions
    ASM_generic_init_ring(asm_ring_norm_f2, NB_RING_NODES_ASM_NORM_F2);
    current_ring_node_asm_norm_f2 = asm_ring_norm_f2;
    DEBUG_CHECK_PTR(current_ring_node_asm_norm_f2);

    BOOT_PRINTF("in AVF2 ***\n");

    status = get_message_queue_id_prc2(&queue_id_prc2);
    DEBUG_CHECK_STATUS(status);
    if (status != RTEMS_SUCCESSFUL)
    {
        LFR_PRINTF("in AVF2 *** ERR get_message_queue_id_prc2 %d\n", status);
    }

    while (1)
    {
        status = rtems_event_receive(
            RTEMS_EVENT_0, RTEMS_WAIT, RTEMS_NO_TIMEOUT, &event_out); // wait for an RTEMS_EVENT0
        DEBUG_CHECK_STATUS(status);

        //****************************************
        // initialize the mesage for the MATR task
        msgForPRC.norm = current_ring_node_asm_norm_f2;
        msgForPRC.burst_sbm = NULL;
        msgForPRC.event
            = EVENT_SETS_NONE_PENDING; // this composite event will be sent to the PRC2 task
        //
        //****************************************

        nodeForAveraging = getRingNodeForAveraging(CHANNELF2);

        DEBUG_PRINTF("in AVF2 *** got new SM\n");
        // compute the average and store it in the averaged_sm_f2 buffer
        SM_average_f2(
            current_ring_node_asm_norm_f2->matrix, nodeForAveraging, nb_norm_bp1, &msgForPRC);

        // update nb_average
        nb_norm_bp1 = nb_norm_bp1 + NB_SM_BEFORE_AVF2;
        nb_norm_bp2 = nb_norm_bp2 + NB_SM_BEFORE_AVF2;
        nb_norm_asm = nb_norm_asm + NB_SM_BEFORE_AVF2;

        if (nb_norm_bp1 == nb_sm_before_f2.norm_bp1)
        {
            nb_norm_bp1 = 0;
            // set another ring for the ASM storage
            current_ring_node_asm_norm_f2 = current_ring_node_asm_norm_f2->next;
            DEBUG_CHECK_PTR(current_ring_node_asm_norm_f2);
            if ((lfrCurrentMode == LFR_MODE_NORMAL) || (lfrCurrentMode == LFR_MODE_SBM1)
                || (lfrCurrentMode == LFR_MODE_SBM2))
            {
                msgForPRC.event = msgForPRC.event | RTEMS_EVENT_NORM_BP1_F2;
            }
        }

        if (nb_norm_bp2 == nb_sm_before_f2.norm_bp2)
        {
            nb_norm_bp2 = 0;
            if ((lfrCurrentMode == LFR_MODE_NORMAL) || (lfrCurrentMode == LFR_MODE_SBM1)
                || (lfrCurrentMode == LFR_MODE_SBM2))
            {
                msgForPRC.event = msgForPRC.event | RTEMS_EVENT_NORM_BP2_F2;
            }
        }

        if (nb_norm_asm == nb_sm_before_f2.norm_asm)
        {
            nb_norm_asm = 0;
            if ((lfrCurrentMode == LFR_MODE_NORMAL) || (lfrCurrentMode == LFR_MODE_SBM1)
                || (lfrCurrentMode == LFR_MODE_SBM2))
            {
                msgForPRC.event = msgForPRC.event | RTEMS_EVENT_NORM_ASM_F2;
            }
        }

        //*************************
        // send the message to PRC2
        if (msgForPRC.event != EVENT_SETS_NONE_PENDING)
        {
            status = rtems_message_queue_send(queue_id_prc2, (char*)&msgForPRC, sizeof(asm_msg));
            DEBUG_CHECK_STATUS(status);
        }

        if (status != RTEMS_SUCCESSFUL)
        {
            LFR_PRINTF("in AVF2 *** Error sending message to PRC2, code %d\n", status);
        }
    }
}

rtems_task prc2_task(rtems_task_argument argument)
{
    IGNORE_UNUSED_PARAMETER(argument);
    char incomingData[MSG_QUEUE_SIZE_SEND]; // incoming data buffer
    size_t size; // size of the incoming TC packet
    asm_msg* incomingMsg;
    //
    rtems_status_code status;
    rtems_id queue_id_send;
    rtems_id queue_id_q_p2;
    bp_packet __attribute__((aligned(4))) packet_norm_bp1;
    bp_packet __attribute__((aligned(4))) packet_norm_bp2;
    ring_node* current_ring_node_to_send_asm_f2;
    float nbSMInASMNORM;

    size = 0;
    queue_id_send = RTEMS_ID_NONE;
    queue_id_q_p2 = RTEMS_ID_NONE;
    memset(&packet_norm_bp1, 0, sizeof(bp_packet));
    memset(&packet_norm_bp2, 0, sizeof(bp_packet));

    // init the ring of the averaged spectral matrices which will be transmitted to the DPU
    init_ring(
        ring_to_send_asm_f2, NB_RING_NODES_ASM_F2, (volatile int*)buffer_asm_f2, TOTAL_SIZE_SM);
    current_ring_node_to_send_asm_f2 = ring_to_send_asm_f2;
    DEBUG_CHECK_PTR(current_ring_node_to_send_asm_f2);

    //*************
    // NORM headers
    BP_init_header(&packet_norm_bp1, APID_TM_SCIENCE_NORMAL_BURST, SID_NORM_BP1_F2,
        PACKET_LENGTH_TM_LFR_SCIENCE_NORM_BP1_F2, NB_BINS_COMPRESSED_SM_F2);
    BP_init_header(&packet_norm_bp2, APID_TM_SCIENCE_NORMAL_BURST, SID_NORM_BP2_F2,
        PACKET_LENGTH_TM_LFR_SCIENCE_NORM_BP2_F2, NB_BINS_COMPRESSED_SM_F2);

    status = get_message_queue_id_send(&queue_id_send);
    DEBUG_CHECK_STATUS(status);
    if (status != RTEMS_SUCCESSFUL)
    {
        LFR_PRINTF("in PRC2 *** ERR get_message_queue_id_send %d\n", status);
    }
    status = get_message_queue_id_prc2(&queue_id_q_p2);
    DEBUG_CHECK_STATUS(status);
    if (status != RTEMS_SUCCESSFUL)
    {
        LFR_PRINTF("in PRC2 *** ERR get_message_queue_id_prc2 %d\n", status);
    }

    BOOT_PRINTF("in PRC2 ***\n");

    while (1)
    {
        status = rtems_message_queue_receive(queue_id_q_p2, incomingData,
            &size, //************************************
            RTEMS_WAIT, RTEMS_NO_TIMEOUT); // wait for a message coming from AVF2
        DEBUG_CHECK_STATUS(status);

        incomingMsg = (asm_msg*)incomingData;
        DEBUG_CHECK_PTR(incomingMsg);

        DEBUG_PRINTF("Before SM_calibrate_and_reorder_f2");
        SM_calibrate_and_reorder_f2(incomingMsg->norm->matrix, asm_f2_patched_norm);
        DEBUG_PRINTF("After SM_calibrate_and_reorder_f2");
        // ASM_patch( incomingMsg->norm->matrix, asm_f2_patched_norm );

        nbSMInASMNORM = incomingMsg->numberOfSMInASMNORM;

        //*****
        //*****
        // NORM
        //*****
        //*****
        // 1) compress the matrix for Basic Parameters calculation
        ASM_compress_divide_and_mask(asm_f2_patched_norm, compressed_sm_norm_f2, nbSMInASMNORM,
            NB_BINS_COMPRESSED_SM_F2, NB_BINS_TO_AVERAGE_ASM_F2, ASM_F2_INDICE_START, CHANNELF2);
        // BP1_F2
        if (incomingMsg->event & RTEMS_EVENT_NORM_BP1_F2)
        {
            // 1) compute the BP1 set
            // BP1_set( compressed_sm_norm_f2, k_coeff_intercalib_f2, NB_BINS_COMPRESSED_SM_F2,
            // packet_norm_bp1.data );
            compute_BP1(compressed_sm_norm_f2, NB_BINS_COMPRESSED_SM_F2, packet_norm_bp1.data);

            // 2) send the BP1 set
            set_time(packet_norm_bp1.time, (unsigned char*)&incomingMsg->coarseTimeNORM);
            set_time(packet_norm_bp1.acquisitionTime, (unsigned char*)&incomingMsg->coarseTimeNORM);
            packet_norm_bp1.pa_bia_status_info = pa_bia_status_info;
            packet_norm_bp1.sy_lfr_common_parameters
                = parameter_dump_packet.sy_lfr_common_parameters;
            BP_send((char*)&packet_norm_bp1, queue_id_send,
                PACKET_LENGTH_TM_LFR_SCIENCE_NORM_BP1_F2 + PACKET_LENGTH_DELTA);
        }
        // BP2_F2
        if (incomingMsg->event & RTEMS_EVENT_NORM_BP2_F2)
        {
            // 1) compute the BP2 set
            compute_BP2(compressed_sm_norm_f2, NB_BINS_COMPRESSED_SM_F2, packet_norm_bp2.data);
            // 2) send the BP2 set
            set_time(packet_norm_bp2.time, (unsigned char*)&incomingMsg->coarseTimeNORM);
            set_time(packet_norm_bp2.acquisitionTime, (unsigned char*)&incomingMsg->coarseTimeNORM);
            packet_norm_bp2.pa_bia_status_info = pa_bia_status_info;
            packet_norm_bp2.sy_lfr_common_parameters
                = parameter_dump_packet.sy_lfr_common_parameters;
            BP_send((char*)&packet_norm_bp2, queue_id_send,
                PACKET_LENGTH_TM_LFR_SCIENCE_NORM_BP2_F2 + PACKET_LENGTH_DELTA);
        }

        if (incomingMsg->event & RTEMS_EVENT_NORM_ASM_F2)
        {
            // 1) reorganize the ASM and divide
            ASM_divide(asm_f2_patched_norm,
                (float*)current_ring_node_to_send_asm_f2->buffer_address, nb_sm_before_f2.norm_bp1,
                ASM_F2_INDICE_START, ASM_F2_INDICE_START + ASM_F2_KEEP_BINS);
            current_ring_node_to_send_asm_f2->coarseTime = incomingMsg->coarseTimeNORM;
            current_ring_node_to_send_asm_f2->fineTime = incomingMsg->fineTimeNORM;
            current_ring_node_to_send_asm_f2->sid = SID_NORM_ASM_F2;

            // 3) send the spectral matrix packets
            status = rtems_message_queue_send(
                queue_id_send, &current_ring_node_to_send_asm_f2, sizeof(ring_node*));
            DEBUG_CHECK_STATUS(status);

            // change asm ring node
            current_ring_node_to_send_asm_f2 = current_ring_node_to_send_asm_f2->next;
            DEBUG_CHECK_PTR(current_ring_node_to_send_asm_f2);
        }

        update_queue_max_count(queue_id_q_p2, &hk_lfr_q_p2_fifo_size_max);
    }
}

//**********
// FUNCTIONS

void reset_nb_sm_f2(void)
{
    nb_sm_before_f2.norm_bp1 = parameter_dump_packet.sy_lfr_n_bp_p0;
    nb_sm_before_f2.norm_bp2 = parameter_dump_packet.sy_lfr_n_bp_p1;
    nb_sm_before_f2.norm_asm = (parameter_dump_packet.sy_lfr_n_asm_p[0] * CONST_256)
        + parameter_dump_packet.sy_lfr_n_asm_p[1];
}


