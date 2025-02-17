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

/** Functions to send TM packets related to TC parsing and execution.
 *
 * @file
 * @author P. LEROY
 *
 * A group of functions to send appropriate TM packets after parsing and execution:
 * - TM_LFR_TC_EXE_SUCCESS
 * - TM_LFR_TC_EXE_INCONSISTENT
 * - TM_LFR_TC_EXE_NOT_EXECUTABLE
 * - TM_LFR_TC_EXE_NOT_IMPLEMENTED
 * - TM_LFR_TC_EXE_ERROR
 * - TM_LFR_TC_EXE_CORRUPTED
 *
 */

#include "tc_tm/tm_lfr_tc_exe.h"
#include "fsw_debug.h"
#include "hw/lfr_regs.h"

int send_tm_lfr_tc_exe_success(ccsdsTelecommandPacket_t* TC, rtems_id queue_id)
{
    /** This function sends a TM_LFR_TC_EXE_SUCCESS packet in the dedicated RTEMS message queue.
     *
     * @param TC points to the TeleCommand packet that is being processed
     * @param queue_id is the id of the queue which handles TM
     *
     * @return RTEMS directive status code:
     * - RTEMS_SUCCESSFUL - message sent successfully
     * - RTEMS_INVALID_ID - invalid queue id
     * - RTEMS_INVALID_SIZE - invalid message size
     * - RTEMS_INVALID_ADDRESS - buffer is NULL
     * - RTEMS_UNSATISFIED - out of message buffers
     * - RTEMS_TOO_MANY - queue s limit has been reached
     *
     */

    rtems_status_code status;
    Packet_TM_LFR_TC_EXE_SUCCESS_t TM;

    TM.targetLogicalAddress = CCSDS_DESTINATION_ID;
    TM.protocolIdentifier = CCSDS_PROTOCOLE_ID;
    TM.reserved = DEFAULT_RESERVED;
    TM.userApplication = CCSDS_USER_APP;
    // PACKET HEADER
    TM.packetID[0] = (unsigned char)(APID_TM_TC_EXE >> SHIFT_1_BYTE);
    TM.packetID[1] = (unsigned char)(APID_TM_TC_EXE);
    increment_seq_counter_destination_id(TM.packetSequenceControl, TC->sourceID);
    TM.packetLength[0] = (unsigned char)(PACKET_LENGTH_TC_EXE_SUCCESS >> SHIFT_1_BYTE);
    TM.packetLength[1] = (unsigned char)(PACKET_LENGTH_TC_EXE_SUCCESS);
    // DATA FIELD HEADER
    TM.spare1_pusVersion_spare2 = DEFAULT_SPARE1_PUSVERSION_SPARE2;
    TM.serviceType = TM_TYPE_TC_EXE;
    TM.serviceSubType = TM_SUBTYPE_EXE_OK;
    TM.destinationID = TC->sourceID;
    TM.time[BYTE_0] = (unsigned char)(time_management_regs->coarse_time >> SHIFT_3_BYTES);
    TM.time[BYTE_1] = (unsigned char)(time_management_regs->coarse_time >> SHIFT_2_BYTES);
    TM.time[BYTE_2] = (unsigned char)(time_management_regs->coarse_time >> SHIFT_1_BYTE);
    TM.time[BYTE_3] = (unsigned char)(time_management_regs->coarse_time);
    TM.time[BYTE_4] = (unsigned char)(time_management_regs->fine_time >> SHIFT_1_BYTE);
    TM.time[BYTE_5] = (unsigned char)(time_management_regs->fine_time);
    //
    TM.telecommand_pkt_id[0] = TC->packetID[0];
    TM.telecommand_pkt_id[1] = TC->packetID[1];
    TM.pkt_seq_control[0] = TC->packetSequenceControl[0];
    TM.pkt_seq_control[1] = TC->packetSequenceControl[1];

    // SEND DATA
    status = rtems_message_queue_send(queue_id, &TM, sizeof (TM));
    DEBUG_CHECK_STATUS(status);

    // UPDATE HK FIELDS
    update_last_TC_exe(TC, TM.time);

    return status;
}

int send_tm_lfr_tc_exe_inconsistent(ccsdsTelecommandPacket_t* TC, rtems_id queue_id,
    unsigned char byte_position, unsigned char rcv_value)
{
    /** This function sends a TM_LFR_TC_EXE_INCONSISTENT packet in the dedicated RTEMS message
     * queue.
     *
     * @param TC points to the TeleCommand packet that is being processed
     * @param queue_id is the id of the queue which handles TM
     * @param byte_position is the byte position of the MSB of the parameter that has been seen as
     * inconsistent
     * @param rcv_value  is the value of the LSB of the parameter that has been detected as
     * inconsistent
     *
     * @return RTEMS directive status code:
     * - RTEMS_SUCCESSFUL - message sent successfully
     * - RTEMS_INVALID_ID - invalid queue id
     * - RTEMS_INVALID_SIZE - invalid message size
     * - RTEMS_INVALID_ADDRESS - buffer is NULL
     * - RTEMS_UNSATISFIED - out of message buffers
     * - RTEMS_TOO_MANY - queue s limit has been reached
     *
     */

    rtems_status_code status;
    Packet_TM_LFR_TC_EXE_INCONSISTENT_t TM;

    TM.targetLogicalAddress = CCSDS_DESTINATION_ID;
    TM.protocolIdentifier = CCSDS_PROTOCOLE_ID;
    TM.reserved = DEFAULT_RESERVED;
    TM.userApplication = CCSDS_USER_APP;
    // PACKET HEADER
    TM.packetID[0] = (unsigned char)(APID_TM_TC_EXE >> SHIFT_1_BYTE);
    TM.packetID[1] = (unsigned char)(APID_TM_TC_EXE);
    increment_seq_counter_destination_id(TM.packetSequenceControl, TC->sourceID);
    TM.packetLength[0] = (unsigned char)(PACKET_LENGTH_TC_EXE_INCONSISTENT >> SHIFT_1_BYTE);
    TM.packetLength[1] = (unsigned char)(PACKET_LENGTH_TC_EXE_INCONSISTENT);
    // DATA FIELD HEADER
    TM.spare1_pusVersion_spare2 = DEFAULT_SPARE1_PUSVERSION_SPARE2;
    TM.serviceType = TM_TYPE_TC_EXE;
    TM.serviceSubType = TM_SUBTYPE_EXE_NOK;
    TM.destinationID = TC->sourceID;
    TM.time[BYTE_0] = (unsigned char)(time_management_regs->coarse_time >> SHIFT_3_BYTES);
    TM.time[BYTE_1] = (unsigned char)(time_management_regs->coarse_time >> SHIFT_2_BYTES);
    TM.time[BYTE_2] = (unsigned char)(time_management_regs->coarse_time >> SHIFT_1_BYTE);
    TM.time[BYTE_3] = (unsigned char)(time_management_regs->coarse_time);
    TM.time[BYTE_4] = (unsigned char)(time_management_regs->fine_time >> SHIFT_1_BYTE);
    TM.time[BYTE_5] = (unsigned char)(time_management_regs->fine_time);
    //
    TM.tc_failure_code[0] = (char)(WRONG_APP_DATA >> SHIFT_1_BYTE);
    TM.tc_failure_code[1] = (char)(WRONG_APP_DATA);
    TM.telecommand_pkt_id[0] = TC->packetID[0];
    TM.telecommand_pkt_id[1] = TC->packetID[1];
    TM.pkt_seq_control[0] = TC->packetSequenceControl[0];
    TM.pkt_seq_control[1] = TC->packetSequenceControl[1];
    TM.tc_service = TC->serviceType; // type of the rejected TC
    TM.tc_subtype = TC->serviceSubType; // subtype of the rejected TC
    TM.byte_position = byte_position;
    TM.rcv_value = (unsigned char)rcv_value;

    // SEND DATA
    status = rtems_message_queue_send(queue_id, &TM, sizeof (TM));
    DEBUG_CHECK_STATUS(status);

    // UPDATE HK FIELDS
    update_last_TC_rej(TC, TM.time);

    return status;
}

int send_tm_lfr_tc_exe_not_executable(ccsdsTelecommandPacket_t* TC, rtems_id queue_id)
{
    /** This function sends a TM_LFR_TC_EXE_NOT_EXECUTABLE packet in the dedicated RTEMS message
     * queue.
     *
     * @param TC points to the TeleCommand packet that is being processed
     * @param queue_id is the id of the queue which handles TM
     *
     * @return RTEMS directive status code:
     * - RTEMS_SUCCESSFUL - message sent successfully
     * - RTEMS_INVALID_ID - invalid queue id
     * - RTEMS_INVALID_SIZE - invalid message size
     * - RTEMS_INVALID_ADDRESS - buffer is NULL
     * - RTEMS_UNSATISFIED - out of message buffers
     * - RTEMS_TOO_MANY - queue s limit has been reached
     *
     */

    rtems_status_code status;
    Packet_TM_LFR_TC_EXE_NOT_EXECUTABLE_t TM;

    TM.targetLogicalAddress = CCSDS_DESTINATION_ID;
    TM.protocolIdentifier = CCSDS_PROTOCOLE_ID;
    TM.reserved = DEFAULT_RESERVED;
    TM.userApplication = CCSDS_USER_APP;
    // PACKET HEADER
    TM.packetID[0] = (unsigned char)(APID_TM_TC_EXE >> SHIFT_1_BYTE);
    TM.packetID[1] = (unsigned char)(APID_TM_TC_EXE);
    increment_seq_counter_destination_id(TM.packetSequenceControl, TC->sourceID);
    TM.packetLength[0] = (unsigned char)(PACKET_LENGTH_TC_EXE_NOT_EXECUTABLE >> SHIFT_1_BYTE);
    TM.packetLength[1] = (unsigned char)(PACKET_LENGTH_TC_EXE_NOT_EXECUTABLE);
    // DATA FIELD HEADER
    TM.spare1_pusVersion_spare2 = DEFAULT_SPARE1_PUSVERSION_SPARE2;
    TM.serviceType = TM_TYPE_TC_EXE;
    TM.serviceSubType = TM_SUBTYPE_EXE_NOK;
    TM.destinationID = TC->sourceID; // default destination id
    TM.time[BYTE_0] = (unsigned char)(time_management_regs->coarse_time >> SHIFT_3_BYTES);
    TM.time[BYTE_1] = (unsigned char)(time_management_regs->coarse_time >> SHIFT_2_BYTES);
    TM.time[BYTE_2] = (unsigned char)(time_management_regs->coarse_time >> SHIFT_1_BYTE);
    TM.time[BYTE_3] = (unsigned char)(time_management_regs->coarse_time);
    TM.time[BYTE_4] = (unsigned char)(time_management_regs->fine_time >> SHIFT_1_BYTE);
    TM.time[BYTE_5] = (unsigned char)(time_management_regs->fine_time);
    //
    TM.tc_failure_code[0] = (char)(TC_NOT_EXE >> SHIFT_1_BYTE);
    TM.tc_failure_code[1] = (char)(TC_NOT_EXE);
    TM.telecommand_pkt_id[0] = TC->packetID[0];
    TM.telecommand_pkt_id[1] = TC->packetID[1];
    TM.pkt_seq_control[0] = TC->packetSequenceControl[0];
    TM.pkt_seq_control[1] = TC->packetSequenceControl[1];
    TM.tc_service = TC->serviceType; // type of the rejected TC
    TM.tc_subtype = TC->serviceSubType; // subtype of the rejected TC
    TM.lfr_status_word[0] = housekeeping_packet.lfr_status_word[0];
    TM.lfr_status_word[1] = housekeeping_packet.lfr_status_word[1];

    // SEND DATA
    status = rtems_message_queue_send(queue_id, &TM, sizeof (TM));
    DEBUG_CHECK_STATUS(status);

    // UPDATE HK FIELDS
    update_last_TC_rej(TC, TM.time);

    return status;
}

int send_tm_lfr_tc_exe_error(ccsdsTelecommandPacket_t* TC, rtems_id queue_id)
{
    /** This function sends a TM_LFR_TC_EXE_ERROR packet in the dedicated RTEMS message queue.
     *
     * @param TC points to the TeleCommand packet that is being processed
     * @param queue_id is the id of the queue which handles TM
     *
     * @return RTEMS directive status code:
     * - RTEMS_SUCCESSFUL - message sent successfully
     * - RTEMS_INVALID_ID - invalid queue id
     * - RTEMS_INVALID_SIZE - invalid message size
     * - RTEMS_INVALID_ADDRESS - buffer is NULL
     * - RTEMS_UNSATISFIED - out of message buffers
     * - RTEMS_TOO_MANY - queue s limit has been reached
     *
     */

    rtems_status_code status;
    Packet_TM_LFR_TC_EXE_ERROR_t TM;

    TM.targetLogicalAddress = CCSDS_DESTINATION_ID;
    TM.protocolIdentifier = CCSDS_PROTOCOLE_ID;
    TM.reserved = DEFAULT_RESERVED;
    TM.userApplication = CCSDS_USER_APP;
    // PACKET HEADER
    TM.packetID[0] = (unsigned char)(APID_TM_TC_EXE >> SHIFT_1_BYTE);
    TM.packetID[1] = (unsigned char)(APID_TM_TC_EXE);
    increment_seq_counter_destination_id(TM.packetSequenceControl, TC->sourceID);
    TM.packetLength[0] = (unsigned char)(PACKET_LENGTH_TC_EXE_ERROR >> SHIFT_1_BYTE);
    TM.packetLength[1] = (unsigned char)(PACKET_LENGTH_TC_EXE_ERROR);
    // DATA FIELD HEADER
    TM.spare1_pusVersion_spare2 = DEFAULT_SPARE1_PUSVERSION_SPARE2;
    TM.serviceType = TM_TYPE_TC_EXE;
    TM.serviceSubType = TM_SUBTYPE_EXE_NOK;
    TM.destinationID = TC->sourceID; // default destination id
    TM.time[BYTE_0] = (unsigned char)(time_management_regs->coarse_time >> SHIFT_3_BYTES);
    TM.time[BYTE_1] = (unsigned char)(time_management_regs->coarse_time >> SHIFT_2_BYTES);
    TM.time[BYTE_2] = (unsigned char)(time_management_regs->coarse_time >> SHIFT_1_BYTE);
    TM.time[BYTE_3] = (unsigned char)(time_management_regs->coarse_time);
    TM.time[BYTE_4] = (unsigned char)(time_management_regs->fine_time >> SHIFT_1_BYTE);
    TM.time[BYTE_5] = (unsigned char)(time_management_regs->fine_time);
    //
    TM.tc_failure_code[0] = (char)(FAIL_DETECTED >> SHIFT_1_BYTE);
    TM.tc_failure_code[1] = (char)(FAIL_DETECTED);
    TM.telecommand_pkt_id[0] = TC->packetID[0];
    TM.telecommand_pkt_id[1] = TC->packetID[1];
    TM.pkt_seq_control[0] = TC->packetSequenceControl[0];
    TM.pkt_seq_control[1] = TC->packetSequenceControl[1];
    TM.tc_service = TC->serviceType; // type of the rejected TC
    TM.tc_subtype = TC->serviceSubType; // subtype of the rejected TC

    // SEND DATA
    status = rtems_message_queue_send(queue_id, &TM, sizeof (TM));
    DEBUG_CHECK_STATUS(status);

    // UPDATE HK FIELDS
    update_last_TC_rej(TC, TM.time);

    return status;
}

int send_tm_lfr_tc_exe_corrupted(ccsdsTelecommandPacket_t* TC, rtems_id queue_id,
    unsigned char* computed_CRC, unsigned char* currentTC_LEN_RCV, unsigned char destinationID)
{
    /** This function sends a TM_LFR_TC_EXE_CORRUPTED packet in the dedicated RTEMS message queue.
     *
     * @param TC points to the TeleCommand packet that is being processed
     * @param queue_id is the id of the queue which handles TM
     * @param computed_CRC points to a buffer of two bytes containing the CRC computed during the
     * parsing of the TeleCommand
     * @param currentTC_LEN_RCV points to a buffer of two bytes containing a packet size field
     * computed on the received data
     *
     * @return RTEMS directive status code:
     * - RTEMS_SUCCESSFUL - message sent successfully
     * - RTEMS_INVALID_ID - invalid queue id
     * - RTEMS_INVALID_SIZE - invalid message size
     * - RTEMS_INVALID_ADDRESS - buffer is NULL
     * - RTEMS_UNSATISFIED - out of message buffers
     * - RTEMS_TOO_MANY - queue s limit has been reached
     *
     */

    rtems_status_code status;
    Packet_TM_LFR_TC_EXE_CORRUPTED_t TM;
    unsigned int packetLength;
    unsigned int estimatedPacketLength;
    unsigned char* packetDataField;

    packetLength = (TC->packetLength[0] * CONST_256)
        + TC->packetLength[1]; // compute the packet length parameter written in the TC
    estimatedPacketLength
        = (unsigned int)((currentTC_LEN_RCV[0] * CONST_256) + currentTC_LEN_RCV[1]);
    packetDataField
        = (unsigned char*)&TC->headerFlag_pusVersion_Ack; // get the beginning of the data field

    TM.targetLogicalAddress = CCSDS_DESTINATION_ID;
    TM.protocolIdentifier = CCSDS_PROTOCOLE_ID;
    TM.reserved = DEFAULT_RESERVED;
    TM.userApplication = CCSDS_USER_APP;
    // PACKET HEADER
    TM.packetID[0] = (unsigned char)(APID_TM_TC_EXE >> SHIFT_1_BYTE);
    TM.packetID[1] = (unsigned char)(APID_TM_TC_EXE);
    increment_seq_counter_destination_id(TM.packetSequenceControl, TC->sourceID);
    TM.packetLength[0] = (unsigned char)(PACKET_LENGTH_TC_EXE_CORRUPTED >> SHIFT_1_BYTE);
    TM.packetLength[1] = (unsigned char)(PACKET_LENGTH_TC_EXE_CORRUPTED);
    // DATA FIELD HEADER
    TM.spare1_pusVersion_spare2 = DEFAULT_SPARE1_PUSVERSION_SPARE2;
    TM.serviceType = TM_TYPE_TC_EXE;
    TM.serviceSubType = TM_SUBTYPE_EXE_NOK;
    TM.destinationID = destinationID;
    TM.time[BYTE_0] = (unsigned char)(time_management_regs->coarse_time >> SHIFT_3_BYTES);
    TM.time[BYTE_1] = (unsigned char)(time_management_regs->coarse_time >> SHIFT_2_BYTES);
    TM.time[BYTE_2] = (unsigned char)(time_management_regs->coarse_time >> SHIFT_1_BYTE);
    TM.time[BYTE_3] = (unsigned char)(time_management_regs->coarse_time);
    TM.time[BYTE_4] = (unsigned char)(time_management_regs->fine_time >> SHIFT_1_BYTE);
    TM.time[BYTE_5] = (unsigned char)(time_management_regs->fine_time);
    //
    TM.tc_failure_code[0] = (unsigned char)(CORRUPTED >> SHIFT_1_BYTE);
    TM.tc_failure_code[1] = (unsigned char)(CORRUPTED);
    TM.telecommand_pkt_id[0] = TC->packetID[0];
    TM.telecommand_pkt_id[1] = TC->packetID[1];
    TM.pkt_seq_control[0] = TC->packetSequenceControl[0];
    TM.pkt_seq_control[1] = TC->packetSequenceControl[1];
    TM.tc_service = TC->serviceType; // type of the rejected TC
    TM.tc_subtype = TC->serviceSubType; // subtype of the rejected TC
    TM.pkt_len_rcv_value[0] = TC->packetLength[0];
    TM.pkt_len_rcv_value[1] = TC->packetLength[1];
    TM.pkt_datafieldsize_cnt[0] = currentTC_LEN_RCV[0];
    TM.pkt_datafieldsize_cnt[1] = currentTC_LEN_RCV[1];
    TM.rcv_crc[0] = packetDataField[estimatedPacketLength - 1];
    TM.rcv_crc[1] = packetDataField[estimatedPacketLength];
    TM.computed_crc[0] = computed_CRC[0];
    TM.computed_crc[1] = computed_CRC[1];

    // SEND DATA
    status = rtems_message_queue_send(queue_id, &TM, sizeof (TM));
    DEBUG_CHECK_STATUS(status);

    // UPDATE HK FIELDS
    update_last_TC_rej(TC, TM.time);

    return status;
}

void increment_seq_counter_destination_id(
    unsigned char* packet_sequence_control, unsigned char destination_id)
{
    /** This function increment the packet sequence control parameter of a TC, depending on its
     * destination ID.
     *
     * @param packet_sequence_control points to the packet sequence control which will be
     * incremented
     * @param destination_id is the destination ID of the TM, there is one counter by destination ID
     *
     * If the destination ID is not known, a dedicated counter is incremented.
     *
     */

    unsigned short sequence_cnt;
    unsigned short segmentation_grouping_flag;
    unsigned short new_packet_sequence_control;
    unsigned char i;

    switch (destination_id)
    {
        case SID_TC_GROUND:
            i = GROUND;
            break;
        case SID_TC_MISSION_TIMELINE:
            i = MISSION_TIMELINE;
            break;
        case SID_TC_TC_SEQUENCES:
            i = TC_SEQUENCES;
            break;
        case SID_TC_RECOVERY_ACTION_CMD:
            i = RECOVERY_ACTION_CMD;
            break;
        case SID_TC_BACKUP_MISSION_TIMELINE:
            i = BACKUP_MISSION_TIMELINE;
            break;
        case SID_TC_DIRECT_CMD:
            i = DIRECT_CMD;
            break;
        case SID_TC_SPARE_GRD_SRC1:
            i = SPARE_GRD_SRC1;
            break;
        case SID_TC_SPARE_GRD_SRC2:
            i = SPARE_GRD_SRC2;
            break;
        case SID_TC_OBCP:
            i = OBCP;
            break;
        case SID_TC_SYSTEM_CONTROL:
            i = SYSTEM_CONTROL;
            break;
        case SID_TC_AOCS:
            i = AOCS;
            break;
        case SID_TC_RPW_INTERNAL:
            i = RPW_INTERNAL;
            break;
        default:
            i = GROUND;
            break;
    }

    segmentation_grouping_flag = TM_PACKET_SEQ_CTRL_STANDALONE << SHIFT_1_BYTE;
    sequence_cnt = sequenceCounters_TC_EXE[i] & SEQ_CNT_MASK;

    new_packet_sequence_control = segmentation_grouping_flag | sequence_cnt;

    packet_sequence_control[0] = (unsigned char)(new_packet_sequence_control >> SHIFT_1_BYTE);
    packet_sequence_control[1] = (unsigned char)(new_packet_sequence_control);

    // increment the sequence counter
    if (sequenceCounters_TC_EXE[i] < SEQ_CNT_MAX)
    {
        sequenceCounters_TC_EXE[i] = sequenceCounters_TC_EXE[i] + 1;
    }
    else
    {
        sequenceCounters_TC_EXE[i] = 0;
    }
}
