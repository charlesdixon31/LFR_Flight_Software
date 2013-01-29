#ifndef FSW_RTEMS_PROCESSING_H_INCLUDED
#define FSW_RTEMS_PROCESSING_H_INCLUDED

#define NB_BINS_spec_mat 128
#define NB_VALUES_PER_spec_mat 25
#define TOTAL_SIZE_SPEC_MAT NB_BINS_spec_mat * NB_VALUES_PER_spec_mat
#define NB_BINS_COMPRESSED_MATRIX_f0 11
#define SIZE_COMPRESSED_spec_mat_f1 13
#define SIZE_COMPRESSED_spec_mat_f2 12
#define TOTAL_SIZE_COMPRESSED_MATRIX_f0 NB_BINS_COMPRESSED_MATRIX_f0 * NB_VALUES_PER_spec_mat
#define NB_AVERAGE_NORMAL_f0 96*4
#define NB_SM_TO_RECEIVE_BEFORE_AVF0 8

#include <rtems.h>
#include <grlib_regs.h>
#include <fsw_params.h>

struct BP1_str{
    volatile unsigned char PE[2];
    volatile unsigned char PB[2];
    volatile unsigned char V0;
    volatile unsigned char V1;
    volatile unsigned char V2_ELLIP_DOP;
    volatile unsigned char SZ;
    volatile unsigned char VPHI;
};
typedef struct BP1_str BP1_t;

// ISR
rtems_isr spectral_matrices_isr( rtems_vector_number vector );
// RTEMS TASKS
rtems_task spw_bppr_task(rtems_task_argument argument);
rtems_task spw_avf0_task(rtems_task_argument argument);
rtems_task spw_bpf0_task(rtems_task_argument argument);
rtems_task spw_smiq_task(rtems_task_argument argument); // added to test the spectral matrix simulator
//
rtems_task spw_bppr_task_rate_monotonic(rtems_task_argument argument);
void matrix_average(volatile int *spec_mat, float *averaged_spec_mat);
void matrix_compression(float *averaged_spec_mat, unsigned char fChannel, float *compressed_spec_mat);
void matrix_reset(float *averaged_spec_mat);
void BP1_set(float * compressed_spec_mat, unsigned char nb_bins_compressed_spec_mat, unsigned char * LFR_BP1);
void BP2_set(float * compressed_spec_mat, unsigned char nb_bins_compressed_spec_mat);

#endif // FSW_RTEMS_PROCESSING_H_INCLUDED
