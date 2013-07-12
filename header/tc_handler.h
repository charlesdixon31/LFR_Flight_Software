#ifndef TC_HANDLER_H_INCLUDED
#define TC_HANDLER_H_INCLUDED

#include <rtems.h>
#include <bsp.h>        // for the LEON_Unmask_interrupt function
#include <stdio.h>
#include <unistd.h>     // for the read call
#include <sys/ioctl.h>  // for the ioctl call
#include <ccsds_types.h>
#include <grspw.h>
#include <fsw_init.h>

extern int fdSPW;
extern rtems_name misc_name[ ];
extern rtems_name misc_id[ ];
extern rtems_id Task_id[ ];         // array of task ids
// MODE PARAMETERS
extern struct param_sbm1_str param_sbm1;
extern struct param_sbm2_str param_sbm2;
extern Packet_TM_LFR_PARAMETER_DUMP_t parameter_dump_packet;
extern Packet_TM_LFR_HK_t housekeeping_packet;
extern time_management_regs_t *time_management_regs;
extern waveform_picker_regs_t *waveform_picker_regs;
extern gptimer_regs_t         *gptimer_regs;

//****
// ISR
rtems_isr commutation_isr1( rtems_vector_number vector );
rtems_isr commutation_isr2( rtems_vector_number vector );

//**********************
// GENERAL USE FUNCTIONS
void initLookUpTableForCRC( void );
void GetCRCAsTwoBytes(unsigned char* data, unsigned char* crcAsTwoBytes, unsigned int sizeOfData);

//*********************
// ACCEPTANCE FUNCTIONS
int TC_acceptance(ccsdsTelecommandPacket_t *TC, unsigned int TC_LEN_RCV);
unsigned char TC_parser(ccsdsTelecommandPacket_t * TMPacket, unsigned int TC_LEN_RCV);

unsigned char TM_build_header( enum TM_TYPE tm_type, unsigned int packetLength,
                              TMHeader_t *TMHeader, unsigned char tc_sid);
unsigned char TM_build_data(ccsdsTelecommandPacket_t *TC, char* data, unsigned int SID, unsigned char *computed_CRC);

//***********
// RTEMS TASK
rtems_task recv_task( rtems_task_argument unused );
rtems_task actn_task( rtems_task_argument unused );
rtems_task dumb_task( rtems_task_argument unused );
int create_message_queue( void );

//***********
// TC ACTIONS
int action_default(ccsdsTelecommandPacket_t *TC);
int action_enter(ccsdsTelecommandPacket_t *TC);
int action_updt_info(ccsdsTelecommandPacket_t *TC);
int action_enable_calibration(ccsdsTelecommandPacket_t *TC);
int action_disable_calibration(ccsdsTelecommandPacket_t *TC);
int action_updt_time(ccsdsTelecommandPacket_t *TC);
// mode transition
int transition_validation(unsigned char requestedMode);
int stop_current_mode();
int enter_mode(unsigned char mode, ccsdsTelecommandPacket_t *TC);
int enter_standby_mode(ccsdsTelecommandPacket_t *TC);
int enter_normal_mode(ccsdsTelecommandPacket_t *TC);
int enter_burst_mode(ccsdsTelecommandPacket_t *TC);
int enter_sbm1_mode(ccsdsTelecommandPacket_t *TC);
int enter_sbm2_mode(ccsdsTelecommandPacket_t *TC);
// parameters loading
int action_load_comm(ccsdsTelecommandPacket_t *TC);
int action_load_norm(ccsdsTelecommandPacket_t *TC);
int action_load_burst(ccsdsTelecommandPacket_t *TC);
int action_load_sbm1(ccsdsTelecommandPacket_t *TC);
int action_load_sbm2(ccsdsTelecommandPacket_t *TC);
int action_dump(ccsdsTelecommandPacket_t *TC);
// other functions
void update_last_TC_exe(ccsdsTelecommandPacket_t *TC);
void update_last_TC_rej(ccsdsTelecommandPacket_t *TC);
void close_action(ccsdsTelecommandPacket_t *TC, int result);
int send_tm_lfr_tc_exe_success(ccsdsTelecommandPacket_t *TC);
int send_tm_lfr_tc_exe_not_executable(ccsdsTelecommandPacket_t *TC);
int send_tm_lfr_tc_exe_not_implemented(ccsdsTelecommandPacket_t *TC);
int send_tm_lfr_tc_exe_error(ccsdsTelecommandPacket_t *TC);

#endif // TC_HANDLER_H_INCLUDED



