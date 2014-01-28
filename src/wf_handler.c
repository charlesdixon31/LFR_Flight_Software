/** Functions and tasks related to waveform packet generation.
 *
 * @file
 * @author P. LEROY
 *
 * A group of functions to handle waveforms, in snapshot or continuous format.\n
 *
 */

#include "wf_handler.h"

//*****************
// waveform headers
// SWF
Header_TM_LFR_SCIENCE_SWF_t headerSWF_F0[7];
Header_TM_LFR_SCIENCE_SWF_t headerSWF_F1[7];
Header_TM_LFR_SCIENCE_SWF_t headerSWF_F2[7];
// CWF
Header_TM_LFR_SCIENCE_CWF_t headerCWF_F1[7];
Header_TM_LFR_SCIENCE_CWF_t headerCWF_F2_BURST[7];
Header_TM_LFR_SCIENCE_CWF_t headerCWF_F2_SBM2[7];
Header_TM_LFR_SCIENCE_CWF_t headerCWF_F3[7];
Header_TM_LFR_SCIENCE_CWF_t headerCWF_F3_light[7];

//**************
// waveform ring
ring_node waveform_ring_f0[NB_RING_NODES_F0];
ring_node waveform_ring_f1[NB_RING_NODES_F1];
ring_node waveform_ring_f2[NB_RING_NODES_F2];
ring_node *current_ring_node_f0;
ring_node *ring_node_to_send_swf_f0;
ring_node *current_ring_node_f1;
ring_node *ring_node_to_send_swf_f1;
ring_node *ring_node_to_send_cwf_f1;
ring_node *current_ring_node_f2;
ring_node *ring_node_to_send_swf_f2;
ring_node *ring_node_to_send_cwf_f2;

unsigned char doubleSendCWF2 = 0;

rtems_isr waveforms_isr( rtems_vector_number vector )
{
    /** This is the interrupt sub routine called by the waveform picker core.
     *
     * This ISR launch different actions depending mainly on two pieces of information:
     * 1. the values read in the registers of the waveform picker.
     * 2. the current LFR mode.
     *
     */

    if ( (lfrCurrentMode == LFR_MODE_NORMAL)
         || (lfrCurrentMode == LFR_MODE_SBM1) || (lfrCurrentMode == LFR_MODE_SBM2) )
    { // in modes other than STANDBY and BURST, send the CWF_F3 data
        if ((waveform_picker_regs->status & 0x08) == 0x08){     // [1000] f3 is full
            // (1) change the receiving buffer for the waveform picker
            if (waveform_picker_regs->addr_data_f3 == (int) wf_cont_f3_a) {
                waveform_picker_regs->addr_data_f3 = (int) (wf_cont_f3_b);
            }
            else {
                waveform_picker_regs->addr_data_f3 = (int) (wf_cont_f3_a);
            }
            // (2) send an event for the waveforms transmission
            if (rtems_event_send( Task_id[TASKID_CWF3], RTEMS_EVENT_0 ) != RTEMS_SUCCESSFUL) {
                rtems_event_send( Task_id[TASKID_DUMB], RTEMS_EVENT_2 );
            }
            waveform_picker_regs->status = waveform_picker_regs->status & 0xfffff777; // reset f3 bits to 0, [1111 0111 0111 0111]
        }
    }

    switch(lfrCurrentMode)
    {
        //********
        // STANDBY
        case(LFR_MODE_STANDBY):
        break;

        //******
        // NORMAL
        case(LFR_MODE_NORMAL):
        if ( (waveform_picker_regs->status & 0x7) == 0x7 ){ // f2 f1 and f0 are full
            // change F0 ring node
            ring_node_to_send_swf_f0 = current_ring_node_f0;
            current_ring_node_f0 = current_ring_node_f0->next;
            waveform_picker_regs->addr_data_f0 = current_ring_node_f0->buffer_address;
            // change F1 ring node
            ring_node_to_send_swf_f1 = current_ring_node_f1;
            current_ring_node_f1 = current_ring_node_f1->next;
            waveform_picker_regs->addr_data_f1 = current_ring_node_f1->buffer_address;
            // change F2 ring node
            ring_node_to_send_swf_f2 = current_ring_node_f2;
            current_ring_node_f2 = current_ring_node_f2->next;
            waveform_picker_regs->addr_data_f2 = current_ring_node_f2->buffer_address;
            // send an event to the WFRM task
            if (rtems_event_send( Task_id[TASKID_WFRM], RTEMS_EVENT_MODE_NORMAL ) != RTEMS_SUCCESSFUL) {
                rtems_event_send( Task_id[TASKID_DUMB], RTEMS_EVENT_2 );
            }
            waveform_picker_regs->status = waveform_picker_regs->status & 0xfffff888; // [1000 1000 1000]
//            rtems_event_send( Task_id[TASKID_DUMB], RTEMS_EVENT_7 );
//            reset_wfp_burst_enable();
        }
        break;

        //******
        // BURST
        case(LFR_MODE_BURST):
        if ( (waveform_picker_regs->status & 0x04) == 0x04 ){ // [0100] check the f2 full bit
            // (1) change the receiving buffer for the waveform picker
            ring_node_to_send_cwf_f2 = current_ring_node_f2;
            current_ring_node_f2 = current_ring_node_f2->next;
            waveform_picker_regs->addr_data_f2 = current_ring_node_f2->buffer_address;
            // (2) send an event for the waveforms transmission
            if (rtems_event_send( Task_id[TASKID_CWF2], RTEMS_EVENT_MODE_BURST ) != RTEMS_SUCCESSFUL) {
                rtems_event_send( Task_id[TASKID_DUMB], RTEMS_EVENT_2 );
            }
            waveform_picker_regs->status = waveform_picker_regs->status & 0xfffffbbb; // [1111 1011 1011 1011] f2 bit = 0
        }
        break;

        //*****
        // SBM1
        case(LFR_MODE_SBM1):
        if ( (waveform_picker_regs->status & 0x02) == 0x02 ) { // [0010] check the f1 full bit
            // (1) change the receiving buffer for the waveform picker
            ring_node_to_send_cwf_f1 = current_ring_node_f1;
            current_ring_node_f1 = current_ring_node_f1->next;
            waveform_picker_regs->addr_data_f1 = current_ring_node_f1->buffer_address;
            // (2) send an event for the waveforms transmission
            if (rtems_event_send( Task_id[TASKID_CWF1], RTEMS_EVENT_MODE_SBM1 ) != RTEMS_SUCCESSFUL) {
                rtems_event_send( Task_id[TASKID_DUMB], RTEMS_EVENT_2 );
            }
            waveform_picker_regs->status = waveform_picker_regs->status & 0xfffffddd; // [1111 1101 1101 1101] f1 bit = 0
        }
        if ( (waveform_picker_regs->status & 0x01) == 0x01 ) { // [0001] check the f0 full bit
            ring_node_to_send_swf_f1 = current_ring_node_f1->previous;
        }
        if ( (waveform_picker_regs->status & 0x04) == 0x04 ) { // [0100] check the f2 full bit
            if (rtems_event_send( Task_id[TASKID_WFRM], RTEMS_EVENT_MODE_NORMAL ) != RTEMS_SUCCESSFUL) {
                rtems_event_send( Task_id[TASKID_DUMB], RTEMS_EVENT_2 );
            }
            waveform_picker_regs->status = waveform_picker_regs->status & 0xfffffaaa; // [1111 1010 1010 1010] f2 and f0 bits = 0
        }
        break;

        //*****
        // SBM2
        case(LFR_MODE_SBM2):
        if ( (waveform_picker_regs->status & 0x04) == 0x04 ){ // [0100] check the f2 full bit
            // (1) change the receiving buffer for the waveform picker
            ring_node_to_send_cwf_f2 = current_ring_node_f2;
            current_ring_node_f2 = current_ring_node_f2->next;
            waveform_picker_regs->addr_data_f2 = current_ring_node_f2->buffer_address;
            // (2) send an event for the waveforms transmission
            if (rtems_event_send( Task_id[TASKID_CWF2], RTEMS_EVENT_MODE_SBM2 ) != RTEMS_SUCCESSFUL) {
                rtems_event_send( Task_id[TASKID_DUMB], RTEMS_EVENT_2 );
            }
            waveform_picker_regs->status = waveform_picker_regs->status & 0xfffffbbb; // [1111 1011 1011 1011] f2 bit = 0
        }
        if ( (waveform_picker_regs->status & 0x01) == 0x01 ) { // [0001] check the f0 full bit
            ring_node_to_send_swf_f2 = current_ring_node_f2->previous;
        }
        if ( (waveform_picker_regs->status & 0x02) == 0x02 ) { // [0010] check the f1 full bit
            if (rtems_event_send( Task_id[TASKID_WFRM], RTEMS_EVENT_MODE_NORMAL ) != RTEMS_SUCCESSFUL) {
                rtems_event_send( Task_id[TASKID_DUMB], RTEMS_EVENT_2 );
            }
            waveform_picker_regs->status = waveform_picker_regs->status & 0xfffffccc; // [1111 1100 1100 1100] f1, f0 bits = 0
        }
        break;

        //********
        // DEFAULT
        default:
        break;
    }
}

rtems_task wfrm_task(rtems_task_argument argument) //used with the waveform picker VHDL IP
{
    /** This RTEMS task is dedicated to the transmission of snapshots of the NORMAL mode.
     *
     * @param unused is the starting argument of the RTEMS task
     *
     * The following data packets are sent by this task:
     * - TM_LFR_SCIENCE_NORMAL_SWF_F0
     * - TM_LFR_SCIENCE_NORMAL_SWF_F1
     * - TM_LFR_SCIENCE_NORMAL_SWF_F2
     *
     */

    rtems_event_set event_out;
    rtems_id queue_id;
    rtems_status_code status;

    init_header_snapshot_wf_table( SID_NORM_SWF_F0, headerSWF_F0 );
    init_header_snapshot_wf_table( SID_NORM_SWF_F1, headerSWF_F1 );
    init_header_snapshot_wf_table( SID_NORM_SWF_F2, headerSWF_F2 );

    init_waveforms();

    status =  get_message_queue_id_send( &queue_id );
    if (status != RTEMS_SUCCESSFUL)
    {
        PRINTF1("in WFRM *** ERR get_message_queue_id_send %d\n", status)
    }

    BOOT_PRINTF("in WFRM ***\n")

    while(1){
        // wait for an RTEMS_EVENT
        rtems_event_receive(RTEMS_EVENT_MODE_NORMAL | RTEMS_EVENT_MODE_SBM1
                            | RTEMS_EVENT_MODE_SBM2 | RTEMS_EVENT_MODE_SBM2_WFRM,
                            RTEMS_WAIT | RTEMS_EVENT_ANY, RTEMS_NO_TIMEOUT, &event_out);
        if (event_out == RTEMS_EVENT_MODE_NORMAL)
        {
            send_waveform_SWF((volatile int*) ring_node_to_send_swf_f0->buffer_address, SID_NORM_SWF_F0, headerSWF_F0, queue_id);
            send_waveform_SWF((volatile int*) ring_node_to_send_swf_f1->buffer_address, SID_NORM_SWF_F1, headerSWF_F1, queue_id);
            send_waveform_SWF((volatile int*) ring_node_to_send_swf_f2->buffer_address, SID_NORM_SWF_F2, headerSWF_F2, queue_id);
        }
        else
        {
            PRINTF("in WFRM *** unexpected event")
        }
    }
}

rtems_task cwf3_task(rtems_task_argument argument) //used with the waveform picker VHDL IP
{
    /** This RTEMS task is dedicated to the transmission of continuous waveforms at f3.
     *
     * @param unused is the starting argument of the RTEMS task
     *
     * The following data packet is sent by this task:
     * - TM_LFR_SCIENCE_NORMAL_CWF_F3
     *
     */

    rtems_event_set event_out;
    rtems_id queue_id;
    rtems_status_code status;

    init_header_continuous_wf_table( SID_NORM_CWF_LONG_F3, headerCWF_F3 );
    init_header_continuous_cwf3_light_table( headerCWF_F3_light );

    status =  get_message_queue_id_send( &queue_id );
    if (status != RTEMS_SUCCESSFUL)
    {
        PRINTF1("in CWF3 *** ERR get_message_queue_id_send %d\n", status)
    }

    BOOT_PRINTF("in CWF3 ***\n")

    while(1){
        // wait for an RTEMS_EVENT
        rtems_event_receive( RTEMS_EVENT_0,
                            RTEMS_WAIT | RTEMS_EVENT_ANY, RTEMS_NO_TIMEOUT, &event_out);
        PRINTF("send CWF F3 \n")
        if (waveform_picker_regs->addr_data_f3 == (int) wf_cont_f3_a) {
            if ( (parameter_dump_packet.sy_lfr_n_cwf_long_f3 & 0x01) == 0x01)
            {
                send_waveform_CWF( wf_cont_f3_b, SID_NORM_CWF_LONG_F3, headerCWF_F3, queue_id );
            }
            else
            {
                send_waveform_CWF3_light( wf_cont_f3_b, headerCWF_F3_light, queue_id );
            }
        }
        else
        {
            if ( (parameter_dump_packet.sy_lfr_n_cwf_long_f3 & 0x01) == 0x00)
            {
                send_waveform_CWF( wf_cont_f3_a, SID_NORM_CWF_LONG_F3, headerCWF_F3, queue_id );
            }
            else
            {
                send_waveform_CWF3_light( wf_cont_f3_a, headerCWF_F3_light, queue_id );
            }

        }
    }
}

rtems_task cwf2_task(rtems_task_argument argument)  // ONLY USED IN BURST AND SBM2
{
    /** This RTEMS task is dedicated to the transmission of continuous waveforms at f2.
     *
     * @param unused is the starting argument of the RTEMS task
     *
     * The following data packet is sent by this function:
     * - TM_LFR_SCIENCE_BURST_CWF_F2
     * - TM_LFR_SCIENCE_SBM2_CWF_F2
     *
     */

    rtems_event_set event_out;
    rtems_id queue_id;
    rtems_status_code status;

    init_header_continuous_wf_table( SID_BURST_CWF_F2, headerCWF_F2_BURST );
    init_header_continuous_wf_table( SID_SBM2_CWF_F2, headerCWF_F2_SBM2 );

    status =  get_message_queue_id_send( &queue_id );
    if (status != RTEMS_SUCCESSFUL)
    {
        PRINTF1("in CWF2 *** ERR get_message_queue_id_send %d\n", status)
    }

    BOOT_PRINTF("in CWF2 ***\n")

    while(1){
        // wait for an RTEMS_EVENT
        rtems_event_receive( RTEMS_EVENT_MODE_BURST | RTEMS_EVENT_MODE_SBM2,
                            RTEMS_WAIT | RTEMS_EVENT_ANY, RTEMS_NO_TIMEOUT, &event_out);
        if (event_out == RTEMS_EVENT_MODE_BURST)
        {
            send_waveform_CWF( (volatile int *) ring_node_to_send_cwf_f2->buffer_address, SID_BURST_CWF_F2, headerCWF_F2_BURST, queue_id );
        }
        if (event_out == RTEMS_EVENT_MODE_SBM2)
        {
            send_waveform_CWF( (volatile int *) ring_node_to_send_cwf_f2->buffer_address, SID_SBM2_CWF_F2, headerCWF_F2_SBM2, queue_id );
        }
    }
}

rtems_task cwf1_task(rtems_task_argument argument)  // ONLY USED IN SBM1
{
    /** This RTEMS task is dedicated to the transmission of continuous waveforms at f1.
     *
     * @param unused is the starting argument of the RTEMS task
     *
     * The following data packet is sent by this function:
     * - TM_LFR_SCIENCE_SBM1_CWF_F1
     *
     */

    rtems_event_set event_out;
    rtems_id queue_id;
    rtems_status_code status;

    init_header_continuous_wf_table( SID_SBM1_CWF_F1, headerCWF_F1 );

    status =  get_message_queue_id_send( &queue_id );
    if (status != RTEMS_SUCCESSFUL)
    {
        PRINTF1("in CWF1 *** ERR get_message_queue_id_send %d\n", status)
    }

    BOOT_PRINTF("in CWF1 ***\n")

    while(1){
        // wait for an RTEMS_EVENT
        rtems_event_receive( RTEMS_EVENT_MODE_SBM1,
                            RTEMS_WAIT | RTEMS_EVENT_ANY, RTEMS_NO_TIMEOUT, &event_out);
        send_waveform_CWF( (volatile int*) ring_node_to_send_cwf_f1->buffer_address, SID_SBM1_CWF_F1, headerCWF_F1, queue_id );
    }
}

//******************
// general functions
void init_waveforms( void )
{
    int i = 0;

    for (i=0; i< NB_SAMPLES_PER_SNAPSHOT; i++)
    {
        //***
        // F0
//        wf_snap_f0[ (i* NB_WORDS_SWF_BLK) + 0 + TIME_OFFSET ] = 0x88887777;     //
//        wf_snap_f0[ (i* NB_WORDS_SWF_BLK) + 1 + TIME_OFFSET  ] = 0x22221111;    //
//        wf_snap_f0[ (i* NB_WORDS_SWF_BLK) + 2 + TIME_OFFSET  ] = 0x44443333;    //

        //***
        // F1
//        wf_snap_f1[ (i* NB_WORDS_SWF_BLK) + 0 + TIME_OFFSET  ] = 0x22221111;
//        wf_snap_f1[ (i* NB_WORDS_SWF_BLK) + 1 + TIME_OFFSET  ] = 0x44443333;
//        wf_snap_f1[ (i* NB_WORDS_SWF_BLK) + 2 + TIME_OFFSET  ] = 0xaaaa0000;

        //***
        // F2
//        wf_snap_f2[ (i* NB_WORDS_SWF_BLK) + 0 + TIME_OFFSET  ] = 0x44443333;
//        wf_snap_f2[ (i* NB_WORDS_SWF_BLK) + 1 + TIME_OFFSET  ] = 0x22221111;
//        wf_snap_f2[ (i* NB_WORDS_SWF_BLK) + 2 + TIME_OFFSET  ] = 0xaaaa0000;

        //***
        // F3
//        wf_cont_f3[ (i* NB_WORDS_SWF_BLK) + 0 ] = val1;
//        wf_cont_f3[ (i* NB_WORDS_SWF_BLK) + 1 ] = val2;
//        wf_cont_f3[ (i* NB_WORDS_SWF_BLK) + 2 ] = 0xaaaa0000;
    }
}

void init_waveform_rings( void )
{
    unsigned char i;

    // F0 RING
    waveform_ring_f0[0].next            = (ring_node*) &waveform_ring_f0[1];
    waveform_ring_f0[0].previous        = (ring_node*) &waveform_ring_f0[NB_RING_NODES_F0-1];
    waveform_ring_f0[0].buffer_address  = (int) &wf_snap_f0[0][0];

    waveform_ring_f0[NB_RING_NODES_F0-1].next           = (ring_node*) &waveform_ring_f0[0];
    waveform_ring_f0[NB_RING_NODES_F0-1].previous       = (ring_node*) &waveform_ring_f0[NB_RING_NODES_F0-2];
    waveform_ring_f0[NB_RING_NODES_F0-1].buffer_address = (int) &wf_snap_f0[NB_RING_NODES_F0-1][0];

    for(i=1; i<NB_RING_NODES_F0-1; i++)
    {
        waveform_ring_f0[i].next            = (ring_node*) &waveform_ring_f0[i+1];
        waveform_ring_f0[i].previous        = (ring_node*) &waveform_ring_f0[i-1];
        waveform_ring_f0[i].buffer_address  = (int) &wf_snap_f0[i][0];
    }

    // F1 RING
    waveform_ring_f1[0].next            = (ring_node*) &waveform_ring_f1[1];
    waveform_ring_f1[0].previous        = (ring_node*) &waveform_ring_f1[NB_RING_NODES_F1-1];
    waveform_ring_f1[0].buffer_address  = (int) &wf_snap_f1[0][0];

    waveform_ring_f1[NB_RING_NODES_F1-1].next           = (ring_node*) &waveform_ring_f1[0];
    waveform_ring_f1[NB_RING_NODES_F1-1].previous       = (ring_node*) &waveform_ring_f1[NB_RING_NODES_F1-2];
    waveform_ring_f1[NB_RING_NODES_F1-1].buffer_address = (int) &wf_snap_f1[NB_RING_NODES_F1-1][0];

    for(i=1; i<NB_RING_NODES_F1-1; i++)
    {
        waveform_ring_f1[i].next            = (ring_node*) &waveform_ring_f1[i+1];
        waveform_ring_f1[i].previous        = (ring_node*) &waveform_ring_f1[i-1];
        waveform_ring_f1[i].buffer_address  = (int) &wf_snap_f1[i][0];
    }

    // F2 RING
    waveform_ring_f2[0].next            = (ring_node*) &waveform_ring_f2[1];
    waveform_ring_f2[0].previous        = (ring_node*) &waveform_ring_f2[NB_RING_NODES_F2-1];
    waveform_ring_f2[0].buffer_address  = (int) &wf_snap_f2[0][0];

    waveform_ring_f2[NB_RING_NODES_F2-1].next           = (ring_node*) &waveform_ring_f2[0];
    waveform_ring_f2[NB_RING_NODES_F2-1].previous       = (ring_node*) &waveform_ring_f2[NB_RING_NODES_F2-2];
    waveform_ring_f2[NB_RING_NODES_F2-1].buffer_address = (int) &wf_snap_f2[NB_RING_NODES_F2-1][0];

    for(i=1; i<NB_RING_NODES_F2-1; i++)
    {
        waveform_ring_f2[i].next            = (ring_node*) &waveform_ring_f2[i+1];
        waveform_ring_f2[i].previous        = (ring_node*) &waveform_ring_f2[i-1];
        waveform_ring_f2[i].buffer_address  = (int) &wf_snap_f2[i][0];
    }

    DEBUG_PRINTF1("waveform_ring_f0 @%x\n", (unsigned int) waveform_ring_f0)
    DEBUG_PRINTF1("waveform_ring_f1 @%x\n", (unsigned int) waveform_ring_f1)
    DEBUG_PRINTF1("waveform_ring_f2 @%x\n", (unsigned int) waveform_ring_f2)

}

void reset_current_ring_nodes( void )
{
    current_ring_node_f0        = waveform_ring_f0;
    ring_node_to_send_swf_f0    = waveform_ring_f0;

    current_ring_node_f1        = waveform_ring_f1;
    ring_node_to_send_cwf_f1    = waveform_ring_f1;
    ring_node_to_send_swf_f1    = waveform_ring_f1;

    current_ring_node_f2        = waveform_ring_f2;
    ring_node_to_send_cwf_f2    = waveform_ring_f2;
    ring_node_to_send_swf_f2    = waveform_ring_f2;
}

int init_header_snapshot_wf_table( unsigned int sid, Header_TM_LFR_SCIENCE_SWF_t *headerSWF)
{
    unsigned char i;

    for (i=0; i<7; i++)
    {
        headerSWF[ i ].targetLogicalAddress = CCSDS_DESTINATION_ID;
        headerSWF[ i ].protocolIdentifier = CCSDS_PROTOCOLE_ID;
        headerSWF[ i ].reserved = DEFAULT_RESERVED;
        headerSWF[ i ].userApplication = CCSDS_USER_APP;
        headerSWF[ i ].packetID[0] = (unsigned char) (TM_PACKET_ID_SCIENCE_NORMAL_BURST >> 8);
        headerSWF[ i ].packetID[1] = (unsigned char) (TM_PACKET_ID_SCIENCE_NORMAL_BURST);
        headerSWF[ i ].packetSequenceControl[0] = TM_PACKET_SEQ_CTRL_STANDALONE;
        if (i == 6)
        {
            headerSWF[ i ].packetLength[0] = (unsigned char) (TM_LEN_SCI_SWF_224 >> 8);
            headerSWF[ i ].packetLength[1] = (unsigned char) (TM_LEN_SCI_SWF_224     );
            headerSWF[ i ].blkNr[0] = (unsigned char) (BLK_NR_224 >> 8);
            headerSWF[ i ].blkNr[1] = (unsigned char) (BLK_NR_224     );
        }
        else
        {
            headerSWF[ i ].packetLength[0] = (unsigned char) (TM_LEN_SCI_SWF_304 >> 8);
            headerSWF[ i ].packetLength[1] = (unsigned char) (TM_LEN_SCI_SWF_304     );
            headerSWF[ i ].blkNr[0] = (unsigned char) (BLK_NR_304 >> 8);
            headerSWF[ i ].blkNr[1] = (unsigned char) (BLK_NR_304     );
        }
        headerSWF[ i ].packetSequenceControl[1] = TM_PACKET_SEQ_CNT_DEFAULT;
        headerSWF[ i ].pktCnt = DEFAULT_PKTCNT;  // PKT_CNT
        headerSWF[ i ].pktNr = i+1;    // PKT_NR
        // DATA FIELD HEADER
        headerSWF[ i ].spare1_pusVersion_spare2 = DEFAULT_SPARE1_PUSVERSION_SPARE2;
        headerSWF[ i ].serviceType = TM_TYPE_LFR_SCIENCE; // service type
        headerSWF[ i ].serviceSubType = TM_SUBTYPE_LFR_SCIENCE; // service subtype
        headerSWF[ i ].destinationID = TM_DESTINATION_ID_GROUND;
        // AUXILIARY DATA HEADER
        headerSWF[ i ].time[0] = 0x00;
        headerSWF[ i ].time[0] = 0x00;
        headerSWF[ i ].time[0] = 0x00;
        headerSWF[ i ].time[0] = 0x00;
        headerSWF[ i ].time[0] = 0x00;
        headerSWF[ i ].time[0] = 0x00;
        headerSWF[ i ].sid = sid;
        headerSWF[ i ].hkBIA = DEFAULT_HKBIA;
    }
    return LFR_SUCCESSFUL;
}

int init_header_continuous_wf_table( unsigned int sid, Header_TM_LFR_SCIENCE_CWF_t *headerCWF )
{
    unsigned int i;

    for (i=0; i<7; i++)
    {
        headerCWF[ i ].targetLogicalAddress = CCSDS_DESTINATION_ID;
        headerCWF[ i ].protocolIdentifier = CCSDS_PROTOCOLE_ID;
        headerCWF[ i ].reserved = DEFAULT_RESERVED;
        headerCWF[ i ].userApplication = CCSDS_USER_APP;
        if ( (sid == SID_SBM1_CWF_F1) || (sid == SID_SBM2_CWF_F2) )
        {
            headerCWF[ i ].packetID[0] = (unsigned char) (TM_PACKET_ID_SCIENCE_SBM1_SBM2 >> 8);
            headerCWF[ i ].packetID[1] = (unsigned char) (TM_PACKET_ID_SCIENCE_SBM1_SBM2);
        }
        else
        {
            headerCWF[ i ].packetID[0] = (unsigned char) (TM_PACKET_ID_SCIENCE_NORMAL_BURST >> 8);
            headerCWF[ i ].packetID[1] = (unsigned char) (TM_PACKET_ID_SCIENCE_NORMAL_BURST);
        }
        if (i == 0)
        {
            headerCWF[ i ].packetSequenceControl[0] = TM_PACKET_SEQ_CTRL_FIRST;
            headerCWF[ i ].packetLength[0] = (unsigned char) (TM_LEN_SCI_CWF_340 >> 8);
            headerCWF[ i ].packetLength[1] = (unsigned char) (TM_LEN_SCI_CWF_340     );
            headerCWF[ i ].blkNr[0] = (unsigned char) (BLK_NR_340 >> 8);
            headerCWF[ i ].blkNr[1] = (unsigned char) (BLK_NR_340     );
        }
        else if (i == 6)
        {
            headerCWF[ i ].packetSequenceControl[0] = TM_PACKET_SEQ_CTRL_LAST;
            headerCWF[ i ].packetLength[0] = (unsigned char) (TM_LEN_SCI_CWF_8 >> 8);
            headerCWF[ i ].packetLength[1] = (unsigned char) (TM_LEN_SCI_CWF_8     );
            headerCWF[ i ].blkNr[0] = (unsigned char) (BLK_NR_8 >> 8);
            headerCWF[ i ].blkNr[1] = (unsigned char) (BLK_NR_8     );
        }
        else
        {
            headerCWF[ i ].packetSequenceControl[0] = TM_PACKET_SEQ_CTRL_CONTINUATION;
            headerCWF[ i ].packetLength[0] = (unsigned char) (TM_LEN_SCI_CWF_340 >> 8);
            headerCWF[ i ].packetLength[1] = (unsigned char) (TM_LEN_SCI_CWF_340     );
            headerCWF[ i ].blkNr[0] = (unsigned char) (BLK_NR_340 >> 8);
            headerCWF[ i ].blkNr[1] = (unsigned char) (BLK_NR_340     );
        }
        headerCWF[ i ].packetSequenceControl[1] = TM_PACKET_SEQ_CNT_DEFAULT;
        // PKT_CNT
        // PKT_NR
        // DATA FIELD HEADER
        headerCWF[ i ].spare1_pusVersion_spare2 = DEFAULT_SPARE1_PUSVERSION_SPARE2;
        headerCWF[ i ].serviceType = TM_TYPE_LFR_SCIENCE; // service type
        headerCWF[ i ].serviceSubType = TM_SUBTYPE_LFR_SCIENCE; // service subtype
        headerCWF[ i ].destinationID = TM_DESTINATION_ID_GROUND;
        // AUXILIARY DATA HEADER
        headerCWF[ i ].sid = sid;
        headerCWF[ i ].hkBIA = DEFAULT_HKBIA;
        headerCWF[ i ].time[0] = 0x00;
        headerCWF[ i ].time[0] = 0x00;
        headerCWF[ i ].time[0] = 0x00;
        headerCWF[ i ].time[0] = 0x00;
        headerCWF[ i ].time[0] = 0x00;
        headerCWF[ i ].time[0] = 0x00;
    }
    return LFR_SUCCESSFUL;
}

int init_header_continuous_cwf3_light_table( Header_TM_LFR_SCIENCE_CWF_t *headerCWF )
{
    unsigned int i;

    for (i=0; i<7; i++)
    {
        headerCWF[ i ].targetLogicalAddress = CCSDS_DESTINATION_ID;
        headerCWF[ i ].protocolIdentifier = CCSDS_PROTOCOLE_ID;
        headerCWF[ i ].reserved = DEFAULT_RESERVED;
        headerCWF[ i ].userApplication = CCSDS_USER_APP;

        headerCWF[ i ].packetID[0] = (unsigned char) (TM_PACKET_ID_SCIENCE_NORMAL_BURST >> 8);
        headerCWF[ i ].packetID[1] = (unsigned char) (TM_PACKET_ID_SCIENCE_NORMAL_BURST);
        if (i == 0)
        {
            headerCWF[ i ].packetSequenceControl[0] = TM_PACKET_SEQ_CTRL_FIRST;
            headerCWF[ i ].packetLength[0] = (unsigned char) (TM_LEN_SCI_CWF3_LIGHT_340 >> 8);
            headerCWF[ i ].packetLength[1] = (unsigned char) (TM_LEN_SCI_CWF3_LIGHT_340     );
            headerCWF[ i ].blkNr[0] = (unsigned char) (BLK_NR_340 >> 8);
            headerCWF[ i ].blkNr[1] = (unsigned char) (BLK_NR_340     );
        }
        else if (i == 6)
        {
            headerCWF[ i ].packetSequenceControl[0] = TM_PACKET_SEQ_CTRL_LAST;
            headerCWF[ i ].packetLength[0] = (unsigned char) (TM_LEN_SCI_CWF3_LIGHT_8 >> 8);
            headerCWF[ i ].packetLength[1] = (unsigned char) (TM_LEN_SCI_CWF3_LIGHT_8     );
            headerCWF[ i ].blkNr[0] = (unsigned char) (BLK_NR_8 >> 8);
            headerCWF[ i ].blkNr[1] = (unsigned char) (BLK_NR_8     );
        }
        else
        {
            headerCWF[ i ].packetSequenceControl[0] = TM_PACKET_SEQ_CTRL_CONTINUATION;
            headerCWF[ i ].packetLength[0] = (unsigned char) (TM_LEN_SCI_CWF3_LIGHT_340 >> 8);
            headerCWF[ i ].packetLength[1] = (unsigned char) (TM_LEN_SCI_CWF3_LIGHT_340     );
            headerCWF[ i ].blkNr[0] = (unsigned char) (BLK_NR_340 >> 8);
            headerCWF[ i ].blkNr[1] = (unsigned char) (BLK_NR_340     );
        }
        headerCWF[ i ].packetSequenceControl[1] = TM_PACKET_SEQ_CNT_DEFAULT;
        // DATA FIELD HEADER
        headerCWF[ i ].spare1_pusVersion_spare2 = DEFAULT_SPARE1_PUSVERSION_SPARE2;
        headerCWF[ i ].serviceType = TM_TYPE_LFR_SCIENCE; // service type
        headerCWF[ i ].serviceSubType = TM_SUBTYPE_LFR_SCIENCE; // service subtype
        headerCWF[ i ].destinationID = TM_DESTINATION_ID_GROUND;
        // AUXILIARY DATA HEADER
        headerCWF[ i ].sid = SID_NORM_CWF_F3;
        headerCWF[ i ].hkBIA = DEFAULT_HKBIA;
        headerCWF[ i ].time[0] = 0x00;
        headerCWF[ i ].time[0] = 0x00;
        headerCWF[ i ].time[0] = 0x00;
        headerCWF[ i ].time[0] = 0x00;
        headerCWF[ i ].time[0] = 0x00;
        headerCWF[ i ].time[0] = 0x00;
    }
    return LFR_SUCCESSFUL;
}

int send_waveform_SWF( volatile int *waveform, unsigned int sid,
                       Header_TM_LFR_SCIENCE_SWF_t *headerSWF, rtems_id queue_id )
{
    /** This function sends SWF CCSDS packets (F2, F1 or F0).
     *
     * @param waveform points to the buffer containing the data that will be send.
     * @param sid is the source identifier of the data that will be sent.
     * @param headerSWF points to a table of headers that have been prepared for the data transmission.
     * @param queue_id is the id of the rtems queue to which spw_ioctl_pkt_send structures will be send. The structures
     * contain information to setup the transmission of the data packets.
     *
     * One group of 2048 samples is sent as 7 consecutive packets, 6 packets containing 340 blocks and 8 packets containing 8 blocks.
     *
     */

    unsigned int i;
    int ret;
    rtems_status_code status;
    spw_ioctl_pkt_send spw_ioctl_send_SWF;

    spw_ioctl_send_SWF.hlen = TM_HEADER_LEN + 4 + 12; // + 4 is for the protocole extra header, + 12 is for the auxiliary header
    spw_ioctl_send_SWF.options = 0;

    ret = LFR_DEFAULT;

    for (i=0; i<7; i++) // send waveform
    {
#ifdef VHDL_DEV
        spw_ioctl_send_SWF.data = (char*) &waveform[ (i * 304 * NB_WORDS_SWF_BLK) + TIME_OFFSET];
#else
        spw_ioctl_send_SWF.data = (char*) &waveform[ (i * 304 * NB_WORDS_SWF_BLK) ];
#endif
        spw_ioctl_send_SWF.hdr = (char*) &headerSWF[ i ];
        // BUILD THE DATA
        if (i==6) {
            spw_ioctl_send_SWF.dlen = 224 * NB_BYTES_SWF_BLK;
        }
        else {
            spw_ioctl_send_SWF.dlen = 304 * NB_BYTES_SWF_BLK;
        }
        // SET PACKET SEQUENCE COUNTER
        increment_seq_counter_source_id( headerSWF[ i ].packetSequenceControl, sid );
        // SET PACKET TIME
        headerSWF[ i ].acquisitionTime[0] = (unsigned char) (time_management_regs->coarse_time>>24);
        headerSWF[ i ].acquisitionTime[1] = (unsigned char) (time_management_regs->coarse_time>>16);
        headerSWF[ i ].acquisitionTime[2] = (unsigned char) (time_management_regs->coarse_time>>8);
        headerSWF[ i ].acquisitionTime[3] = (unsigned char) (time_management_regs->coarse_time);
        headerSWF[ i ].acquisitionTime[4] = (unsigned char) (time_management_regs->fine_time>>8);
        headerSWF[ i ].acquisitionTime[5] = (unsigned char) (time_management_regs->fine_time);
        headerSWF[ i ].time[0] = (unsigned char) (time_management_regs->coarse_time>>24);
        headerSWF[ i ].time[1] = (unsigned char) (time_management_regs->coarse_time>>16);
        headerSWF[ i ].time[2] = (unsigned char) (time_management_regs->coarse_time>>8);
        headerSWF[ i ].time[3] = (unsigned char) (time_management_regs->coarse_time);
        headerSWF[ i ].time[4] = (unsigned char) (time_management_regs->fine_time>>8);
        headerSWF[ i ].time[5] = (unsigned char) (time_management_regs->fine_time);
        // SEND PACKET
        status =  rtems_message_queue_send( queue_id, &spw_ioctl_send_SWF, ACTION_MSG_SPW_IOCTL_SEND_SIZE);
        if (status != RTEMS_SUCCESSFUL) {
            printf("%d-%d, ERR %d\n", sid, i, (int) status);
            ret = LFR_DEFAULT;
        }
        rtems_task_wake_after(TIME_BETWEEN_TWO_SWF_PACKETS);  // 300 ms between each packet => 7 * 3 = 21 packets => 6.3 seconds
    }

    return ret;
}

int send_waveform_CWF(volatile int *waveform, unsigned int sid,
                      Header_TM_LFR_SCIENCE_CWF_t *headerCWF, rtems_id queue_id)
{
    /** This function sends CWF CCSDS packets (F2, F1 or F0).
     *
     * @param waveform points to the buffer containing the data that will be send.
     * @param sid is the source identifier of the data that will be sent.
     * @param headerCWF points to a table of headers that have been prepared for the data transmission.
     * @param queue_id is the id of the rtems queue to which spw_ioctl_pkt_send structures will be send. The structures
     * contain information to setup the transmission of the data packets.
     *
     * One group of 2048 samples is sent as 7 consecutive packets, 6 packets containing 340 blocks and 8 packets containing 8 blocks.
     *
     */

    unsigned int i;
    int ret;
    rtems_status_code status;
    spw_ioctl_pkt_send spw_ioctl_send_CWF;

    spw_ioctl_send_CWF.hlen = TM_HEADER_LEN + 4 + 10; // + 4 is for the protocole extra header, + 10 is for the auxiliary header
    spw_ioctl_send_CWF.options = 0;

    ret = LFR_DEFAULT;

    for (i=0; i<7; i++) // send waveform
    {
        int coarseTime = 0x00;
        int fineTime = 0x00;
#ifdef VHDL_DEV
        spw_ioctl_send_CWF.data = (char*) &waveform[ (i * 340 * NB_WORDS_SWF_BLK) + TIME_OFFSET];
#else
        spw_ioctl_send_CWF.data = (char*) &waveform[ (i * 340 * NB_WORDS_SWF_BLK) ];
#endif
        spw_ioctl_send_CWF.hdr = (char*) &headerCWF[ i ];
        // BUILD THE DATA
        if (i==6) {
            spw_ioctl_send_CWF.dlen = 8 * NB_BYTES_SWF_BLK;
        }
        else {
            spw_ioctl_send_CWF.dlen = 340 * NB_BYTES_SWF_BLK;
        }
        // SET PACKET SEQUENCE COUNTER
        increment_seq_counter_source_id( headerCWF[ i ].packetSequenceControl, sid );
        // SET PACKET TIME
        coarseTime = time_management_regs->coarse_time;
        fineTime = time_management_regs->fine_time;
        headerCWF[ i ].acquisitionTime[0] = (unsigned char) (coarseTime>>24);
        headerCWF[ i ].acquisitionTime[1] = (unsigned char) (coarseTime>>16);
        headerCWF[ i ].acquisitionTime[2] = (unsigned char) (coarseTime>>8);
        headerCWF[ i ].acquisitionTime[3] = (unsigned char) (coarseTime);
        headerCWF[ i ].acquisitionTime[4] = (unsigned char) (fineTime>>8);
        headerCWF[ i ].acquisitionTime[5] = (unsigned char) (fineTime);
        headerCWF[ i ].time[0] = (unsigned char) (coarseTime>>24);
        headerCWF[ i ].time[1] = (unsigned char) (coarseTime>>16);
        headerCWF[ i ].time[2] = (unsigned char) (coarseTime>>8);
        headerCWF[ i ].time[3] = (unsigned char) (coarseTime);
        headerCWF[ i ].time[4] = (unsigned char) (fineTime>>8);
        headerCWF[ i ].time[5] = (unsigned char) (fineTime);
        // SEND PACKET
        if (sid == SID_NORM_CWF_LONG_F3)
        {
            status =  rtems_message_queue_send( queue_id, &spw_ioctl_send_CWF, sizeof(spw_ioctl_send_CWF));
            if (status != RTEMS_SUCCESSFUL) {
                printf("%d-%d, ERR %d\n", sid, i, (int) status);
                ret = LFR_DEFAULT;
            }
            rtems_task_wake_after(TIME_BETWEEN_TWO_CWF3_PACKETS);
        }
        else
        {
            status =  rtems_message_queue_send( queue_id, &spw_ioctl_send_CWF, sizeof(spw_ioctl_send_CWF));
            if (status != RTEMS_SUCCESSFUL) {
                printf("%d-%d, ERR %d\n", sid, i, (int) status);
                ret = LFR_DEFAULT;
            }
        }
    }

    return ret;
}

int send_waveform_CWF3_light(volatile int *waveform, Header_TM_LFR_SCIENCE_CWF_t *headerCWF, rtems_id queue_id)
{
    /** This function sends CWF_F3 CCSDS packets without the b1, b2 and b3 data.
     *
     * @param waveform points to the buffer containing the data that will be send.
     * @param headerCWF points to a table of headers that have been prepared for the data transmission.
     * @param queue_id is the id of the rtems queue to which spw_ioctl_pkt_send structures will be send. The structures
     * contain information to setup the transmission of the data packets.
     *
     * By default, CWF_F3 packet are send without the b1, b2 and b3 data. This function rebuilds a data buffer
     * from the incoming data and sends it in 7 packets, 6 containing 340 blocks and 1 one containing 8 blocks.
     *
     */

    unsigned int i;
    int ret;
    rtems_status_code status;
    spw_ioctl_pkt_send spw_ioctl_send_CWF;
    char *sample;

    spw_ioctl_send_CWF.hlen = TM_HEADER_LEN + 4 + 10; // + 4 is for the protocole extra header, + 10 is for the auxiliary header
    spw_ioctl_send_CWF.options = 0;

    ret = LFR_DEFAULT;

    //**********************
    // BUILD CWF3_light DATA
    for ( i=0; i< 2048; i++)
    {
#ifdef VHDL_DEV
        sample = (char*) &waveform[ (i * NB_WORDS_SWF_BLK) + TIME_OFFSET ];
#else
        sample = (char*) &waveform[ i * NB_WORDS_SWF_BLK ];
#endif
        wf_cont_f3_light[ (i * NB_BYTES_CWF3_LIGHT_BLK)     ] = sample[ 0 ];
        wf_cont_f3_light[ (i * NB_BYTES_CWF3_LIGHT_BLK) + 1 ] = sample[ 1 ];
        wf_cont_f3_light[ (i * NB_BYTES_CWF3_LIGHT_BLK) + 2 ] = sample[ 2 ];
        wf_cont_f3_light[ (i * NB_BYTES_CWF3_LIGHT_BLK) + 3 ] = sample[ 3 ];
        wf_cont_f3_light[ (i * NB_BYTES_CWF3_LIGHT_BLK) + 4 ] = sample[ 4 ];
        wf_cont_f3_light[ (i * NB_BYTES_CWF3_LIGHT_BLK) + 5 ] = sample[ 5 ];
    }

    //*********************
    // SEND CWF3_light DATA

    for (i=0; i<7; i++) // send waveform
    {
        int coarseTime = 0x00;
        int fineTime = 0x00;

        spw_ioctl_send_CWF.data = (char*) &wf_cont_f3_light[ (i * 340 * NB_BYTES_CWF3_LIGHT_BLK) ];
        spw_ioctl_send_CWF.hdr = (char*) &headerCWF[ i ];
        // BUILD THE DATA
        if ( i == WFRM_INDEX_OF_LAST_PACKET ) {
            spw_ioctl_send_CWF.dlen = 8 * NB_BYTES_CWF3_LIGHT_BLK;
        }
        else {
            spw_ioctl_send_CWF.dlen = 340 * NB_BYTES_CWF3_LIGHT_BLK;
        }
        // SET PACKET SEQUENCE COUNTER
        increment_seq_counter_source_id( headerCWF[ i ].packetSequenceControl, SID_NORM_CWF_F3 );
        // SET PACKET TIME
        coarseTime = time_management_regs->coarse_time;
        fineTime = time_management_regs->fine_time;
        headerCWF[ i ].acquisitionTime[0] = (unsigned char) (coarseTime>>24);
        headerCWF[ i ].acquisitionTime[1] = (unsigned char) (coarseTime>>16);
        headerCWF[ i ].acquisitionTime[2] = (unsigned char) (coarseTime>>8);
        headerCWF[ i ].acquisitionTime[3] = (unsigned char) (coarseTime);
        headerCWF[ i ].acquisitionTime[4] = (unsigned char) (fineTime>>8);
        headerCWF[ i ].acquisitionTime[5] = (unsigned char) (fineTime);
        headerCWF[ i ].time[0] = (unsigned char) (coarseTime>>24);
        headerCWF[ i ].time[1] = (unsigned char) (coarseTime>>16);
        headerCWF[ i ].time[2] = (unsigned char) (coarseTime>>8);
        headerCWF[ i ].time[3] = (unsigned char) (coarseTime);
        headerCWF[ i ].time[4] = (unsigned char) (fineTime>>8);
        headerCWF[ i ].time[5] = (unsigned char) (fineTime);
        // SEND PACKET
        status =  rtems_message_queue_send( queue_id, &spw_ioctl_send_CWF, sizeof(spw_ioctl_send_CWF));
        if (status != RTEMS_SUCCESSFUL) {
            printf("%d-%d, ERR %d\n", SID_NORM_CWF_F3, i, (int) status);
            ret = LFR_DEFAULT;
        }
        rtems_task_wake_after(TIME_BETWEEN_TWO_CWF3_PACKETS);
    }

    return ret;
}


//**************
// wfp registers
void set_wfp_data_shaping()
{
    /** This function sets the data_shaping register of the waveform picker module.
     *
     * The value is read from one field of the parameter_dump_packet structure:\n
     * bw_sp0_sp1_r0_r1
     *
     */

    unsigned char data_shaping;

    // get the parameters for the data shaping [BW SP0 SP1 R0 R1] in sy_lfr_common1 and configure the register
    // waveform picker : [R1 R0 SP1 SP0 BW]

    data_shaping = parameter_dump_packet.bw_sp0_sp1_r0_r1;

#ifdef GSA
#else
    waveform_picker_regs->data_shaping =
              ( (data_shaping & 0x10) >> 4 )     // BW
            + ( (data_shaping & 0x08) >> 2 )     // SP0
            + ( (data_shaping & 0x04)      )     // SP1
            + ( (data_shaping & 0x02) << 2 )     // R0
            + ( (data_shaping & 0x01) << 4 );    // R1
#endif
}

char set_wfp_delta_snapshot()
{
    /** This function sets the delta_snapshot register of the waveform picker module.
     *
     * The value is read from two (unsigned char) of the parameter_dump_packet structure:
     * - sy_lfr_n_swf_p[0]
     * - sy_lfr_n_swf_p[1]
     *
     */

    char ret;
    unsigned int delta_snapshot;
    unsigned int aux;

    aux = 0;
    ret = LFR_DEFAULT;

    delta_snapshot = parameter_dump_packet.sy_lfr_n_swf_p[0]*256
            + parameter_dump_packet.sy_lfr_n_swf_p[1];

#ifdef GSA
#else
    if ( delta_snapshot < MIN_DELTA_SNAPSHOT )
    {
        aux = MIN_DELTA_SNAPSHOT;
        ret = LFR_DEFAULT;
    }
    else
    {
        aux = delta_snapshot ;
        ret = LFR_SUCCESSFUL;
    }
    waveform_picker_regs->delta_snapshot = aux - 1;             // max 2 bytes
#endif

    return ret;
}

#ifdef VHDL_DEV
void set_wfp_burst_enable_register( unsigned char mode )
{
    /** This function sets the waveform picker burst_enable register depending on the mode.
     *
     * @param mode is the LFR mode to launch.
     *
     * The burst bits shall be before the enable bits.
     *
     */

    // [0000 0000] burst f2, f1, f0 enable f3 f2 f1 f0
    // the burst bits shall be set first, before the enable bits
    switch(mode) {
    case(LFR_MODE_NORMAL):
        waveform_picker_regs->run_burst_enable = 0x00;  // [0000 0000] no burst enable
        waveform_picker_regs->run_burst_enable = 0x0f; // [0000 1111] enable f3 f2 f1 f0
        break;
    case(LFR_MODE_BURST):
        waveform_picker_regs->run_burst_enable = 0x40;  // [0100 0000] f2 burst enabled
        waveform_picker_regs->run_burst_enable =  waveform_picker_regs->run_burst_enable | 0x04; // [0100] enable f2
        break;
    case(LFR_MODE_SBM1):
        waveform_picker_regs->run_burst_enable = 0x20;  // [0010 0000] f1 burst enabled
        waveform_picker_regs->run_burst_enable =  waveform_picker_regs->run_burst_enable | 0x0f; // [1111] enable f3 f2 f1 f0
        break;
    case(LFR_MODE_SBM2):
        waveform_picker_regs->run_burst_enable = 0x40;  // [0100 0000] f2 burst enabled
        waveform_picker_regs->run_burst_enable =  waveform_picker_regs->run_burst_enable | 0x0f; // [1111] enable f3 f2 f1 f0
        break;
    default:
        waveform_picker_regs->run_burst_enable = 0x00;  // [0000 0000] no burst enabled, no waveform enabled
        break;
    }
}
#else
void set_wfp_burst_enable_register( unsigned char mode )
{
    /** This function sets the waveform picker burst_enable register depending on the mode.
     *
     * @param mode is the LFR mode to launch.
     *
     * The burst bits shall be before the enable bits.
     *
     */

    // [0000 0000] burst f2, f1, f0 enable f3 f2 f1 f0
    // the burst bits shall be set first, before the enable bits
    switch(mode) {
    case(LFR_MODE_NORMAL):
        waveform_picker_regs->burst_enable = 0x00;  // [0000 0000] no burst enable
        waveform_picker_regs->burst_enable = 0x0f; // [0000 1111] enable f3 f2 f1 f0
        break;
    case(LFR_MODE_BURST):
        waveform_picker_regs->burst_enable = 0x40;  // [0100 0000] f2 burst enabled
        waveform_picker_regs->burst_enable =  waveform_picker_regs->burst_enable | 0x04; // [0100] enable f2
        break;
    case(LFR_MODE_SBM1):
        waveform_picker_regs->burst_enable = 0x20;  // [0010 0000] f1 burst enabled
        waveform_picker_regs->burst_enable =  waveform_picker_regs->burst_enable | 0x0f; // [1111] enable f3 f2 f1 f0
        break;
    case(LFR_MODE_SBM2):
        waveform_picker_regs->burst_enable = 0x40;  // [0100 0000] f2 burst enabled
        waveform_picker_regs->burst_enable =  waveform_picker_regs->burst_enable | 0x0f; // [1111] enable f3 f2 f1 f0
        break;
    default:
        waveform_picker_regs->burst_enable = 0x00;  // [0000 0000] no burst enabled, no waveform enabled
        break;
    }
}
#endif

void reset_wfp_burst_enable()
{
    /** This function resets the waveform picker burst_enable register.
     *
     * The burst bits [f2 f1 f0] and the enable bits [f3 f2 f1 f0] are set to 0.
     *
     */

#ifdef VHDL_DEV
    waveform_picker_regs->run_burst_enable = 0x00;              // burst f2, f1, f0     enable f3, f2, f1, f0
#else
    waveform_picker_regs->burst_enable = 0x00;              // burst f2, f1, f0     enable f3, f2, f1, f0
#endif
}

void reset_wfp_status()
{
    /** This function resets the waveform picker status register.
     *
     * All status bits are set to 0 [new_err full_err full].
     *
     */

#ifdef GSA
#else
    waveform_picker_regs->status = 0x00;              // burst f2, f1, f0     enable f3, f2, f1, f0
#endif
}

#ifdef VHDL_DEV
void reset_waveform_picker_regs()
{
    /** This function resets the waveform picker module registers.
    *
    * The registers affected by this function are located at the following offset addresses:
    * - 0x00 data_shaping
    * - 0x04 run_burst_enable
    * - 0x08 addr_data_f0
    * - 0x0C addr_data_f1
    * - 0x10 addr_data_f2
    * - 0x14 addr_data_f3
    * - 0x18 status
    * - 0x1C delta_snapshot
    * - 0x20 delta_f0
    * - 0x24 delta_f0_2
    * - 0x28 delta_f1
    * - 0x2c delta_f2
    * - 0x30 nb_data_by_buffer
    * - 0x34 nb_snapshot_param
    * - 0x38 start_date
    * - 0x3c nb_word_in_buffer
    *
    */
    waveform_picker_regs->data_shaping = 0x01; // 0x00 *** R1 R0 SP1 SP0 BW
    waveform_picker_regs->run_burst_enable = 0x00; // 0x04 *** [run *** burst f2, f1, f0 *** enable f3, f2, f1, f0 ]
    //waveform_picker_regs->addr_data_f0 = (int) (wf_snap_f0); // 0x08
    waveform_picker_regs->addr_data_f0 = current_ring_node_f0->buffer_address; // 0x08
    waveform_picker_regs->addr_data_f1 = current_ring_node_f1->buffer_address; // 0x0c
    waveform_picker_regs->addr_data_f2 = current_ring_node_f2->buffer_address; // 0x10
    waveform_picker_regs->addr_data_f3 = (int) (wf_cont_f3_a); // 0x14
    waveform_picker_regs->status = 0x00; // 0x18
    // waveform_picker_regs->delta_snapshot = 0x12800; // 0x1c 296 * 256 = 75776
    waveform_picker_regs->delta_snapshot = 0x1000; // 0x1c 16 * 256 = 4096
    //waveform_picker_regs->delta_snapshot = 0x2000; // 0x1c 32 * 256 = 8192
    waveform_picker_regs->delta_f0 = 0xbf5; // 0x20 *** 1013
    waveform_picker_regs->delta_f0_2 = 0x7; // 0x24 *** 7 [7 bits]
    waveform_picker_regs->delta_f1 = 0xbc0; // 0x28 *** 960
    // waveform_picker_regs->delta_f2 = 0x12200; // 0x2c *** 290 * 256 = 74240
    waveform_picker_regs->delta_f2 = 0xc00;             // 0x2c *** 12 * 256 = 3072
    waveform_picker_regs->nb_data_by_buffer = 0x7ff;    // 0x30 *** 2048 -1 => nb samples -1
    waveform_picker_regs->snapshot_param = 0x800; // 0x34 *** 2048 => nb samples
    waveform_picker_regs->start_date = 0x00; // 0x38
    waveform_picker_regs->nb_word_in_buffer = 0x1802; // 0x3c *** 2048 * 3 + 2 = 6146
}
#else
void reset_waveform_picker_regs()
{
    /** This function resets the waveform picker module registers.
     *
     * The registers affected by this function are located at the following offset addresses:
     * - 0x00 data_shaping
     * - 0x04 burst_enable
     * - 0x08 addr_data_f0
     * - 0x0C addr_data_f1
     * - 0x10 addr_data_f2
     * - 0x14 addr_data_f3
     * - 0x18 status
     * - 0x1C delta_snapshot
     * - 0x20 delta_f2_f1
     * - 0x24 delta_f2_f0
     * - 0x28 nb_burst
     * - 0x2C nb_snapshot
     *
     */

    reset_wfp_burst_enable();
    reset_wfp_status();
    // set buffer addresses
    waveform_picker_regs->addr_data_f0 = (int) (wf_snap_f0);
    waveform_picker_regs->addr_data_f1 = current_ring_node_f1->buffer_address;
    waveform_picker_regs->addr_data_f2 = current_ring_node_f2->buffer_address;
    waveform_picker_regs->addr_data_f3 = (int) (wf_cont_f3_a);
    // set other parameters
    set_wfp_data_shaping();
    set_wfp_delta_snapshot();                           // time in seconds between two snapshots
    waveform_picker_regs->delta_f2_f1 = 0xffff;         // 0x16800 => 92160 (max 4 bytes)
    waveform_picker_regs->delta_f2_f0 = 0x17c00;        // 97 280 (max 5 bytes)
//    waveform_picker_regs->nb_burst_available = 0x180;   // max 3 bytes, size of the buffer in burst (1 burst = 16 x 4 octets)
//                                                        // 3 * 2048 / 16 = 384
//    waveform_picker_regs->nb_snapshot_param = 0x7ff;    // max 3 octets, 2048 - 1
    waveform_picker_regs->nb_burst_available = 0x1b9;   // max 3 bytes, size of the buffer in burst (1 burst = 16 x 4 octets)
                                                        // 3 * 2352 / 16 = 441
    waveform_picker_regs->nb_snapshot_param = 0x944;    // max 3 octets, 2372 - 1
}
#endif

//*****************
// local parameters
void set_local_nb_interrupt_f0_MAX( void )
{
    /** This function sets the value of the nb_interrupt_f0_MAX local parameter.
     *
     * This parameter is used for the SM validation only.\n
     * The software waits param_local.local_nb_interrupt_f0_MAX interruptions from the spectral matrices
     * module before launching a basic processing.
     *
     */

    param_local.local_nb_interrupt_f0_MAX = ( (parameter_dump_packet.sy_lfr_n_asm_p[0]) * 256
            + parameter_dump_packet.sy_lfr_n_asm_p[1] ) * 100;
}

void increment_seq_counter_source_id( unsigned char *packet_sequence_control, unsigned int sid )
{
    unsigned short *sequence_cnt;
    unsigned short segmentation_grouping_flag;
    unsigned short new_packet_sequence_control;

    if ( (sid ==SID_NORM_SWF_F0)    || (sid ==SID_NORM_SWF_F1) || (sid ==SID_NORM_SWF_F2)
         || (sid ==SID_NORM_CWF_F3) || (sid==SID_NORM_CWF_LONG_F3) || (sid ==SID_BURST_CWF_F2) )
    {
        sequence_cnt = &sequenceCounters_SCIENCE_NORMAL_BURST;
    }
    else if ( (sid ==SID_SBM1_CWF_F1) || (sid ==SID_SBM2_CWF_F2) )
    {
        sequence_cnt = &sequenceCounters_SCIENCE_SBM1_SBM2;
    }
    else
    {
        sequence_cnt = NULL;
        PRINTF1("in increment_seq_counter_source_id *** ERR apid_destid %d not known\n", sid)
    }

    if (sequence_cnt != NULL)
    {
        segmentation_grouping_flag  = (packet_sequence_control[ 0 ] & 0xc0) << 8;
        *sequence_cnt                = (*sequence_cnt) & 0x3fff;

        new_packet_sequence_control = segmentation_grouping_flag | *sequence_cnt ;

        packet_sequence_control[0] = (unsigned char) (new_packet_sequence_control >> 8);
        packet_sequence_control[1] = (unsigned char) (new_packet_sequence_control     );

        // increment the sequence counter for the next packet
        if ( *sequence_cnt < SEQ_CNT_MAX)
        {
            *sequence_cnt = *sequence_cnt + 1;
        }
        else
        {
            *sequence_cnt = 0;
        }
    }
}
