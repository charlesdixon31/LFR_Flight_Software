#ifndef FSW_SPACEWIRE_H_INCLUDED
#define FSW_SPACEWIRE_H_INCLUDED

#include <rtems.h>

#include <grspw.h>

#include <errno.h>
#include <fcntl.h> // for  O_RDWR
#include <sys/ioctl.h> // for the ioctl call
#include <unistd.h> // for the read call

#include "fsw_init.h"
#include "fsw_params.h"
#include "tc_tm/tc_handler.h"

#define SPW_LINK_OK       5
#define CONF_TCODE_CTRL   0x0909 // [Time Rx : Time Tx : Link error : Tick-out IRQ]
#define SPW_BIT_NP        0x00100000 // [NP] set the No port force bit
#define SPW_BIT_NP_MASK   0xffdfffff
#define SPW_BIT_RE        0x00010000 // [RE] set the RMAP Enable bit
#define SPW_BIT_RE_MASK   0xfffdffff
#define SPW_LINK_STAT_POS 21
#define SPW_TIMECODE_MAX  63

extern spw_stats grspw_stats;
extern rtems_name timecode_timer_name;
extern rtems_id timecode_timer_id;
extern unsigned char oneTcLfrUpdateTimeReceived;

// RTEMS TASK
rtems_task spiq_task(rtems_task_argument argument);
rtems_task recv_task(rtems_task_argument unused);
rtems_task send_task(rtems_task_argument argument);
rtems_task link_task(rtems_task_argument argument);

int spacewire_open_link(void);
int spacewire_start_link(int fd);
int spacewire_stop_and_start_link(int fd);
int spacewire_configure_link(int fd);
int spacewire_several_connect_attemps(void);
void spacewire_set_NP(unsigned char val, unsigned int regAddr); // No Port force
void spacewire_set_RE(unsigned char val, unsigned int regAddr); // RMAP Enable
void spacewire_read_statistics(void);
void spacewire_get_last_error(void);
void update_hk_lfr_last_er_fields(unsigned int rid, unsigned char code);
void update_hk_with_grspw_stats(void);
void spacewire_update_hk_lfr_link_state(unsigned char* hk_lfr_status_word_0);

void init_header_cwf(Header_TM_LFR_SCIENCE_CWF_t* header);
void init_header_swf(Header_TM_LFR_SCIENCE_SWF_t* header);
void init_header_asm(Header_TM_LFR_SCIENCE_ASM_t* header);
int spw_send_waveform_CWF(ring_node* ring_node_to_send, Header_TM_LFR_SCIENCE_CWF_t* header);
int spw_send_waveform_SWF(ring_node* ring_node_to_send, Header_TM_LFR_SCIENCE_SWF_t* header);
int spw_send_waveform_CWF3_light(ring_node* ring_node_to_send, Header_TM_LFR_SCIENCE_CWF_t* header);
void spw_send_asm_f0(ring_node* ring_node_to_send, Header_TM_LFR_SCIENCE_ASM_t* header);
void spw_send_asm_f1(ring_node* ring_node_to_send, Header_TM_LFR_SCIENCE_ASM_t* header);
void spw_send_asm_f2(ring_node* ring_node_to_send, Header_TM_LFR_SCIENCE_ASM_t* header);
void spw_send_k_dump(ring_node* ring_node_to_send);

unsigned int check_timecode_and_previous_timecode_coherency(unsigned char currentTimecodeCtr);
unsigned int check_timecode_and_internal_time_coherency(
    unsigned char timecode, unsigned char internalTime);
void timecode_irq_handler(void* pDev, void* regs, int minor, unsigned int tc);
rtems_timer_service_routine timecode_timer_routine(rtems_id timer_id, void* user_data);

void (*grspw_timecode_callback)(void* pDev, void* regs, int minor, unsigned int tc);

#endif // FSW_SPACEWIRE_H_INCLUDED
