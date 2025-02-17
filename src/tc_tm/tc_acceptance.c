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
/** Functions related to TeleCommand acceptance.
 *
 * @file
 * @author P. LEROY
 *
 * A group of functions to handle TeleCommands parsing.\n
 *
 */

#include "tc_tm/tc_acceptance.h"
#include <stdio.h>

unsigned int lookUpTableForCRC[CONST_256] = { 0 };

//**********************
// GENERAL USE FUNCTIONS
unsigned int Crc_opt(unsigned char D, unsigned int Chk)
{
    /** This function generate the CRC for one byte and returns the value of the new syndrome.
     *
     * @param D is the current byte of data.
     * @param Chk is the current syndrom value.
     *
     * @return the value of the new syndrome on two bytes.
     *
     */

    return (((Chk << SHIFT_1_BYTE) & BYTE0_MASK)
        ^ lookUpTableForCRC[(((Chk >> SHIFT_1_BYTE) ^ D) & BYTE1_MASK)]);
}

void initLookUpTableForCRC(void)
{
    /** This function is used to initiates the look-up table for fast CRC computation.
     *
     * The global table lookUpTableForCRC[256] is initiated.
     *
     */

    unsigned int i;
    unsigned int tmp;

    for (i = 0; i < CONST_256; i++)
    {
        tmp = 0;
        if ((i & BIT_0) != 0)
        {
            tmp = tmp ^ CONST_CRC_0;
        }
        if ((i & BIT_1) != 0)
        {
            tmp = tmp ^ CONST_CRC_1;
        }
        if ((i & BIT_2) != 0)
        {
            tmp = tmp ^ CONST_CRC_2;
        }
        if ((i & BIT_3) != 0)
        {
            tmp = tmp ^ CONST_CRC_3;
        }
        if ((i & BIT_4) != 0)
        {
            tmp = tmp ^ CONST_CRC_4;
        }
        if ((i & BIT_5) != 0)
        {
            tmp = tmp ^ CONST_CRC_5;
        }
        if ((i & BIT_6) != 0)
        {
            tmp = tmp ^ CONST_CRC_6;
        }
        if ((i & BIT_7) != 0)
        {
            tmp = tmp ^ CONST_CRC_7;
        }
        lookUpTableForCRC[i] = tmp;
    }
}

void GetCRCAsTwoBytes(unsigned char* data, unsigned char* crcAsTwoBytes, unsigned int sizeOfData)
{
    /** This function calculates a two bytes Cyclic Redundancy Code.
     *
     * @param data points to a buffer containing the data on which to compute the CRC.
     * @param crcAsTwoBytes points points to a two bytes buffer in which the CRC is stored.
     * @param sizeOfData is the number of bytes of *data* used to compute the CRC.
     *
     * The specification of the Cyclic Redundancy Code is described in the following document:
     * ECSS-E-70-41-A.
     *
     */

    unsigned int Chk = CRC_RESET;
    for (unsigned int j = 0; j < sizeOfData; j++)
    {
        Chk = Crc_opt(data[j], Chk);
    }
    crcAsTwoBytes[0] = (unsigned char)(Chk >> SHIFT_1_BYTE);
    crcAsTwoBytes[1] = (unsigned char)(Chk & BYTE1_MASK);
}

//*********************
// ACCEPTANCE FUNCTIONS
int tc_parser(ccsdsTelecommandPacket_t* TCPacket, unsigned int estimatedPacketLength,
    unsigned char* computed_CRC)
{
    /** This function parses TeleCommands.
     *
     * @param TC points to the TeleCommand that will be parsed.
     * @param estimatedPacketLength is the PACKET_LENGTH field calculated from the effective length
     * of the received packet.
     *
     * @return Status code of the parsing.
     *
     * The parsing checks:
     * - process id
     * - category
     * - length: a global check is performed and a per subtype check also
     * - type
     * - subtype
     * - crc
     *
     */

    int status;
    int status_crc;
    unsigned char pid;
    unsigned char category;
    unsigned int packetLength;
    unsigned char packetType;
    unsigned char packetSubtype;
    unsigned char sid;

    status = CCSDS_TM_VALID;

    // APID check *** APID on 2 bytes
    pid = ((TCPacket->packetID[0] & BITS_PID_0) << SHIFT_4_BITS)
        + ((TCPacket->packetID[1] >> SHIFT_4_BITS)
            & BITS_PID_1); // PID = 11 *** 7 bits xxxxx210 7654xxxx
    category
        = (TCPacket->packetID[1] & BITS_CAT); // PACKET_CATEGORY = 12 *** 4 bits xxxxxxxx xxxx3210
    packetLength = (TCPacket->packetLength[0] * CONST_256) + TCPacket->packetLength[1];
    packetType = TCPacket->serviceType;
    packetSubtype = TCPacket->serviceSubType;
    sid = TCPacket->sourceID;

    if (pid != CCSDS_PROCESS_ID) // CHECK THE PROCESS ID
    {
        status = ILLEGAL_APID;
    }
    if (status == CCSDS_TM_VALID) // CHECK THE CATEGORY
    {
        if (category != CCSDS_PACKET_CATEGORY)
        {
            status = ILLEGAL_APID;
        }
    }
    if (status == CCSDS_TM_VALID) // CHECK THE PACKET_LENGTH FIELD AND THE ESTIMATED PACKET_LENGTH
                                  // COMPLIANCE
    {
        if (packetLength != estimatedPacketLength)
        {
            status = WRONG_LEN_PKT;
        }
    }
    if (status == CCSDS_TM_VALID) // CHECK THAT THE PACKET DOES NOT EXCEED THE MAX SIZE
    {
        if (packetLength > CCSDS_TC_PKT_MAX_SIZE)
        {
            status = WRONG_LEN_PKT;
        }
    }
    if (status == CCSDS_TM_VALID) // CHECK THE TYPE
    {
        status = tc_check_type(packetType);
    }
    if (status == CCSDS_TM_VALID) // CHECK THE SUBTYPE
    {
        status = tc_check_type_subtype(packetType, packetSubtype);
    }
    if (status == CCSDS_TM_VALID) // CHECK THE SID
    {
        status = tc_check_sid(sid);
    }
    if (status == CCSDS_TM_VALID) // CHECK THE SUBTYPE AND LENGTH COMPLIANCE
    {
        status = tc_check_length(packetSubtype, packetLength);
    }
    status_crc = tc_check_crc(TCPacket, estimatedPacketLength, computed_CRC);
    if (status == CCSDS_TM_VALID) // CHECK CRC
    {
        status = status_crc;
    }

    return status;
}

int tc_check_type(unsigned char packetType)
{
    /** This function checks that the type of a TeleCommand is valid.
     *
     * @param packetType is the type to check.
     *
     * @return Status code CCSDS_TM_VALID or ILL_TYPE.
     *
     */

    int status;

    status = ILL_TYPE;

    if ((packetType == TC_TYPE_GEN) || (packetType == TC_TYPE_TIME))
    {
        status = CCSDS_TM_VALID;
    }

    return status;
}

int tc_check_type_subtype(unsigned char packetType, unsigned char packetSubType)
{
    /** This function checks that the subtype of a TeleCommand is valid and coherent with the type.
     *
     * @param packetType is the type of the TC.
     * @param packetSubType is the subtype to check.
     *
     * @return Status code CCSDS_TM_VALID or ILL_SUBTYPE.
     *
     */

    int status;

    switch (packetType)
    {
        case TC_TYPE_GEN:
            if ((packetSubType == TC_SUBTYPE_RESET) || (packetSubType == TC_SUBTYPE_LOAD_COMM)
                || (packetSubType == TC_SUBTYPE_LOAD_NORM)
                || (packetSubType == TC_SUBTYPE_LOAD_BURST)
                || (packetSubType == TC_SUBTYPE_LOAD_SBM1)
                || (packetSubType == TC_SUBTYPE_LOAD_SBM2) || (packetSubType == TC_SUBTYPE_DUMP)
                || (packetSubType == TC_SUBTYPE_ENTER) || (packetSubType == TC_SUBTYPE_UPDT_INFO)
                || (packetSubType == TC_SUBTYPE_EN_CAL) || (packetSubType == TC_SUBTYPE_DIS_CAL)
                || (packetSubType == TC_SUBTYPE_LOAD_K) || (packetSubType == TC_SUBTYPE_DUMP_K)
                || (packetSubType == TC_SUBTYPE_LOAD_FBINS)
                || (packetSubType == TC_SUBTYPE_LOAD_FILTER_PAR))
            {
                status = CCSDS_TM_VALID;
            }
            else
            {
                status = ILL_SUBTYPE;
            }
            break;

        case TC_TYPE_TIME:
            if (packetSubType == TC_SUBTYPE_UPDT_TIME)
            {
                status = CCSDS_TM_VALID;
            }
            else
            {
                status = ILL_SUBTYPE;
            }
            break;

        default:
            status = ILL_SUBTYPE;
            break;
    }

    return status;
}

int tc_check_sid(unsigned char sid)
{
    /** This function checks that the sid of a TeleCommand is valid.
     *
     * @param sid is the sid to check.
     *
     * @return Status code CCSDS_TM_VALID or CORRUPTED.
     *
     */

    int status;

    status = WRONG_SRC_ID;

    if ((sid == SID_TC_MISSION_TIMELINE) || (sid == SID_TC_TC_SEQUENCES)
        || (sid == SID_TC_RECOVERY_ACTION_CMD) || (sid == SID_TC_BACKUP_MISSION_TIMELINE)
        || (sid == SID_TC_DIRECT_CMD) || (sid == SID_TC_SPARE_GRD_SRC1)
        || (sid == SID_TC_SPARE_GRD_SRC2) || (sid == SID_TC_OBCP) || (sid == SID_TC_SYSTEM_CONTROL)
        || (sid == SID_TC_AOCS) || (sid == SID_TC_RPW_INTERNAL))
    {
        status = CCSDS_TM_VALID;
    }
    else
    {
        status = WRONG_SRC_ID;
    }

    return status;
}

int tc_check_length(unsigned char packetSubType, unsigned int length)
{
    /** This function checks that the subtype and the length are compliant.
     *
     * @param packetSubType is the subtype to check.
     * @param length is the length to check.
     *
     * @return Status code CCSDS_TM_VALID or ILL_TYPE.
     *
     */

    int status;

    status = LFR_SUCCESSFUL;

    switch (packetSubType)
    {
        case TC_SUBTYPE_RESET:
            if (length != (TC_LEN_RESET - CCSDS_TC_TM_PACKET_OFFSET))
            {
                status = WRONG_LEN_PKT;
            }
            else
            {
                status = CCSDS_TM_VALID;
            }
            break;
        case TC_SUBTYPE_LOAD_COMM:
            if (length != (TC_LEN_LOAD_COMM - CCSDS_TC_TM_PACKET_OFFSET))
            {
                status = WRONG_LEN_PKT;
            }
            else
            {
                status = CCSDS_TM_VALID;
            }
            break;
        case TC_SUBTYPE_LOAD_NORM:
            if (length != (TC_LEN_LOAD_NORM - CCSDS_TC_TM_PACKET_OFFSET))
            {
                status = WRONG_LEN_PKT;
            }
            else
            {
                status = CCSDS_TM_VALID;
            }
            break;
        case TC_SUBTYPE_LOAD_BURST:
            if (length != (TC_LEN_LOAD_BURST - CCSDS_TC_TM_PACKET_OFFSET))
            {
                status = WRONG_LEN_PKT;
            }
            else
            {
                status = CCSDS_TM_VALID;
            }
            break;
        case TC_SUBTYPE_LOAD_SBM1:
            if (length != (TC_LEN_LOAD_SBM1 - CCSDS_TC_TM_PACKET_OFFSET))
            {
                status = WRONG_LEN_PKT;
            }
            else
            {
                status = CCSDS_TM_VALID;
            }
            break;
        case TC_SUBTYPE_LOAD_SBM2:
            if (length != (TC_LEN_LOAD_SBM2 - CCSDS_TC_TM_PACKET_OFFSET))
            {
                status = WRONG_LEN_PKT;
            }
            else
            {
                status = CCSDS_TM_VALID;
            }
            break;
        case TC_SUBTYPE_DUMP:
            if (length != (TC_LEN_DUMP - CCSDS_TC_TM_PACKET_OFFSET))
            {
                status = WRONG_LEN_PKT;
            }
            else
            {
                status = CCSDS_TM_VALID;
            }
            break;
        case TC_SUBTYPE_ENTER:
            if (length != (TC_LEN_ENTER - CCSDS_TC_TM_PACKET_OFFSET))
            {
                status = WRONG_LEN_PKT;
            }
            else
            {
                status = CCSDS_TM_VALID;
            }
            break;
        case TC_SUBTYPE_UPDT_INFO:
            if (length != (TC_LEN_UPDT_INFO - CCSDS_TC_TM_PACKET_OFFSET))
            {
                status = WRONG_LEN_PKT;
            }
            else
            {
                status = CCSDS_TM_VALID;
            }
            break;
        case TC_SUBTYPE_EN_CAL:
            if (length != (TC_LEN_EN_CAL - CCSDS_TC_TM_PACKET_OFFSET))
            {
                status = WRONG_LEN_PKT;
            }
            else
            {
                status = CCSDS_TM_VALID;
            }
            break;
        case TC_SUBTYPE_DIS_CAL:
            if (length != (TC_LEN_DIS_CAL - CCSDS_TC_TM_PACKET_OFFSET))
            {
                status = WRONG_LEN_PKT;
            }
            else
            {
                status = CCSDS_TM_VALID;
            }
            break;
        case TC_SUBTYPE_LOAD_K:
            if (length != (TC_LEN_LOAD_K - CCSDS_TC_TM_PACKET_OFFSET))
            {
                status = WRONG_LEN_PKT;
            }
            else
            {
                status = CCSDS_TM_VALID;
            }
            break;
        case TC_SUBTYPE_DUMP_K:
            if (length != (TC_LEN_DUMP_K - CCSDS_TC_TM_PACKET_OFFSET))
            {
                status = WRONG_LEN_PKT;
            }
            else
            {
                status = CCSDS_TM_VALID;
            }
            break;
        case TC_SUBTYPE_LOAD_FBINS:
            if (length != (TC_LEN_LOAD_FBINS - CCSDS_TC_TM_PACKET_OFFSET))
            {
                status = WRONG_LEN_PKT;
            }
            else
            {
                status = CCSDS_TM_VALID;
            }
            break;
        case TC_SUBTYPE_LOAD_FILTER_PAR:
            if (length != (TC_LEN_LOAD_FILTER_PAR - CCSDS_TC_TM_PACKET_OFFSET))
            {
                status = WRONG_LEN_PKT;
            }
            else
            {
                status = CCSDS_TM_VALID;
            }
            break;
        case TC_SUBTYPE_UPDT_TIME:
            if (length != (TC_LEN_UPDT_TIME - CCSDS_TC_TM_PACKET_OFFSET))
            {
                status = WRONG_LEN_PKT;
            }
            else
            {
                status = CCSDS_TM_VALID;
            }
            break;
        default: // if the subtype is not a legal value, return ILL_SUBTYPE
            status = ILL_SUBTYPE;
            break;
    }

    return status;
}

int tc_check_crc(
    ccsdsTelecommandPacket_t* TCPacket, unsigned int length, unsigned char* computed_CRC)
{
    /** This function checks the CRC validity of the corresponding TeleCommand packet.
     *
     * @param TCPacket points to the TeleCommand packet to check.
     * @param length is the length of the TC packet.
     *
     * @return Status code CCSDS_TM_VALID or INCOR_CHECKSUM.
     *
     */

    int status;
    unsigned char* CCSDSContent;

    status = INCOR_CHECKSUM;

    CCSDSContent = (unsigned char*)TCPacket->packetID;
    GetCRCAsTwoBytes(CCSDSContent, computed_CRC,
        length + CCSDS_TC_TM_PACKET_OFFSET
            - BYTES_PER_CRC); // 2 CRC bytes removed from the calculation of the CRC

    if (computed_CRC[0] != CCSDSContent[length + CCSDS_TC_TM_PACKET_OFFSET - BYTES_PER_CRC])
    {
        status = INCOR_CHECKSUM;
    }
    else if (computed_CRC[1] != CCSDSContent[length + CCSDS_TC_TM_PACKET_OFFSET - 1])
    {
        status = INCOR_CHECKSUM;
    }
    else
    {
        status = CCSDS_TM_VALID;
    }

    return status;
}
