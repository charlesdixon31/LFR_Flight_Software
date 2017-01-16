#ifndef FSW_PROCESSING_H_INCLUDED
#define FSW_PROCESSING_H_INCLUDED

#include <rtems.h>
#include <grspw.h>
#include <math.h>
#include <stdlib.h> // abs() is in the stdlib
#include <stdio.h>
#include <math.h>
#include <grlib_regs.h>

#include "fsw_params.h"

#define SBM_COEFF_PER_NORM_COEFF    2
#define MAX_SRC_DATA                780     // MAX size is 26 bins * 30 Bytes [TM_LFR_SCIENCE_BURST_BP2_F1]
#define MAX_SRC_DATA_WITH_SPARE     143     // 13 bins  * 11 Bytes

#define NODE_0  0
#define NODE_1  1
#define NODE_2  2
#define NODE_3  3
#define NODE_4  4
#define NODE_5  5
#define NODE_6  6
#define NODE_7  7

typedef struct ring_node_asm
{
    struct ring_node_asm *next;
    float  matrix[ TOTAL_SIZE_SM ];
    unsigned int status;
} ring_node_asm;

typedef struct
{
    unsigned char targetLogicalAddress;
    unsigned char protocolIdentifier;
    unsigned char reserved;
    unsigned char userApplication;
    unsigned char packetID[BYTES_PER_PACKETID];
    unsigned char packetSequenceControl[BYTES_PER_SEQ_CTRL];
    unsigned char packetLength[BYTES_PER_PKT_LEN];
    // DATA FIELD HEADER
    unsigned char spare1_pusVersion_spare2;
    unsigned char serviceType;
    unsigned char serviceSubType;
    unsigned char destinationID;
    unsigned char time[BYTES_PER_TIME];
    // AUXILIARY HEADER
    unsigned char sid;
    unsigned char pa_bia_status_info;
    unsigned char sy_lfr_common_parameters_spare;
    unsigned char sy_lfr_common_parameters;
    unsigned char acquisitionTime[BYTES_PER_TIME];
    unsigned char pa_lfr_bp_blk_nr[BYTES_PER_BLKNR];
    // SOURCE DATA
    unsigned char data[ MAX_SRC_DATA ];   // MAX size is 26 bins * 30 Bytes [TM_LFR_SCIENCE_BURST_BP2_F1]
} bp_packet;

typedef struct
{
    unsigned char targetLogicalAddress;
    unsigned char protocolIdentifier;
    unsigned char reserved;
    unsigned char userApplication;
    unsigned char packetID[BYTES_PER_PACKETID];
    unsigned char packetSequenceControl[BYTES_PER_SEQ_CTRL];
    unsigned char packetLength[BYTES_PER_PKT_LEN];
    // DATA FIELD HEADER
    unsigned char spare1_pusVersion_spare2;
    unsigned char serviceType;
    unsigned char serviceSubType;
    unsigned char destinationID;
    unsigned char time[BYTES_PER_TIME];
    // AUXILIARY HEADER
    unsigned char sid;
    unsigned char pa_bia_status_info;
    unsigned char sy_lfr_common_parameters_spare;
    unsigned char sy_lfr_common_parameters;
    unsigned char acquisitionTime[BYTES_PER_TIME];
    unsigned char source_data_spare;
    unsigned char pa_lfr_bp_blk_nr[BYTES_PER_BLKNR];
    // SOURCE DATA
    unsigned char data[ MAX_SRC_DATA_WITH_SPARE ];   // 13 bins  * 11 Bytes
} bp_packet_with_spare; // only for TM_LFR_SCIENCE_NORMAL_BP1_F0 and F1

typedef struct asm_msg
{
    ring_node_asm *norm;
    ring_node_asm *burst_sbm;
    rtems_event_set event;
    unsigned int coarseTimeNORM;
    unsigned int fineTimeNORM;
    unsigned int coarseTimeSBM;
    unsigned int fineTimeSBM;
    unsigned int numberOfSMInASMNORM;
    unsigned int numberOfSMInASMSBM;
} asm_msg;

extern unsigned char thisIsAnASMRestart;

extern volatile int sm_f0[ ];
extern volatile int sm_f1[ ];
extern volatile int sm_f2[ ];
extern unsigned int acquisitionDurations[];

// parameters
extern struct param_local_str param_local;
extern Packet_TM_LFR_PARAMETER_DUMP_t parameter_dump_packet;

// registers
extern time_management_regs_t *time_management_regs;
extern volatile spectral_matrix_regs_t *spectral_matrix_regs;

extern rtems_name  misc_name[];
extern rtems_id    Task_id[];         /* array of task ids */

ring_node * getRingNodeForAveraging( unsigned char frequencyChannel);
// ISR
rtems_isr spectral_matrices_isr( rtems_vector_number vector );

//******************
// Spectral Matrices
void reset_nb_sm( void );
// SM
void SM_init_rings( void );
void SM_reset_current_ring_nodes( void );
// ASM
void ASM_generic_init_ring(ring_node_asm *ring, unsigned char nbNodes );

//*****************
// Basic Parameters

void BP_reset_current_ring_nodes( void );
void BP_init_header(bp_packet *packet,
                    unsigned int apid, unsigned char sid,
                    unsigned int packetLength , unsigned char blkNr);
void BP_init_header_with_spare(bp_packet_with_spare *packet,
                               unsigned int apid, unsigned char sid,
                               unsigned int packetLength, unsigned char blkNr );
void BP_send( char *data,
              rtems_id queue_id,
              unsigned int nbBytesToSend , unsigned int sid );
void BP_send_s1_s2(char *data,
                   rtems_id queue_id,
                   unsigned int nbBytesToSend, unsigned int sid );

//******************
// general functions
void reset_sm_status( void );
void reset_spectral_matrix_regs( void );
void set_time(unsigned char *time, unsigned char *timeInBuffer );
unsigned long long int get_acquisition_time( unsigned char *timePtr );
unsigned char getSID( rtems_event_set event );

extern rtems_status_code get_message_queue_id_prc1( rtems_id *queue_id );
extern rtems_status_code get_message_queue_id_prc2( rtems_id *queue_id );

//***************************************
// DEFINITIONS OF STATIC INLINE FUNCTIONS
static inline void SM_average(float *averaged_spec_mat_NORM, float *averaged_spec_mat_SBM,
                              ring_node *ring_node_tab[],
                              unsigned int nbAverageNORM, unsigned int nbAverageSBM,
                              asm_msg *msgForMATR , unsigned char channel);

void ASM_patch( float *inputASM, float *outputASM );

void extractReImVectors(float *inputASM, float *outputASM, unsigned int asmComponent );

static inline void ASM_reorganize_and_divide(float *averaged_spec_mat, float *averaged_spec_mat_reorganized,
                                             float divider );

static inline void ASM_compress_reorganize_and_divide(float *averaged_spec_mat, float *compressed_spec_mat,
                                                      float divider,
                                                      unsigned char nbBinsCompressedMatrix, unsigned char nbBinsToAverage , unsigned char ASMIndexStart);

static inline void ASM_convert(volatile float *input_matrix, char *output_matrix);

unsigned char acquisitionTimeIsValid(unsigned int coarseTime, unsigned int fineTime, unsigned char channel);

void SM_average( float *averaged_spec_mat_NORM, float *averaged_spec_mat_SBM,
                 ring_node *ring_node_tab[],
                 unsigned int nbAverageNORM, unsigned int nbAverageSBM,
                 asm_msg *msgForMATR, unsigned char channel )
{
    float sum;
    unsigned int i;
    unsigned int k;
    unsigned char incomingSMIsValid[NB_SM_BEFORE_AVF0_F1];
    unsigned int numberOfValidSM;
    unsigned char isValid;

    //**************
    // PAS FILTERING
    // check acquisitionTime of the incoming data
    numberOfValidSM = 0;
    for (k=0; k<NB_SM_BEFORE_AVF0_F1; k++)
    {
        isValid = acquisitionTimeIsValid( ring_node_tab[k]->coarseTime, ring_node_tab[k]->fineTime, channel );
        incomingSMIsValid[k] = isValid;
        numberOfValidSM = numberOfValidSM + isValid;
    }

    //************************
    // AVERAGE SPECTRAL MATRIX
    for(i=0; i<TOTAL_SIZE_SM; i++)
    {
//        sum = ( (int *) (ring_node_tab[0]->buffer_address) ) [ i ]
//                + ( (int *) (ring_node_tab[1]->buffer_address) ) [ i ]
//                + ( (int *) (ring_node_tab[2]->buffer_address) ) [ i ]
//                + ( (int *) (ring_node_tab[3]->buffer_address) ) [ i ]
//                + ( (int *) (ring_node_tab[4]->buffer_address) ) [ i ]
//                + ( (int *) (ring_node_tab[5]->buffer_address) ) [ i ]
//                + ( (int *) (ring_node_tab[6]->buffer_address) ) [ i ]
//                + ( (int *) (ring_node_tab[7]->buffer_address) ) [ i ];

        sum = ( incomingSMIsValid[BYTE_0] * ((int *)(ring_node_tab[NODE_0]->buffer_address) )[ i ] )
                + ( incomingSMIsValid[BYTE_1] * ((int *)(ring_node_tab[NODE_1]->buffer_address) )[ i ] )
                + ( incomingSMIsValid[BYTE_2] * ((int *)(ring_node_tab[NODE_2]->buffer_address) )[ i ] )
                + ( incomingSMIsValid[BYTE_3] * ((int *)(ring_node_tab[NODE_3]->buffer_address) )[ i ] )
                + ( incomingSMIsValid[BYTE_4] * ((int *)(ring_node_tab[NODE_4]->buffer_address) )[ i ] )
                + ( incomingSMIsValid[BYTE_5] * ((int *)(ring_node_tab[NODE_5]->buffer_address) )[ i ] )
                + ( incomingSMIsValid[BYTE_6] * ((int *)(ring_node_tab[NODE_6]->buffer_address) )[ i ] )
                + ( incomingSMIsValid[BYTE_7] * ((int *)(ring_node_tab[NODE_7]->buffer_address) )[ i ] );

        if ( (nbAverageNORM == 0) && (nbAverageSBM == 0) )
        {
            averaged_spec_mat_NORM[ i ] = sum;
            averaged_spec_mat_SBM[  i ] = sum;
            msgForMATR->coarseTimeNORM  = ring_node_tab[0]->coarseTime;
            msgForMATR->fineTimeNORM    = ring_node_tab[0]->fineTime;
            msgForMATR->coarseTimeSBM   = ring_node_tab[0]->coarseTime;
            msgForMATR->fineTimeSBM     = ring_node_tab[0]->fineTime;
        }
        else if ( (nbAverageNORM != 0) && (nbAverageSBM != 0) )
        {
            averaged_spec_mat_NORM[ i ] = ( averaged_spec_mat_NORM[  i ] + sum );
            averaged_spec_mat_SBM[  i ] = ( averaged_spec_mat_SBM[   i ] + sum );
        }
        else if ( (nbAverageNORM != 0) && (nbAverageSBM == 0) )
        {
            averaged_spec_mat_NORM[ i ] = ( averaged_spec_mat_NORM[ i ] + sum );
            averaged_spec_mat_SBM[  i ] = sum;
            msgForMATR->coarseTimeSBM   = ring_node_tab[0]->coarseTime;
            msgForMATR->fineTimeSBM     = ring_node_tab[0]->fineTime;
        }
        else
        {
            averaged_spec_mat_NORM[ i ] = sum;
            averaged_spec_mat_SBM[  i ] = ( averaged_spec_mat_SBM[   i ] + sum );
            msgForMATR->coarseTimeNORM   = ring_node_tab[0]->coarseTime;
            msgForMATR->fineTimeNORM     = ring_node_tab[0]->fineTime;
            //            PRINTF2("ERR *** in SM_average *** unexpected parameters %d %d\n", nbAverageNORM, nbAverageSBM)
        }
    }

    //*******************
    // UPDATE SM COUNTERS
    if ( (nbAverageNORM == 0) && (nbAverageSBM == 0) )
    {
        msgForMATR->numberOfSMInASMNORM = numberOfValidSM;
        msgForMATR->numberOfSMInASMSBM  = numberOfValidSM;
    }
    else if ( (nbAverageNORM != 0) && (nbAverageSBM != 0) )
    {
        msgForMATR->numberOfSMInASMNORM = msgForMATR->numberOfSMInASMNORM   + numberOfValidSM;
        msgForMATR->numberOfSMInASMSBM  = msgForMATR->numberOfSMInASMSBM    + numberOfValidSM;
    }
    else if ( (nbAverageNORM != 0) && (nbAverageSBM == 0) )
    {
        msgForMATR->numberOfSMInASMNORM = msgForMATR->numberOfSMInASMNORM   + numberOfValidSM;
        msgForMATR->numberOfSMInASMSBM  = numberOfValidSM;
    }
    else
    {
        msgForMATR->numberOfSMInASMNORM = numberOfValidSM;
        msgForMATR->numberOfSMInASMSBM  = msgForMATR->numberOfSMInASMSBM    + numberOfValidSM;
    }
}

void ASM_reorganize_and_divide( float *averaged_spec_mat, float *averaged_spec_mat_reorganized, float divider )
{
    int frequencyBin;
    int asmComponent;
    unsigned int offsetASM;
    unsigned int offsetASMReorganized;

    // BUILD DATA
    for (asmComponent = 0; asmComponent < NB_VALUES_PER_SM; asmComponent++)
    {
        for( frequencyBin = 0; frequencyBin < NB_BINS_PER_SM; frequencyBin++ )
        {
            offsetASMReorganized =
                    (frequencyBin * NB_VALUES_PER_SM)
                    + asmComponent;
            offsetASM            =
                    (asmComponent * NB_BINS_PER_SM)
                    + frequencyBin;
            if ( divider != INIT_FLOAT )
            {
                averaged_spec_mat_reorganized[offsetASMReorganized  ] = averaged_spec_mat[ offsetASM ] / divider;
            }
            else
            {
                averaged_spec_mat_reorganized[offsetASMReorganized  ] = INIT_FLOAT;
            }
        }
    }
}

void ASM_compress_reorganize_and_divide(float *averaged_spec_mat, float *compressed_spec_mat , float divider,
                                        unsigned char nbBinsCompressedMatrix, unsigned char nbBinsToAverage, unsigned char ASMIndexStart )
{
    int frequencyBin;
    int asmComponent;
    int offsetASM;
    int offsetCompressed;
    int k;

    // BUILD DATA
    for (asmComponent = 0; asmComponent < NB_VALUES_PER_SM; asmComponent++)
    {
        for( frequencyBin = 0; frequencyBin < nbBinsCompressedMatrix; frequencyBin++ )
        {
            offsetCompressed =  // NO TIME OFFSET
                    (frequencyBin * NB_VALUES_PER_SM)
                    + asmComponent;
            offsetASM =         // NO TIME OFFSET
                    (asmComponent * NB_BINS_PER_SM)
                    + ASMIndexStart
                    + (frequencyBin * nbBinsToAverage);
            compressed_spec_mat[ offsetCompressed ] = 0;
            for ( k = 0; k < nbBinsToAverage; k++ )
            {
                compressed_spec_mat[offsetCompressed ] =
                        ( compressed_spec_mat[ offsetCompressed ]
                          + averaged_spec_mat[ offsetASM + k ] );
            }
            compressed_spec_mat[ offsetCompressed ] =
                    compressed_spec_mat[ offsetCompressed ] / (divider * nbBinsToAverage);
        }
    }
}

void ASM_convert( volatile float *input_matrix, char *output_matrix)
{
    unsigned int frequencyBin;
    unsigned int asmComponent;
    char * pt_char_input;
    char * pt_char_output;
    unsigned int offsetInput;
    unsigned int offsetOutput;

    pt_char_input = (char*) &input_matrix;
    pt_char_output = (char*) &output_matrix;

    // convert all other data
    for( frequencyBin=0; frequencyBin<NB_BINS_PER_SM; frequencyBin++)
    {
        for ( asmComponent=0; asmComponent<NB_VALUES_PER_SM; asmComponent++)
        {
            offsetInput  =                      (frequencyBin*NB_VALUES_PER_SM) + asmComponent   ;
            offsetOutput = SM_BYTES_PER_VAL * ( (frequencyBin*NB_VALUES_PER_SM) + asmComponent ) ;
            pt_char_input =  (char*) &input_matrix [ offsetInput  ];
            pt_char_output = (char*) &output_matrix[ offsetOutput ];
            pt_char_output[0] = pt_char_input[0];   // bits 31 downto 24 of the float
            pt_char_output[1] = pt_char_input[1];   // bits 23 downto 16 of the float
        }
    }
}

void ASM_compress_reorganize_and_divide_mask(float *averaged_spec_mat, float *compressed_spec_mat,
                                             float divider,
                                             unsigned char nbBinsCompressedMatrix, unsigned char nbBinsToAverage , unsigned char ASMIndexStart, unsigned char channel);

int getFBinMask(int k, unsigned char channel);

void init_kcoeff_sbm_from_kcoeff_norm( float *input_kcoeff, float *output_kcoeff, unsigned char nb_bins_norm);

#endif // FSW_PROCESSING_H_INCLUDED
