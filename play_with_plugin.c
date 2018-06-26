#include <string.h>
#include <sched.h>
#include <errno.h>
#include <getopt.h>
#include <alsa/asoundlib.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>
#include <ladspa.h>
#include <dlfcn.h>
#include "utils.h"
#include "createUI.c"



#define SAMPLE_RATE 22050

#define AMPLIFIER 0
#define DELAY 1
#define REVERB 2
#define DISTORTION 3
#define RESET 5
#define CHORUS 4
#define MAX_NAME_LENGTH  20
#define NO_EFFECTS 5
// the buffer size isn't really important either
#define BUF_SIZE 1024
#define DELAY_BUF_SIZE (64 * 1024)
#define MAX_DELAY 10
#define LIMIT_BETWEEN_0_AND_1(x)          \
(((x) < 0) ? 0 : (((x) > 1) ? 1 : (x)))
#define LIMIT_BETWEEN_0_AND_MAX_DELAY(x)  \
(((x) < 0) ? 0 : (((x) > DELAY_BUF_SIZE) ? DELAY_BUF_SIZE : (x)))
#define UI 0
#define M_PI 3.14159265358979323846


typedef int16_t SAMPLE_Data_Type;


static const int32_t MAX_VALUE =  32767;
static const int32_t MIN_VALUE = -32768;

SAMPLE_Data_Type input_buffer[BUF_SIZE * 2];
SAMPLE_Data_Type output_buffer[BUF_SIZE * 2];
SAMPLE_Data_Type delay_intermediate_buffer[DELAY_BUF_SIZE];
SAMPLE_Data_Type reverb_intermediate_buffer[DELAY_BUF_SIZE];
SAMPLE_Data_Type chorus_intermediate_buffer[DELAY_BUF_SIZE];
// char names[NO_EFFECTS][MAX_NAME_LENGTH];

pthread_mutex_t lock;
effect_entry effect_array[10];
volatile int no_effects_applied = 0;

char *pdevice = "hw:0,0";
char *cdevice = "hw:0,0";
snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE;
int channels = 1;
int buffer_size = BUF_SIZE;     
int period_size = 0;            
int resample = 1;
snd_output_t *output = NULL;
unsigned long delay_write_pointer = 0;
unsigned long reverb_write_pointer = 0;
unsigned long chorus_write_pointer = 0;

int setparams_stream(snd_pcm_t *handle, snd_pcm_hw_params_t *params, const char *id);
int setparams_bufsize(snd_pcm_t *handle, snd_pcm_hw_params_t *params, snd_pcm_hw_params_t *tparams, snd_pcm_uframes_t bufsize, const char *id);
int setparams_set(snd_pcm_t *handle, snd_pcm_hw_params_t *params, snd_pcm_sw_params_t *swparams, const char *id);
int setparams(snd_pcm_t *phandle, snd_pcm_t *chandle, int *bufsize);
void showstat(snd_pcm_t *handle, size_t frames);
void showlatency(size_t latency);
void showinmax(size_t in_max);
void gettimestamp(snd_pcm_t *handle, snd_timestamp_t *timestamp);
long timediff(snd_timestamp_t t1, snd_timestamp_t t2);
long readbuf(snd_pcm_t *handle, SAMPLE_Data_Type *buf, long len, size_t *frames, size_t *max);
long writebuf(snd_pcm_t *handle, SAMPLE_Data_Type *buf, long len, size_t *frames);
void 
run_amplifier(SAMPLE_Data_Type *input_buff, SAMPLE_Data_Type * output_buff, unsigned long sample_count, float gain) ;
void 
run_delay(SAMPLE_Data_Type * input_buff, SAMPLE_Data_Type * output_buff, unsigned long sample_count, SAMPLE_Data_Type * temp_buff,  float dry_value,  float delay, unsigned long* write_pointer);

void 
run_reverb(SAMPLE_Data_Type * input_buff, 
    SAMPLE_Data_Type * output_buff, 
    unsigned long sample_count, 
    SAMPLE_Data_Type * temp_buff, 
    float dry_value,  
    float delay1, 
    float delay2, 
    float delay3, 
    float delay4, 
    float delay5, 
    unsigned long* write_pointer
);
void 
run_distortion(SAMPLE_Data_Type *input_buff, SAMPLE_Data_Type * output_buff, unsigned long sample_count, SAMPLE_Data_Type above_treshold, SAMPLE_Data_Type below_treshold);
void 
run_chorus_effect(SAMPLE_Data_Type * input_buff, SAMPLE_Data_Type * output_buff, unsigned long sample_count, SAMPLE_Data_Type * temp_buff,  float dry_value,  float delay_min, float delay_max, unsigned long* write_pointer);
void *print_to_UI(void *vargp);
void *wait_for_event(void *vargp);
void apply_effect(SAMPLE_Data_Type* input_buffer, SAMPLE_Data_Type* output_buffer, unsigned long sample_count);
void get_effects_to_apply();
void setscheduler(void)
{
        struct sched_param sched_param;
        if (sched_getparam(0, &sched_param) < 0) {
                printf("Scheduler getparam failed...\n");
                return;
        }
        sched_param.sched_priority = sched_get_priority_max(SCHED_FIFO);
        if (!sched_setscheduler(0, SCHED_FIFO, &sched_param)) {
                printf("Scheduler set to Round Robin with priority %i...\n", sched_param.sched_priority);
                fflush(stdout);
                return;
        }
        printf("!!!Scheduler set to Round Robin with priority %i FAILED!!!\n", sched_param.sched_priority);
}
int main(int argc, char *argv[])
{
    snd_pcm_t *phandle, *chandle;
    int err, latency, morehelp;
    int ok;
    snd_timestamp_t p_tstamp, c_tstamp;
    long no_samples;
    size_t frames_in, frames_out, in_max;
    pthread_mutex_t lock;
    total_number_of_effects = 0;


    memcpy(effect_descriptor_array.names[AMPLIFIER], "AMP", 9);
    effect_descriptor_array.args[AMPLIFIER] = 1;
    effect_descriptor_array.lims[AMPLIFIER][0].min = 0.0;
    effect_descriptor_array.lims[AMPLIFIER][0].max = 50.0;
    total_number_of_effects++;

    memcpy(effect_descriptor_array.names[1], "DELAY", 6);    
    effect_descriptor_array.args[DELAY] = 2;
    effect_descriptor_array.lims[DELAY][1].min = 0.0;
    effect_descriptor_array.lims[DELAY][1].max = 10.0;
    effect_descriptor_array.lims[DELAY][0].min = 0.0;
    effect_descriptor_array.lims[DELAY][0].max = 1.0;
    total_number_of_effects++;
    
    memcpy(effect_descriptor_array.names[2], "REVERB", 6);    
    effect_descriptor_array.args[REVERB] = 6;
    effect_descriptor_array.lims[REVERB][0].min = 0.0;
    effect_descriptor_array.lims[REVERB][0].max = 1.0;
    effect_descriptor_array.lims[REVERB][1].min = 0.0;
    effect_descriptor_array.lims[REVERB][1].max = 10.0;
    effect_descriptor_array.lims[REVERB][2].min = 0.0;
    effect_descriptor_array.lims[REVERB][2].max = 10.0;
    effect_descriptor_array.lims[REVERB][3].min = 0.0;
    effect_descriptor_array.lims[REVERB][3].max = 10.0;
    effect_descriptor_array.lims[REVERB][4].min = 0.0;
    effect_descriptor_array.lims[REVERB][4].max = 10.0;
    effect_descriptor_array.lims[REVERB][5].min = 0.0;
    effect_descriptor_array.lims[REVERB][5].max = 10.0;
    total_number_of_effects++;
    
    memcpy(effect_descriptor_array.names[DISTORTION], "DIST", 6);    
    effect_descriptor_array.args[DISTORTION] = 2;
    effect_descriptor_array.lims[DISTORTION][1].min = 0.0;
    effect_descriptor_array.lims[DISTORTION][1].max = 100.0;
    effect_descriptor_array.lims[DISTORTION][0].min = 0.0;
    effect_descriptor_array.lims[DISTORTION][0].max = 100.0;
    total_number_of_effects++;

    memcpy(effect_descriptor_array.names[CHORUS], "CHORUS", 6);    
    effect_descriptor_array.args[CHORUS] = 3;
    effect_descriptor_array.lims[CHORUS][0].min = 0.0;
    effect_descriptor_array.lims[CHORUS][0].max = 1.0;
    effect_descriptor_array.lims[CHORUS][1].min = 0.0;
    effect_descriptor_array.lims[CHORUS][1].max = 10.0;
    effect_descriptor_array.lims[CHORUS][2].min = 0.0;
    effect_descriptor_array.lims[CHORUS][2].max = 10.0;
    total_number_of_effects++;
    
    memcpy(effect_descriptor_array.names[RESET], "RESET", 6);    
    effect_descriptor_array.args[RESET] = 0;
    total_number_of_effects++;

    // setscheduler();
    #if UI

    if (pipe(fd)==-1)
    {
        fprintf(stderr, "Pipe Failed" );
        return NULL;
    }

    pid_t pid = fork();
    if(pid == 0) {
        close(fd[0]);
        createUI();
        return 0;
    } 

    printf("PROCESS pid: %d\n", pid);    
    close(fd[1]);
    pthread_t thread_id;
    if (pthread_mutex_init(&lock, NULL) != 0)
    {
        printf("\n mutex init has failed\n");
        return 1;
    }
    pthread_create(&thread_id, NULL, wait_for_event, NULL);
        


    #else
        get_effects_to_apply();
    #endif /*UI*/   
    // int opt
    
        err = snd_output_stdio_attach(&output, stdout, 0);
        if (err < 0) {
            printf("Output failed: %s\n", snd_strerror(err));
            return 0;
        }
        // buffer = malloc((latency_max * snd_pcm_format_width(format) / 8) * 2);
        printf("Playback device: %s\n", pdevice);
        printf("Capture device: %s\n", cdevice);
        printf("Sample rate: %iHz, PCM fomrat: %s, Channels: %i, Non-blocking mode\n", SAMPLE_RATE, snd_pcm_format_name(format), channels);
        if ((err = snd_pcm_open(&phandle, pdevice, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK)) < 0) {
            printf("Open error at playback handle: %s\n", snd_strerror(err));
            return 0;
        }
        if ((err = snd_pcm_open(&chandle, cdevice, SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK)) < 0) {
            printf("Open error at capture handle: %s\n", snd_strerror(err));
            return 0;
        }



        frames_in = frames_out = 0;
        if (setparams(phandle, chandle, &buffer_size) < 0)
            return 0;
        showlatency(buffer_size);

        //Link the two handles together
        if ((err = snd_pcm_link(chandle, phandle)) < 0) {
            printf("Error to link the 2 pcm handle: %s\n", snd_strerror(err));
            exit(0);
        }

        //Set silence at the beginning
        if (snd_pcm_format_set_silence(format, output_buffer, buffer_size*channels) < 0) {
            fprintf(stderr, "Error setting silence in the output buffer\n");
            return 0;
        }
        if (writebuf(phandle, output_buffer, buffer_size, &frames_out) < 0) {
            fprintf(stderr, "Error writing silence to the playback handle\n");
            return 0;
        }

        //Start the capture handle
        if ((err = snd_pcm_start(chandle)) < 0) {
            printf("Error statring the capture handle: %s\n", snd_strerror(err));
            exit(0);
        }

        gettimestamp(phandle, &p_tstamp);
        gettimestamp(chandle, &c_tstamp);
        ok = 1;
        in_max = 0;
        int count = 0;
        SAMPLE_Data_Type* buffer_to_play;
        
        printf("Start to play\n");
        while (ok) {
            if ((no_samples = readbuf(chandle, input_buffer, buffer_size, &frames_in, &in_max)) < 0){
                ok = 0;
            }
            else {
                //Lock while reading the effect_descriptor structure
                // printf("%d\n", no_samples);
                #if UI
                pthread_mutex_lock(&lock);
                #endif
                //If the number of effect to apply is 0 => simply redirect the input to the output
                if (no_effects_applied == 0){
                    buffer_to_play = input_buffer;
                }
                else{
                    //Apply the chosen effects to the audio signal
                    apply_effect(input_buffer, output_buffer, no_samples);
                    buffer_to_play = output_buffer;
                }
                #if UI
                pthread_mutex_unlock(&lock);
                #endif
                if ((no_samples = writebuf(phandle, buffer_to_play, no_samples, &frames_out)) < 0){
                    ok = 0;
                }
            }
        }
        if (ok)
            printf("Success\n");
        else
            printf("Failure\n");
        // printf("Playback:\n");
        showstat(phandle, frames_out);
        printf("Capture:\n");
        showstat(chandle, frames_in);
        showinmax(in_max);
        if (p_tstamp.tv_sec == p_tstamp.tv_sec &&
            p_tstamp.tv_usec == c_tstamp.tv_usec)
            printf("Hardware sync\n");
        snd_pcm_drop(chandle);
        snd_pcm_nonblock(chandle, 0);

        snd_pcm_nonblock(phandle, 0);
        snd_pcm_drain(phandle);
    //    snd_pcm_nonblock(phandle, !block ? 1 : 0);
        snd_pcm_unlink(chandle);
        snd_pcm_hw_free(phandle);
        snd_pcm_hw_free(chandle);
        snd_pcm_close(phandle);
        snd_pcm_close(chandle);

        #if UI
        pthread_join(thread_id, NULL);
        #endif
        return 0;
    } 


void get_effects_to_apply(){
    #if UI
    pthread_mutex_lock(&lock);
    #endif
    no_effects_applied = 1;
    effect_array[0].idx = CHORUS;
    effect_array[0].args[0] = 0.5;
    effect_array[0].args[1] = 0.4;
    effect_array[0].args[2] = 2;
    #if UI
    pthread_mutex_unlock(&lock);
    #endif
}

void apply_effect(SAMPLE_Data_Type* input_buffer, SAMPLE_Data_Type* output_buffer, unsigned long sample_count){
    for (int i = 0; i < no_effects_applied; i++){
        switch(effect_array[i].idx){

            case AMPLIFIER:
                run_amplifier(input_buffer, 
                    output_buffer, 
                    sample_count, 
                    effect_array[i].args[0]);
            break;
            case DELAY:
                run_delay(input_buffer, 
                    output_buffer, 
                    sample_count, 
                    delay_intermediate_buffer, 
                    effect_array[i].args[0], 
                    effect_array[i].args[1], 
                    &delay_write_pointer);
            break;
            case REVERB:
                run_reverb(input_buffer, 
                    output_buffer, 
                    sample_count, 
                    reverb_intermediate_buffer, 
                    effect_array[i].args[0],  
                    effect_array[i].args[1], 
                    effect_array[i].args[2], 
                    effect_array[i].args[3], 
                    effect_array[i].args[4], 
                    effect_array[i].args[5], 
                    &reverb_write_pointer);            
            break;
            case DISTORTION:
                run_distortion(input_buffer, 
                    output_buffer, 
                    sample_count, 
                    effect_array[i].args[0], 
                    effect_array[i].args[1]);
            break;
            case CHORUS:
                run_chorus_effect(input_buffer, 
                    output_buffer, 
                    sample_count, 
                    chorus_intermediate_buffer, 
                    effect_array[i].args[0], 
                    effect_array[i].args[1], 
                    effect_array[i].args[2], 
                    &chorus_write_pointer);
            break;
            case RESET:
                no_effects_applied = 0;
            break;
            default:
            break;
        }
        input_buffer = output_buffer;
    }
}

int setparams_stream(snd_pcm_t *handle, snd_pcm_hw_params_t *params, const char *id){
    int err;
    unsigned int rrate;
    err = snd_pcm_hw_params_any(handle, params);
    if (err < 0) {
        printf("Broken configuration for %s PCM: no configurations available: %s\n", snd_strerror(err), id);
        return err;
    }
    err = snd_pcm_hw_params_set_rate_resample(handle, params, resample);
    if (err < 0) {
        printf("Resample setup failed for %s (val %i): %s\n", id, resample, snd_strerror(err));
        return err;
    }
    err = snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0) {
        printf("Access type not available for %s: %s\n", id, snd_strerror(err));
        return err;
    }
    err = snd_pcm_hw_params_set_format(handle, params, format);
    if (err < 0) {
        printf("Sample format not available for %s: %s\n", id, snd_strerror(err));
        return err;
    }
    err = snd_pcm_hw_params_set_channels_near(handle, params, &channels);
    printf("%d\n", channels);
    if (err < 0) {
        printf("Channels count (%i) not available for %s: %s\n", channels, id, snd_strerror(err));
        return err;
    }
    rrate = SAMPLE_RATE;
    err = snd_pcm_hw_params_set_rate_near(handle, params, &rrate, 0);
    if (err < 0) {
        printf("Rate %iHz not available for %s: %s\n", SAMPLE_RATE, id, snd_strerror(err));
        return err;
    }
    if ((int)rrate != SAMPLE_RATE) {
        printf("Rate doesn't match (requested %iHz, get %iHz)\n", SAMPLE_RATE, err);
        return -EINVAL;
    }
    return 0;
}
int setparams_bufsize(snd_pcm_t *handle, snd_pcm_hw_params_t *params, snd_pcm_hw_params_t *tparams, snd_pcm_uframes_t period_size, const char *id)
{
    int err;
    snd_pcm_uframes_t bufsize;
    snd_pcm_hw_params_copy(params, tparams);
    bufsize = period_size * 2;
    err = snd_pcm_hw_params_set_buffer_size_near(handle, params, &bufsize);
    if (err < 0) {
        printf("Unable to set buffer size %li for %s: %s\n", bufsize, id, snd_strerror(err));
        return err;
    }
    
    err = snd_pcm_hw_params_set_period_size_near(handle, params, &period_size, 0);
    if (err < 0) {
        printf("Unable to set period size %li for %s: %s\n", period_size, id, snd_strerror(err));
        return err;
    }
    return 0;
}
int setparams_set(snd_pcm_t *handle,
  snd_pcm_hw_params_t *params,
  snd_pcm_sw_params_t *swparams,
  const char *id)
{
    int err;
    snd_pcm_uframes_t val;
    err = snd_pcm_hw_params(handle, params);
    if (err < 0) {
        printf("Unable to set hw params for %s: %s\n", id, snd_strerror(err));
        return err;
    }
    err = snd_pcm_sw_params_current(handle, swparams);
    if (err < 0) {
        printf("Unable to determine current swparams for %s: %s\n", id, snd_strerror(err));
        return err;
    }
    err = snd_pcm_sw_params_set_start_threshold(handle, swparams, 0x7fffffff);
    if (err < 0) {
        printf("Unable to set start threshold mode for %s: %s\n", id, snd_strerror(err));
        return err;
    }
    val = 4;
    err = snd_pcm_sw_params_set_avail_min(handle, swparams, val);
    if (err < 0) {
        printf("Unable to set avail min for %s: %s\n", id, snd_strerror(err));
        return err;
    }
    err = snd_pcm_sw_params(handle, swparams);
    if (err < 0) {
        printf("Unable to set sw params for %s: %s\n", id, snd_strerror(err));
        return err;
    }
    return 0;
}
int setparams(snd_pcm_t *phandle, snd_pcm_t *chandle, int *bufsize)
{
    int err;
    snd_pcm_hw_params_t *pt_params, *ct_params;     /* templates with rate, format and channels */
    snd_pcm_hw_params_t *p_params, *c_params;
    snd_pcm_sw_params_t *p_swparams, *c_swparams;
    snd_pcm_uframes_t p_size, c_size, p_psize, c_psize;
    unsigned int p_time, c_time;
    unsigned int val;
    snd_pcm_hw_params_alloca(&p_params);
    snd_pcm_hw_params_alloca(&c_params);
    snd_pcm_hw_params_alloca(&pt_params);
    snd_pcm_hw_params_alloca(&ct_params);
    snd_pcm_sw_params_alloca(&p_swparams);
    snd_pcm_sw_params_alloca(&c_swparams);

    if ((err = setparams_stream(phandle, pt_params, "playback")) < 0) {
        printf("Unable to set parameters for playback stream: %s\n", snd_strerror(err));
        exit(0);
    }
    if ((err = setparams_stream(chandle, ct_params, "capture")) < 0) {
        printf("Unable to set parameters for playback stream: %s\n", snd_strerror(err));
        exit(0);
    }
    int ok = 0;
    while (!ok){
        if ((err = setparams_bufsize(phandle, p_params, pt_params, *bufsize, "playback")) < 0) {
            printf("Unable to set sw parameters for playback stream: %s\n", snd_strerror(err));
            exit(0);
        }
        if ((err = setparams_bufsize(chandle, c_params, ct_params, *bufsize, "capture")) < 0) {
            printf("Unable to set sw parameters for playback stream: %s\n", snd_strerror(err));
            exit(0);
        }
        snd_pcm_hw_params_get_period_size(p_params, &p_psize, NULL);
        if (p_psize > (unsigned int)*bufsize)
            *bufsize = p_psize;
        snd_pcm_hw_params_get_period_size(c_params, &c_psize, NULL);
        if (c_psize > (unsigned int)*bufsize)
            *bufsize = c_psize;
        snd_pcm_hw_params_get_period_time(p_params, &p_time, NULL);
        snd_pcm_hw_params_get_period_time(c_params, &c_time, NULL);
        if (p_time != c_time)
            continue;
        snd_pcm_hw_params_get_buffer_size(p_params, &p_size);
        if (p_psize * 2 < p_size) {
            snd_pcm_hw_params_get_periods_min(p_params, &val, NULL);
            if (val > 2) {
                printf("playback device does not support 2 periods per buffer\n");
                exit(0);
            }
            continue;
        }
        snd_pcm_hw_params_get_buffer_size(c_params, &c_size);
        if (c_psize * 2 < c_size) {
            snd_pcm_hw_params_get_periods_min(c_params, &val, NULL);
            if (val > 2 ) {
                printf("capture device does not support 2 periods per buffer\n");
                exit(0);
            }
            continue;
        }
        ok = 1;
    }
    if ((err = setparams_set(phandle, p_params, p_swparams, "playback")) < 0) {
        printf("Unable to set sw parameters for playback stream: %s\n", snd_strerror(err));
        exit(0);
    }
    if ((err = setparams_set(chandle, c_params, c_swparams, "capture")) < 0) {
        printf("Unable to set sw parameters for playback stream: %s\n", snd_strerror(err));
        exit(0);
    }
    if ((err = snd_pcm_prepare(phandle)) < 0) {
        printf("Prepare error: %s\n", snd_strerror(err));
        exit(0);
    }
    snd_pcm_dump(phandle, output);
    snd_pcm_dump(chandle, output);
    fflush(stdout);
    return 0;
}
void showstat(snd_pcm_t *handle, size_t frames)
{
    int err;
    snd_pcm_status_t *status;
    snd_pcm_status_alloca(&status);
    if ((err = snd_pcm_status(handle, status)) < 0) {
        printf("Stream status error: %s\n", snd_strerror(err));
        exit(0);
    }
    printf("*** frames = %li ***\n", (long)frames);
    snd_pcm_status_dump(status, output);
}
void showlatency(size_t latency)
{
    double d;
    latency *= 2;
    d = (double)latency / SAMPLE_RATE;
    printf("Trying latency %li frames, %.3fus, %.6fms (%.4fHz)\n", (long)latency, d * 1000000, d * 1000, (double)1 / d);
}
void showinmax(size_t in_max)
{
    double d;
    printf("Maximum read: %li frames\n", (long)in_max);
    d = (double)in_max / SAMPLE_RATE;
    printf("Maximum read latency: %.3fus, %.6fms (%.4fHz)\n", d * 1000000, d * 1000, (double)1 / d);
}
void gettimestamp(snd_pcm_t *handle, snd_timestamp_t *timestamp)
{
    int err;
    snd_pcm_status_t *status;
    snd_pcm_status_alloca(&status);
    if ((err = snd_pcm_status(handle, status)) < 0) {
        printf("Stream status error: %s\n", snd_strerror(err));
        exit(0);
    }
    snd_pcm_status_get_trigger_tstamp(status, timestamp);
}

long timediff(snd_timestamp_t t1, snd_timestamp_t t2)
{
    signed long l;
    t1.tv_sec -= t2.tv_sec;
    l = (signed long) t1.tv_usec - (signed long) t2.tv_usec;
    if (l < 0) {
        t1.tv_sec--;
        l = 1000000 + l;
        l %= 1000000;
    }
    return (t1.tv_sec * 1000000) + l;
}
long readbuf(snd_pcm_t *handle, SAMPLE_Data_Type *buffer, long len, size_t *frames, size_t *max)
{
    long return_code;
    do {
        return_code = snd_pcm_readi(handle, buffer, len);
    } while (return_code == -EAGAIN);
    if (return_code > 0) {
        // printf("read %lu\n", return_code);
        *frames += return_code;
        if ((long)*max < return_code)
            *max = return_code;
        //for (int i = 0; i < return_code; i++){
          //  printf("%d\n", buffer[i]);
        //}
    }
    return return_code;
}

long writebuf(snd_pcm_t *handle, SAMPLE_Data_Type *buffer, long len, size_t *frames)
{
    static int bla = 0;
    long return_code;
    while (len > 0) {
        return_code = snd_pcm_writei(handle, buffer, len);
        if (return_code == -EAGAIN)
            continue;
                // printf("write = %li\n", r);
        if (return_code < 0){
            printf("Return code: %ld\n", return_code);
            return return_code;
        }
                // showstat(handle, 0);
        // printf("write %lu\n", return_code);
        buffer += return_code * sizeof(SAMPLE_Data_Type);
        len -= return_code;
        *frames += return_code;
    }
    return 0;
}


void 
run_amplifier(SAMPLE_Data_Type *input_buff, SAMPLE_Data_Type * output_buff, unsigned long sample_count, float gain) {
      unsigned long sample_index;
      int32_t temp_value;
        sample_count *= channels;        
      for (sample_index = 0; sample_index < sample_count; sample_index++) {
        temp_value = *input_buff * gain;
        if (temp_value < MIN_VALUE){
            *output_buff = MIN_VALUE;
        }
        else{
            if (temp_value > MAX_VALUE){
                *output_buff = MAX_VALUE;
            }
            else{
                *output_buff = temp_value;
            }
        }
        output_buff++;
        input_buff++;
    }
}

void 
run_distortion(SAMPLE_Data_Type *input_buff, SAMPLE_Data_Type * output_buff, unsigned long sample_count, SAMPLE_Data_Type above_treshold, SAMPLE_Data_Type below_treshold) {

      unsigned long sample_index;
      SAMPLE_Data_Type input_sample;
      sample_count *= channels;
      above_treshold = MAX_VALUE * above_treshold / 100;
      below_treshold = MIN_VALUE * below_treshold / 100;
      // printf("treshold: %d %d\n", above_treshold, below_treshold);
      for (sample_index = 0; sample_index < sample_count; sample_index++) {
        input_sample = *input_buff;
        if (input_sample < below_treshold){

            *output_buff = below_treshold;
        }
        else{
            if (input_sample > above_treshold){
                *output_buff = above_treshold;       
            }
            else{
                *output_buff = input_sample;
            }
        }
        // printf("%d %d\n", *input_buff, *output_buff);
        output_buff++;
        input_buff++;
    }

}

void 
run_delay(SAMPLE_Data_Type * input_buff, SAMPLE_Data_Type * output_buff, unsigned long sample_count, SAMPLE_Data_Type * temp_buff,  float dry_value,  float delay, unsigned long* write_pointer){  
  SAMPLE_Data_Type input_sample;
  float wet_value;
  unsigned long buffer_read_offset;
  unsigned long buffer_size_minus_one;
  unsigned long buffer_write_offset;
  unsigned long ldelay;
  unsigned long sample_index;

  sample_count *= channels;
  buffer_size_minus_one = DELAY_BUF_SIZE - 1;
  ldelay = (unsigned long)
  (LIMIT_BETWEEN_0_AND_MAX_DELAY(delay* SAMPLE_RATE * channels));

  
  buffer_write_offset = *write_pointer;
  buffer_read_offset
  = (buffer_write_offset + DELAY_BUF_SIZE - ldelay) & buffer_size_minus_one;
  dry_value = LIMIT_BETWEEN_0_AND_1(dry_value);
  wet_value = 1.0 - dry_value;
  for (sample_index = 0; sample_index < sample_count; sample_index++) {
    input_sample = *(input_buff);
    *(output_buff) =  (dry_value * input_sample + wet_value * temp_buff[((sample_index + buffer_read_offset) & buffer_size_minus_one)]);
    temp_buff[((sample_index + buffer_write_offset) & buffer_size_minus_one)] = input_sample;
    input_buff++;
    output_buff++;
    }

    *write_pointer = ((*write_pointer + sample_count) & buffer_size_minus_one);
}

void 
run_chorus_effect(SAMPLE_Data_Type * input_buff, SAMPLE_Data_Type * output_buff, unsigned long sample_count, SAMPLE_Data_Type * temp_buff,  float dry_value,  float delay_min, float delay_max, unsigned long* write_pointer){  
  SAMPLE_Data_Type input_sample;
  float wet_value;
  float value_to_shift;
  float value_to_multiply;
  static float delay;
  unsigned long delay_in_samples;
  unsigned long buffer_read_offset;
  unsigned long buffer_size_minus_one;
  unsigned long buffer_write_offset;
  unsigned long ldelay;
  unsigned long sample_index;
  float delay_step =0.01;
  int control = 1;
  sample_count *= channels;
  // printf("%d\n", delay);
  buffer_size_minus_one = DELAY_BUF_SIZE - 1;
  value_to_multiply = (delay_max - delay_min) / 2;
  value_to_shift = delay_max - value_to_multiply;
  
  buffer_write_offset = *write_pointer;
  
  // printf("%lu %lu %d\n", buffer_write_offset, buffer_read_offset, ldelay);
  dry_value = LIMIT_BETWEEN_0_AND_1(dry_value);
  wet_value = 1.0 - dry_value;

  for (sample_index = 0; sample_index < sample_count; sample_index++) {
    if (value_to_multiply == 0){
        input_sample = *(input_buff);
        *(output_buff) =  dry_value * input_sample;
    }
    else{
        delay += control*delay_step;
  
        if (delay > delay_max) {
            control = -1;
        } 

        if (delay < delay_min) {
            control = 1;
        }

        // printf("ValueTo value_to_multiply%f\n", value_to_multiply);
        // printf("value_to_shift %f\n", value_to_shift);
        // delay = (sin(100 * (2 * M_PI) * sample_index / SAMPLE_RATE)) * value_to_multiply + value_to_shift;
        // printf("%f\n", delay);
        delay_in_samples = (unsigned long)(LIMIT_BETWEEN_0_AND_MAX_DELAY(delay* SAMPLE_RATE * channels));
        // printf("%lu\n", delay_in_samples);
        buffer_read_offset = (buffer_write_offset + DELAY_BUF_SIZE - delay_in_samples) & buffer_size_minus_one;
        input_sample = *(input_buff);
        *(output_buff) =  (dry_value * input_sample + wet_value * temp_buff[((sample_index + buffer_read_offset) & buffer_size_minus_one)]);
    }
    temp_buff[((sample_index + buffer_write_offset) & buffer_size_minus_one)] = input_sample;
    // if (*input_buff - *output_buff)
        // printf("%d %d %d %d %lu\n", (int)*input_buff, (int)(dry_value * input_sample), (int)(temp_buff[((sample_index + buffer_read_offset) & buffer_size_minus_one)]), (int)(*output_buff), sample_index);
    input_buff++;
    output_buff++;
    }

    *write_pointer = ((*write_pointer + sample_count) & buffer_size_minus_one);
}

void 
run_reverb(SAMPLE_Data_Type * input_buff, 
    SAMPLE_Data_Type * output_buff, 
    unsigned long sample_count, 
    SAMPLE_Data_Type * temp_buff, 
    float dry_value,  
    float delay1, 
    float delay2, 
    float delay3, 
    float delay4, 
    float delay5, 
    unsigned long* write_pointer
){  
  
  SAMPLE_Data_Type input_sample;
  float wet_value;
  unsigned long buffer_read_offset1;
  unsigned long buffer_read_offset2;
  unsigned long buffer_read_offset3;
  unsigned long buffer_read_offset4;
  unsigned long buffer_read_offset5;
  unsigned long buffer_size_minus_one;
  unsigned long buffer_write_offset;
  unsigned long ldelay1;
  unsigned long ldelay2;
  unsigned long ldelay3;
  unsigned long ldelay4;
  unsigned long ldelay5;
  unsigned long sample_index;

  sample_count *= channels;
  buffer_size_minus_one = DELAY_BUF_SIZE - 1;
  ldelay1 = (unsigned long)
  (LIMIT_BETWEEN_0_AND_MAX_DELAY(delay1* SAMPLE_RATE * channels));
    ldelay2 = (unsigned long)
  (LIMIT_BETWEEN_0_AND_MAX_DELAY(delay2* SAMPLE_RATE * channels));
  ldelay3 = (unsigned long)
  (LIMIT_BETWEEN_0_AND_MAX_DELAY(delay3* SAMPLE_RATE * channels));
  ldelay4 = (unsigned long)
  (LIMIT_BETWEEN_0_AND_MAX_DELAY(delay4* SAMPLE_RATE * channels));
  ldelay5 = (unsigned long)
  (LIMIT_BETWEEN_0_AND_MAX_DELAY(delay5* SAMPLE_RATE * channels));

  
  buffer_write_offset = *write_pointer;
  buffer_read_offset1
  // printf("%f\n", delay);
  = (buffer_write_offset + DELAY_BUF_SIZE - ldelay1) & buffer_size_minus_one;
  buffer_read_offset2
  = (buffer_write_offset + DELAY_BUF_SIZE - ldelay2) & buffer_size_minus_one;
  buffer_read_offset3
  = (buffer_write_offset + DELAY_BUF_SIZE - ldelay3) & buffer_size_minus_one;
  buffer_read_offset4
  = (buffer_write_offset + DELAY_BUF_SIZE - ldelay4) & buffer_size_minus_one;
  buffer_read_offset5
  = (buffer_write_offset + DELAY_BUF_SIZE - ldelay5) & buffer_size_minus_one;
  dry_value = LIMIT_BETWEEN_0_AND_1(dry_value);
  wet_value = 1 - dry_value;
  // printf("%f\n", dry_value);
  // printf("%f\n", wet_value);
    int32_t temp_value;
  for (sample_index = 0; sample_index < sample_count; sample_index++) {
    input_sample = *(input_buff);
    temp_value =  dry_value * input_sample + wet_value * (
        temp_buff[((sample_index + buffer_read_offset1) & buffer_size_minus_one)] + 
        temp_buff[((sample_index + buffer_read_offset2) & buffer_size_minus_one)] +
        temp_buff[((sample_index + buffer_read_offset3) & buffer_size_minus_one)] +
        temp_buff[((sample_index + buffer_read_offset4) & buffer_size_minus_one)] +
        temp_buff[((sample_index + buffer_read_offset5) & buffer_size_minus_one)]);
    if (temp_value < MIN_VALUE){
            *output_buff = MIN_VALUE;
            // printf("Under %d %d\n", (int)temp_value, (int)*output_buff);
        }
        else{
            if (temp_value > MAX_VALUE){
                *output_buff = MAX_VALUE;
                // printf("Over%d %d\n", (int)temp_value, (int)*output_buff);
            }
            else{
                *output_buff = temp_value;
            }
        }
    temp_buff[((sample_index + buffer_write_offset) & buffer_size_minus_one)] = input_sample;
    // printf("%A %A\n", *input_buff, *output_buff);
    input_buff++;
    output_buff++;
    }

    *write_pointer = ((*write_pointer + sample_count) & buffer_size_minus_one);
}

void *print_to_UI(void *vargp)
{   
    // printf("HERE\n");
    // char (*nam)[100][100] = vargp;
    // printf("HERE%s\n", nam[5]);
    // for (int i  = 0; i < 3; i++)
    //     printf("name: %s\n", nam[i]);
    
    createUI();

    return NULL;
}

void *wait_for_event(void *vargp){
    char arg_string[100];
    int arg_length_count = 0;
    char effect_idx, no_args;
    float arguments[100];
    char c;
    while(1){
        //Read idx and convert it to number
        read(fd[0], &effect_idx, sizeof(effect_idx));
        effect_idx -= (int)'0';

        //Read the number of arguments and convert it to number
        read(fd[0], &no_args, sizeof(no_args));
        no_args -= (int)'0';

        //Read the arguments as strings: {arg}
        for (int i = 0; i < no_args; i++){

            read(fd[0], &c, sizeof(char));
            arg_length_count = 0;
            while(c != '}'){
                read(fd[0], &c, sizeof(char));
                arg_string[arg_length_count++] = c;
            }
            arg_string[arg_length_count - 1] = 0;
            //Convert the arguments to float
            arguments[i] = atof(arg_string);
        }

        //Write to the effect_array structure
        pthread_mutex_lock(&lock);
        effect_array[no_effects_applied].idx = effect_idx;
        for (int i = 0; i <no_args; i++){
            effect_array[no_effects_applied].args[i] = arguments[i];
        }
        no_effects_applied++;
        pthread_mutex_unlock(&lock);
    }

}
