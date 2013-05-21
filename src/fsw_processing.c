#include <fsw_processing.h>
#include <math.h>

#include <fsw_processing_globals.c>

//***********************************************************
// Interrupt Service Routine for spectral matrices processing
rtems_isr spectral_matrices_isr( rtems_vector_number vector )
{
    if (rtems_event_send( Task_id[TASKID_SMIQ], RTEMS_EVENT_0 ) != RTEMS_SUCCESSFUL) {
        printf("in spectral_matrices_isr *** Error sending event to AVF0\n");
    }
}

//************
// RTEMS TASKS
rtems_task smiq_task(rtems_task_argument argument) // process the Spectral Matrices IRQ
{
    rtems_event_set event_out;
    unsigned char nb_interrupt_f0 = 0;

    PRINTF("in SMIQ *** \n")

    while(1){
        rtems_event_receive(RTEMS_EVENT_0, RTEMS_WAIT, RTEMS_NO_TIMEOUT, &event_out); // wait for an RTEMS_EVENT0
        nb_interrupt_f0 = nb_interrupt_f0 + 1;
        if (nb_interrupt_f0 == (NB_SM_TO_RECEIVE_BEFORE_AVF0-1) ){
            if (rtems_event_send( Task_id[TASKID_AVF0], RTEMS_EVENT_0 ) != RTEMS_SUCCESSFUL)
                printf("in SMIQ *** Error sending event to AVF0\n");
            nb_interrupt_f0 = 0;
        }
    }
}

rtems_task spw_bppr_task(rtems_task_argument argument)
{
    rtems_status_code status;
    rtems_event_set event_out;
    static int Nb_average_f0 = 0;
    //static int nb_average_f1 = 0;
    //static int nb_average_f2 = 0;

    spectral_matrices_regs = (struct spectral_matrices_regs_str *) REGS_ADDR_SPECTRAL_MATRICES;
    spectral_matrices_regs->address0 = (volatile int) spec_mat_f0_a;
    spectral_matrices_regs->address1 = (volatile int) spec_mat_f0_b;

    printf("in BPPR ***\n");

    while(true){ // wait for an event to begin with the processing
        status = rtems_event_receive(RTEMS_EVENT_0, RTEMS_WAIT, RTEMS_NO_TIMEOUT, &event_out);
        if (status == RTEMS_SUCCESSFUL) {
            if ((spectral_matrices_regs->ctrl & 0x00000001)==1) {
                matrix_average(spec_mat_f0_a, averaged_spec_mat_f0);
                spectral_matrices_regs->ctrl = spectral_matrices_regs->ctrl & 0xfffffffe;
                //printf("f0_a\n");
                Nb_average_f0++;
            }
            if (((spectral_matrices_regs->ctrl>>1) & 0x00000001)==1) {
                matrix_average(spec_mat_f0_b, compressed_spec_mat_f0);
                spectral_matrices_regs->ctrl = spectral_matrices_regs->ctrl & 0xfffffffd;
                //printf("f0_b\n");
                Nb_average_f0++;
            }
            if (Nb_average_f0 == NB_AVERAGE_NORMAL_f0) {
                    matrix_compression(averaged_spec_mat_f0, 0, compressed_spec_mat_f0);
                    //printf("f0 compressed\n");
                    Nb_average_f0 = 0;
                    matrix_reset(averaged_spec_mat_f0);
            }
        }
    }
}

rtems_task avf0_task(rtems_task_argument argument){
    int i;
    static int nb_average;
    rtems_event_set event_out;
    rtems_status_code status;

    spectral_matrices_regs = (struct spectral_matrices_regs_str *) REGS_ADDR_SPECTRAL_MATRICES;
    spectral_matrices_regs->address0 = (volatile int) spec_mat_f0_a;
    spectral_matrices_regs->address1 = (volatile int) spec_mat_f0_b;

    nb_average = 0;

    PRINTF("in AVFO *** \n")

    while(1){
        rtems_event_receive(RTEMS_EVENT_0, RTEMS_WAIT, RTEMS_NO_TIMEOUT, &event_out); // wait for an RTEMS_EVENT0
        for(i=0; i<TOTAL_SIZE_SPEC_MAT; i++){
            averaged_spec_mat_f0[i] = averaged_spec_mat_f0[i] + spec_mat_f0_a[i]
                                            + spec_mat_f0_b[i]
                                            + spec_mat_f0_c[i]
                                            + spec_mat_f0_d[i]
                                            + spec_mat_f0_e[i]
                                            + spec_mat_f0_f[i]
                                            + spec_mat_f0_g[i]
                                            + spec_mat_f0_h[i];
        }
        spectral_matrices_regs->ctrl = spectral_matrices_regs->ctrl & 0xfffffffe; // reset the appropriate bit in the register
        nb_average = nb_average + NB_SM_TO_RECEIVE_BEFORE_AVF0;
        if (nb_average == NB_AVERAGE_NORMAL_f0) {
            nb_average = 0;
            status = rtems_event_send( Task_id[7], RTEMS_EVENT_0 ); // sending an event to the task 7, BPF0
            if (status != RTEMS_SUCCESSFUL) {
                printf("iN TASK AVF0 *** Error sending RTEMS_EVENT_0, code %d\n", status);
            }
        }
    }
}

rtems_task bpf0_task(rtems_task_argument argument){
    rtems_event_set event_out;

    PRINTF("in BPFO *** \n")

    while(1){
        rtems_event_receive(RTEMS_EVENT_0, RTEMS_WAIT, RTEMS_NO_TIMEOUT, &event_out); // wait for an RTEMS_EVENT0
        matrix_compression(averaged_spec_mat_f0, 0, compressed_spec_mat_f0);
        BP1_set(compressed_spec_mat_f0, NB_BINS_COMPRESSED_MATRIX_f0, LFR_BP1_F0);
        //PRINTF("IN TASK BPF0 *** Matrix compressed, parameters calculated\n")
    }
}

//*****************************
// Spectral matrices processing
void matrix_average(volatile int *spec_mat, float *averaged_spec_mat)
{
    int i;
    for(i=0; i<TOTAL_SIZE_SPEC_MAT; i++){
        averaged_spec_mat[i] = averaged_spec_mat[i] + spec_mat_f0_a[i]
                                        + spec_mat_f0_b[i]
                                        + spec_mat_f0_c[i]
                                        + spec_mat_f0_d[i]
                                        + spec_mat_f0_e[i]
                                        + spec_mat_f0_f[i]
                                        + spec_mat_f0_g[i]
                                        + spec_mat_f0_h[i];
    }
}

void matrix_reset(float *averaged_spec_mat)
{
    int i;
    for(i=0; i<TOTAL_SIZE_SPEC_MAT; i++){
        averaged_spec_mat_f0[i] = 0;
    }
}

void matrix_compression(float *averaged_spec_mat, unsigned char fChannel, float *compressed_spec_mat)
{
    int i;
    int j;
    switch (fChannel){
        case 0:
                for(i=0;i<NB_BINS_COMPRESSED_MATRIX_f0;i++){
                    j = 17 + (i * 8);
                    compressed_spec_mat[i] = (averaged_spec_mat[j]
                                                    + averaged_spec_mat[j+1]
                                                    + averaged_spec_mat[j+2]
                                                    + averaged_spec_mat[j+3]
                                                    + averaged_spec_mat[j+4]
                                                    + averaged_spec_mat[j+5]
                                                    + averaged_spec_mat[j+6]
                                                    + averaged_spec_mat[j+7])/(8*NB_AVERAGE_NORMAL_f0);
                }
            break;
        case 1:
            // case fChannel = f1 to be completed later
            break;
        case 2:
            // case fChannel = f1 to be completed later
            break;
        default:
            break;
    }
}

void BP1_set(float * compressed_spec_mat, unsigned char nb_bins_compressed_spec_mat, unsigned char * LFR_BP1){
    int i;
    int j;
    unsigned char tmp_u_char;
    unsigned char * pt_char = NULL;
    float PSDB, PSDE;
    float NVEC_V0;
    float NVEC_V1;
    float NVEC_V2;
    //float significand;
    //int exponent;
    float aux;
    float tr_SB_SB;
    float tmp;
    float sx_re;
    float sx_im;
    float nebx_re = 0;
    float nebx_im = 0;
    float ny = 0;
    float nz = 0;
    float bx_bx_star = 0;
    for(i=0; i<nb_bins_compressed_spec_mat; i++){
        //==============================================
        // BP1 PSD == B PAR_LFR_SC_BP1_PE_FL0 == 16 bits
        PSDB = compressed_spec_mat[i*30]      // S11
            + compressed_spec_mat[(i*30) + 10]    // S22
            + compressed_spec_mat[(i*30) + 18];   // S33
        //significand = frexp(PSDB, &exponent);
        pt_char = (unsigned char*) &PSDB;
        LFR_BP1[(i*9) + 2] = pt_char[0]; // bits 31 downto 24 of the float
        LFR_BP1[(i*9) + 3] = pt_char[1];  // bits 23 downto 16 of the float
        //==============================================
        // BP1 PSD == E PAR_LFR_SC_BP1_PB_FL0 == 16 bits
        PSDE = compressed_spec_mat[(i*30) + 24] * K44_pe     // S44
            + compressed_spec_mat[(i*30) + 28] * K55_pe      // S55
            + compressed_spec_mat[(i*30) + 26] * K45_pe_re   // S45
            - compressed_spec_mat[(i*30) + 27] * K45_pe_im;  // S45
        pt_char = (unsigned char*) &PSDE;
        LFR_BP1[(i*9) + 0] = pt_char[0]; // bits 31 downto 24 of the float
        LFR_BP1[(i*9) + 1] = pt_char[1]; // bits 23 downto 16 of the float
        //==============================================================================
        // BP1 normal wave vector == PAR_LFR_SC_BP1_NVEC_V0_F0 == 8 bits
                               // == PAR_LFR_SC_BP1_NVEC_V1_F0 == 8 bits
                               // == PAR_LFR_SC_BP1_NVEC_V2_F0 == 1 bits
        tmp = sqrt(
                    compressed_spec_mat[(i*30) + 3]*compressed_spec_mat[(i*30) + 3]     //Im S12
                    +compressed_spec_mat[(i*30) + 5]*compressed_spec_mat[(i*30) + 5]    //Im S13
                    +compressed_spec_mat[(i*30) + 13]*compressed_spec_mat[(i*30) + 13]   //Im S23
                    );
        NVEC_V0 = compressed_spec_mat[(i*30) + 13] / tmp;  // Im S23
        NVEC_V1 = -compressed_spec_mat[(i*30) + 5] / tmp;  // Im S13
        NVEC_V2 = compressed_spec_mat[(i*30) + 3] / tmp;   // Im S12
        LFR_BP1[(i*9) + 4] = (char) (NVEC_V0*127);
        LFR_BP1[(i*9) + 5] = (char) (NVEC_V1*127);
        pt_char = (unsigned char*) &NVEC_V2;
        LFR_BP1[(i*9) + 6] = pt_char[0] & 0x80;  // extract the sign of NVEC_V2
        //=======================================================
        // BP1 ellipticity == PAR_LFR_SC_BP1_ELLIP_F0   == 4 bits
        aux = 2*tmp / PSDB;                                             // compute the ellipticity
        tmp_u_char = (unsigned char) (aux*(16-1));                      // convert the ellipticity
        LFR_BP1[i*9+6] = LFR_BP1[i*9+6] | ((tmp_u_char&0x0f)<<3); // keeps 4 bits of the resulting unsigned char
        //==============================================================
        // BP1 degree of polarization == PAR_LFR_SC_BP1_DOP_F0 == 3 bits
        for(j = 0; j<NB_VALUES_PER_spec_mat;j++){
            tr_SB_SB = compressed_spec_mat[i*30] * compressed_spec_mat[i*30]
                    + compressed_spec_mat[(i*30) + 10] * compressed_spec_mat[(i*30) + 10]
                    + compressed_spec_mat[(i*30) + 18] * compressed_spec_mat[(i*30) + 18]
                    + 2 * compressed_spec_mat[(i*30) + 2] * compressed_spec_mat[(i*30) + 2]
                    + 2 * compressed_spec_mat[(i*30) + 3] * compressed_spec_mat[(i*30) + 3]
                    + 2 * compressed_spec_mat[(i*30) + 4] * compressed_spec_mat[(i*30) + 4]
                    + 2 * compressed_spec_mat[(i*30) + 5] * compressed_spec_mat[(i*30) + 5]
                    + 2 * compressed_spec_mat[(i*30) + 12] * compressed_spec_mat[(i*30) + 12]
                    + 2 * compressed_spec_mat[(i*30) + 13] * compressed_spec_mat[(i*30) + 13];
        }
        aux = PSDB*PSDB;
        tmp = sqrt( abs( ( 3*tr_SB_SB - aux ) / ( 2 * aux ) ) );
        tmp_u_char = (unsigned char) (NVEC_V0*(8-1));
        LFR_BP1[(i*9) + 6] = LFR_BP1[(i*9) + 6] | (tmp_u_char & 0x07); // keeps 3 bits of the resulting unsigned char
        //=======================================================================================
        // BP1 x-component of the normalized Poynting flux == PAR_LFR_SC_BP1_SZ_F0 == 8 bits (7+1)
        sx_re = compressed_spec_mat[(i*30) + 20] * K34_sx_re
                        + compressed_spec_mat[(i*30) + 6] * K14_sx_re
                        + compressed_spec_mat[(i*30) + 8] * K15_sx_re
                        + compressed_spec_mat[(i*30) + 14] * K24_sx_re
                        + compressed_spec_mat[(i*30) + 16] * K25_sx_re
                        + compressed_spec_mat[(i*30) + 22] * K35_sx_re;
        sx_im = compressed_spec_mat[(i*30) + 21] * K34_sx_im
                        + compressed_spec_mat[(i*30) + 7] * K14_sx_im
                        + compressed_spec_mat[(i*30) + 9] * K15_sx_im
                        + compressed_spec_mat[(i*30) + 15] * K24_sx_im
                        + compressed_spec_mat[(i*30) + 17] * K25_sx_im
                        + compressed_spec_mat[(i*30) + 23] * K35_sx_im;
        LFR_BP1[(i*9) + 7] = ((unsigned char) (sx_re * 128)) & 0x7f; // cf DOC for the compression
        if ( abs(sx_re) > abs(sx_im) ) {
            LFR_BP1[(i*9) + 7] = LFR_BP1[(i*9) + 1] | (0x80);  // extract the sector of sx
        }
        else {
            LFR_BP1[(i*9) + 7] = LFR_BP1[(i*9) + 1] & (0x7f);  // extract the sector of sx
        }
        //======================================================================
        // BP1 phase velocity estimator == PAR_LFR_SC_BP1_VPHI_F0 == 8 bits (7+1)
        ny = sin(Alpha_M)*NVEC_V1 + cos(Alpha_M)*NVEC_V2;
        nz = NVEC_V0;
        bx_bx_star = cos(Alpha_M) * cos(Alpha_M) * compressed_spec_mat[i*30+10]            // re S22
                        + sin(Alpha_M) * sin(Alpha_M) * compressed_spec_mat[i*30+18]       // re S33
                        - 2 * sin(Alpha_M) * cos(Alpha_M) * compressed_spec_mat[i*30+12];  // re S23
        nebx_re = ny * (compressed_spec_mat[(i*30) + 14] * K24_ny_re
                                        +compressed_spec_mat[(i*30) + 16] * K25_ny_re
                                        +compressed_spec_mat[(i*30) + 20] * K34_ny_re
                                        +compressed_spec_mat[(i*30) + 22] * K35_ny_re)
                                + nz * (compressed_spec_mat[(i*30) + 14] * K24_nz_re
                                        +compressed_spec_mat[(i*30) + 16] * K25_nz_re
                                        +compressed_spec_mat[(i*30) + 20] * K34_nz_re
                                        +compressed_spec_mat[(i*30) + 22] * K35_nz_re);
        nebx_im = ny * (compressed_spec_mat[(i*30) + 15]*K24_ny_re
                                        +compressed_spec_mat[(i*30) + 17] * K25_ny_re
                                        +compressed_spec_mat[(i*30) + 21] * K34_ny_re
                                        +compressed_spec_mat[(i*30) + 23] * K35_ny_re)
                                + nz * (compressed_spec_mat[(i*30) + 15] * K24_nz_im
                                        +compressed_spec_mat[(i*30) + 17] * K25_nz_im
                                        +compressed_spec_mat[(i*30) + 21] * K34_nz_im
                                        +compressed_spec_mat[(i*30) + 23] * K35_nz_im);
        tmp = nebx_re / bx_bx_star;
        LFR_BP1[(i*9) + 8] = ((unsigned char) (tmp * 128)) & 0x7f; // cf DOC for the compression
        if ( abs(nebx_re) > abs(nebx_im) ) {
            LFR_BP1[(i*9) + 8] = LFR_BP1[(i*9) + 8] | (0x80);  // extract the sector of nebx
        }
        else {
            LFR_BP1[(i*9) + 8] = LFR_BP1[(i*9) + 8] & (0x7f);  // extract the sector of nebx
        }
    }

}

void BP2_set(float * compressed_spec_mat, unsigned char nb_bins_compressed_spec_mat){
    // BP2 autocorrelation
    int i;
    int aux = 0;

    for(i = 0; i<nb_bins_compressed_spec_mat; i++){
        // S12
        aux = sqrt(compressed_spec_mat[i*30]*compressed_spec_mat[(i*30) + 10]);
        compressed_spec_mat[(i*30) + 2] = compressed_spec_mat[(i*30) + 2] / aux;
        compressed_spec_mat[(i*30) + 3] = compressed_spec_mat[(i*30) + 3] / aux;
        // S13
        aux = sqrt(compressed_spec_mat[i*30]*compressed_spec_mat[(i*30) + 18]);
        compressed_spec_mat[(i*30) + 4] = compressed_spec_mat[(i*30) + 4] / aux;
        compressed_spec_mat[(i*30) + 5] = compressed_spec_mat[(i*30) + 5] / aux;
        // S23
        aux = sqrt(compressed_spec_mat[i*30+12]*compressed_spec_mat[(i*30) + 18]);
        compressed_spec_mat[(i*30) + 12] = compressed_spec_mat[(i*30) + 12] / aux;
        compressed_spec_mat[(i*30) + 13] = compressed_spec_mat[(i*30) + 13] / aux;
        // S45
        aux = sqrt(compressed_spec_mat[i*30+24]*compressed_spec_mat[(i*30) + 28]);
        compressed_spec_mat[(i*30) + 26] = compressed_spec_mat[(i*30) + 26] / aux;
        compressed_spec_mat[(i*30) + 27] = compressed_spec_mat[(i*30) + 27] / aux;
        // S14
        aux = sqrt(compressed_spec_mat[i*30]*compressed_spec_mat[(i*30)  +24]);
        compressed_spec_mat[(i*30) + 6] = compressed_spec_mat[(i*30) + 6] / aux;
        compressed_spec_mat[(i*30) + 7] = compressed_spec_mat[(i*30) + 7] / aux;
        // S15
        aux = sqrt(compressed_spec_mat[i*30]*compressed_spec_mat[(i*30) + 28]);
        compressed_spec_mat[(i*30) + 8] = compressed_spec_mat[(i*30) + 8] / aux;
        compressed_spec_mat[(i*30) + 9] = compressed_spec_mat[(i*30) + 9] / aux;
        // S24
        aux = sqrt(compressed_spec_mat[i*10]*compressed_spec_mat[(i*30) + 24]);
        compressed_spec_mat[(i*30) + 14] = compressed_spec_mat[(i*30) + 14] / aux;
        compressed_spec_mat[(i*30) + 15] = compressed_spec_mat[(i*30) + 15] / aux;
        // S25
        aux = sqrt(compressed_spec_mat[i*10]*compressed_spec_mat[(i*30) + 28]);
        compressed_spec_mat[(i*30) + 16] = compressed_spec_mat[(i*30) + 16] / aux;
        compressed_spec_mat[(i*30) + 17] = compressed_spec_mat[(i*30) + 17] / aux;
        // S34
        aux = sqrt(compressed_spec_mat[i*18]*compressed_spec_mat[(i*30) + 24]);
        compressed_spec_mat[(i*30) + 20] = compressed_spec_mat[(i*30) + 20] / aux;
        compressed_spec_mat[(i*30) + 21] = compressed_spec_mat[(i*30) + 21] / aux;
        // S35
        aux = sqrt(compressed_spec_mat[i*18]*compressed_spec_mat[(i*30) + 28]);
        compressed_spec_mat[(i*30) + 22] = compressed_spec_mat[(i*30) + 22] / aux;
        compressed_spec_mat[(i*30) + 23] = compressed_spec_mat[(i*30) + 23] / aux;
    }
}

