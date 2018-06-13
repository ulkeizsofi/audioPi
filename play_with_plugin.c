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


// for the amplifier, the sample rate doesn't really matter
#define SAMPLE_RATE 22050

#define AMPLIFIER 0
#define DELAY 1
#define REVERB 2
#define DISTORTION 3
#define RESET 4
#define MAX_NAME_LENGTH  20
#define NO_EFFECTS 5
// the buffer size isn't really important either
#define BUF_SIZE 2048
#define DELAY_BUF_SIZE (64 * 1024)
#define MAX_DELAY 10
#define LIMIT_BETWEEN_0_AND_1(x)          \
(((x) < 0) ? 0 : (((x) > 1) ? 1 : (x)))
#define LIMIT_BETWEEN_0_AND_MAX_DELAY(x)  \
(((x) < 0) ? 0 : (((x) > DELAY_BUF_SIZE) ? DELAY_BUF_SIZE : (x)))
#define UI 1

typedef int16_t SAMPLE_Data_Type;


static const int32_t MAX_VALUE =  32767;
static const int32_t MIN_VALUE = -32768;

SAMPLE_Data_Type pInBuffer[BUF_SIZE * 2];
SAMPLE_Data_Type pOutBuffer[BUF_SIZE * 2];
SAMPLE_Data_Type pIntermBuffer[DELAY_BUF_SIZE];
// char names[NO_EFFECTS][MAX_NAME_LENGTH];

pthread_mutex_t lock;
effectEntry effectArray[10];
volatile int no_effects_applied = 0;

char *pdevice = "hw:0,0";
char *cdevice = "hw:0,0";
snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE;
int channels = 1;
int buffer_size = BUF_SIZE;     
int period_size = 0;            
int block = 0;                  /* block mode */
int use_poll = 0;
int resample = 1;
snd_output_t *output = NULL;

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
runMonoAmplifier(SAMPLE_Data_Type *pfInput, SAMPLE_Data_Type * pfOutput, unsigned long SampleCount, float Gain) ;
void 
runSimpleDelayLine(SAMPLE_Data_Type * pfInput, SAMPLE_Data_Type * pfOutput, unsigned long SampleCount, SAMPLE_Data_Type * pfBuffer,  float fDry,  float Delay, unsigned long* writePointer);
void 
runReverbSource(SAMPLE_Data_Type * pfInput, SAMPLE_Data_Type * pfOutput, unsigned long SampleCount, SAMPLE_Data_Type * pfBuffer, float fDry,  SAMPLE_Data_Type Delay1, SAMPLE_Data_Type Delay2, unsigned long* writePointer);
void 
runDistortionEffect(SAMPLE_Data_Type *pfInput, SAMPLE_Data_Type * pfOutput, unsigned long SampleCount, SAMPLE_Data_Type above_treshold, SAMPLE_Data_Type below_treshold);
void *printToUI(void *vargp);
void *wait_for_event(void *vargp);
void apply_effect(SAMPLE_Data_Type* input_buffer, SAMPLE_Data_Type* output_buffer, unsigned long sample_count, SAMPLE_Data_Type* intermediate_buffer, unsigned long* write_offset);
void get_effects_to_apply();
void setscheduler(void)
{
        struct sched_param sched_param;
        if (sched_getparam(0, &sched_param) < 0) {
                printf("Scheduler getparam failed...\n");
                return;
        }
        sched_param.sched_priority = sched_get_priority_max(SCHED_RR);
        if (!sched_setscheduler(0, SCHED_RR, &sched_param)) {
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
    unsigned long writePointer = 0;
    pthread_mutex_t lock;
    idx = 5;


    memcpy(effectDescriptorArray.names[AMPLIFIER], "AMP", 9);
    effectDescriptorArray.args[AMPLIFIER] = 1;
    effectDescriptorArray.lims[AMPLIFIER][0].min = 0.0;
    effectDescriptorArray.lims[AMPLIFIER][0].max = 50.0;

    memcpy(effectDescriptorArray.names[1], "DELAY", 6);    
    effectDescriptorArray.args[DELAY] = 2;
    effectDescriptorArray.lims[DELAY][1].min = 0.0;
    effectDescriptorArray.lims[DELAY][1].max = 10.0;
    effectDescriptorArray.lims[DELAY][0].min = 0.0;
    effectDescriptorArray.lims[DELAY][0].max = 1.0;
    
    memcpy(effectDescriptorArray.names[2], "REVERB", 6);    
    effectDescriptorArray.args[REVERB] = 3;
    effectDescriptorArray.lims[REVERB][2].min = 0.0;
    effectDescriptorArray.lims[REVERB][2].max = 10.0;
    effectDescriptorArray.lims[REVERB][1].min = 0.0;
    effectDescriptorArray.lims[REVERB][1].max = 10.0;
    effectDescriptorArray.lims[REVERB][0].min = 0.0;
    effectDescriptorArray.lims[REVERB][0].max = 1.0;
    
    memcpy(effectDescriptorArray.names[DISTORTION], "DIST", 6);    
    effectDescriptorArray.args[DISTORTION] = 2;
    effectDescriptorArray.lims[DISTORTION][1].min = -30000.0;
    effectDescriptorArray.lims[DISTORTION][1].max = 0.0;
    effectDescriptorArray.lims[DISTORTION][0].min = 0.0;
    effectDescriptorArray.lims[DISTORTION][0].max = 30000.0;
    
    memcpy(effectDescriptorArray.names[RESET], "RESET", 6);    
    effectDescriptorArray.args[RESET] = 0;
    // listPlugins();

    // setscheduler();
    #if UI
    pthread_t thread_id;
    if (pthread_mutex_init(&lock, NULL) != 0)
    {
        printf("\n mutex init has failed\n");
        return 1;
    }
    // pthread_create(&thread_id, NULL, printToUI, NULL);

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

    pthread_t thread_id2;
    pthread_create(&thread_id2, NULL, wait_for_event, NULL);
    close(fd[1]);
        


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
        printf("Playback device is %s\n", pdevice);
        printf("Capture device is %s\n", cdevice);
        printf("Parameters are %iHz, %s, %i channels, %s mode\n", SAMPLE_RATE, snd_pcm_format_name(format), channels, block ? "blocking" : "non-blocking");
        if ((err = snd_pcm_open(&phandle, pdevice, SND_PCM_STREAM_PLAYBACK, block ? 0 : SND_PCM_NONBLOCK)) < 0) {
            printf("Playback open error: %s\n", snd_strerror(err));
            return 0;
        }
        if ((err = snd_pcm_open(&chandle, cdevice, SND_PCM_STREAM_CAPTURE, block ? 0 : SND_PCM_NONBLOCK)) < 0) {
            printf("Record open error: %s\n", snd_strerror(err));
            return 0;
        }



        // while (1) {
        frames_in = frames_out = 0;
        if (setparams(phandle, chandle, &buffer_size) < 0)
            return 0;
        showlatency(buffer_size);
        if ((err = snd_pcm_link(chandle, phandle)) < 0) {
            printf("Streams link error: %s\n", snd_strerror(err));
            exit(0);
        }
        if (snd_pcm_format_set_silence(format, pOutBuffer, buffer_size*channels) < 0) {
            fprintf(stderr, "silence error\n");
            return 0;
        }
        if (writebuf(phandle, pOutBuffer, buffer_size, &frames_out) < 0) {
            fprintf(stderr, "write error\n");
            return 0;
        }

        if ((err = snd_pcm_start(chandle)) < 0) {
            printf("Go error: %s\n", snd_strerror(err));
            exit(0);
        }
        gettimestamp(phandle, &p_tstamp);
        gettimestamp(chandle, &c_tstamp);
        ok = 1;
        in_max = 0;
        printf("Start to play\n");
        float amp = 0.1;
        int count = 0;
        SAMPLE_Data_Type* buffer_to_play;
        
        while (ok) {
            if (use_poll) {
                /* use poll to wait for next event */
                snd_pcm_wait(chandle, 1000);
            }
            if ((no_samples = readbuf(chandle, pInBuffer, buffer_size, &frames_in, &in_max)) < 0){
                ok = 0;
            }
            else {
                // printf("%d\n", no_samples);
                // SAMPLE_Data_Type* temp = pInBuffer;
                    // printf("STARTING\n");
                    // for (int i = 0; i < no_samples; ++i)
                    // {
                    //     printf("%A\n", *temp);
                    //     temp++;
                    // }
                // runMonoAmplifier(pInBuffer, pOutBuffer, no_samples, amp);
                // count++;
                // if (count == 10000){
                //     count = 0;
                //     amp += 0.1;
                    // printf("%f\n", amp);
                // }
                // runSimpleDelayLine(pInBuffer, pOutBuffer, no_samples, pIntermBuffer, 0.5, 0.5, &writePointer);
                // printf("%lu\n", writePointer);
                    // runSimpleDelayLine(pInBuffer, pOutBuffer, r, pIntermBuffer, 0.5, 1, &writePointer);
                pthread_mutex_lock(&lock);
                // printf("%d\n", no_effects_applied);
                if (no_effects_applied == 0){
                    buffer_to_play = pInBuffer;
                }
                else{
                    apply_effect(pInBuffer, pOutBuffer, no_samples , pIntermBuffer, &writePointer);
                    buffer_to_play = pOutBuffer;
                }
                pthread_mutex_unlock(&lock);

                    // temp = pOutBuffer; 
                    // printf("STARTING\n");
                    // for (int i = 0; i < no_samples; ++i)
                    // {
                    //     printf("%A\n", *temp);
                    //     temp++;
                    // }
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
        pthread_join(thread_id, NULL);
        return 0;
    } 


void get_effects_to_apply(){
    pthread_mutex_lock(&lock);
    no_effects_applied = 2;
    effectArray[0].idx = DELAY;
    effectArray[0].args[0] = 0.5;
    effectArray[0].args[1] = 0.5;
    // effectArray[0].args[0] = 0.5;
    effectArray[1].idx = AMPLIFIER;
    effectArray[1].args[0] = 2;

    pthread_mutex_unlock(&lock);
}

void apply_effect(SAMPLE_Data_Type* input_buffer, SAMPLE_Data_Type* output_buffer, unsigned long sample_count, SAMPLE_Data_Type* intermediate_buffer, unsigned long* write_offset){
    // printf("%d\n", no_effects_applied);
    for (int i = 0; i < no_effects_applied; i++){
        printf("%d\n", i);
        switch(effectArray[i].idx){

            case AMPLIFIER:
            runMonoAmplifier(input_buffer, output_buffer, sample_count, effectArray[i].args[0]);
            break;
            case DELAY:
          //  printf("asdas+");
            // void runSimpleDelayLine(SAMPLE_Data_Type * pfInput, SAMPLE_Data_Type * pfOutput, unsigned long SampleCount, SAMPLE_Data_Type * pfBuffer,  SAMPLE_Data_Type fDry,  SAMPLE_Data_Type Delay, unsigned long* writePointer)
            // printf("CALLED DELAY\n");
            runSimpleDelayLine(input_buffer, output_buffer, sample_count, intermediate_buffer, effectArray[i].args[0], effectArray[i].args[1], write_offset);
            break;
            case REVERB:
            runReverbSource(input_buffer, output_buffer, sample_count, intermediate_buffer, effectArray[i].args[0],  effectArray[i].args[1], effectArray[i].args[2], write_offset);            
            break;
            case DISTORTION:
            runDistortionEffect(input_buffer, output_buffer, sample_count, effectArray[i].args[0], effectArray[i].args[1]);
            break;
            case RESET:
            pthread_mutex_lock(&lock);
            no_effects_applied = 0;
            printf("%d\n", no_effects_applied);
            pthread_mutex_unlock(&lock);
            default:
          //  input_buffer = output_buffer;
            break;
        }
        input_buffer = output_buffer;
// 
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
int setparams_bufsize(snd_pcm_t *handle, snd_pcm_hw_params_t *params, snd_pcm_hw_params_t *tparams, snd_pcm_uframes_t bufsize, const char *id)
{
    int err;
    snd_pcm_uframes_t periodsize;
    snd_pcm_hw_params_copy(params, tparams);
    periodsize = bufsize * 2;
    err = snd_pcm_hw_params_set_buffer_size_near(handle, params, &periodsize);
    if (err < 0) {
        printf("Unable to set buffer size %li for %s: %s\n", bufsize * 2, id, snd_strerror(err));
        return err;
    }
    if (period_size > 0)
        periodsize = period_size;
    else
        periodsize /= 2;
    err = snd_pcm_hw_params_set_period_size_near(handle, params, &periodsize, 0);
    if (err < 0) {
        printf("Unable to set period size %li for %s: %s\n", periodsize, id, snd_strerror(err));
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
    if (!block)
        val = 64;
    else
        snd_pcm_hw_params_get_period_size(params, &val, NULL);
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
    int err, last_bufsize = *bufsize;
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
      //   if (buffer_size > 0) {
      //           *bufsize = buffer_size;
      //           goto __set_it;
      //   }
    __again:
      //   if (buffer_size > 0)
      //           return -1;
      //   if (last_bufsize == *bufsize)
      //           *bufsize += 4;
      //   last_bufsize = *bufsize;
      //   if (*bufsize > latency_max)
      //           return -1;
      // __set_it:
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
        goto __again;
    snd_pcm_hw_params_get_buffer_size(p_params, &p_size);
    if (p_psize * 2 < p_size) {
        snd_pcm_hw_params_get_periods_min(p_params, &val, NULL);
        if (val > 2) {
            printf("playback device does not support 2 periods per buffer\n");
            exit(0);
        }
        goto __again;
    }
    snd_pcm_hw_params_get_buffer_size(c_params, &c_size);
    if (c_psize * 2 < c_size) {
        snd_pcm_hw_params_get_periods_min(c_params, &val, NULL);
        if (val > 2 ) {
            printf("capture device does not support 2 periods per buffer\n");
            exit(0);
        }
        goto __again;
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
long readbuf(snd_pcm_t *handle, SAMPLE_Data_Type *buf, long len, size_t *frames, size_t *max)
{
    long r;
 //   if (!block) {
        do {
            r = snd_pcm_readi(handle, buf, len);
        } while (r == -EAGAIN);
        //printf("%d\n", r);
        if (r > 0) {
            *frames += r;
            if ((long)*max < r)
                *max = r;
        }
                // printf("read = %li\n", r);
  /*  } else {
        int frame_bytes = (snd_pcm_format_width(format) / 8) * channels;
        do {
            r = snd_pcm_readi(handle, buf, len);
            if (r > 0) {
                buf += r * frame_bytes;
                len -= r;
                *frames += r;
                if ((long)*max < r)
                    *max = r;
            }
                        // printf("r = %li, len = %li\n", r, len);
        } while (r >= 1 && len > 0);
    }*/
        // showstat(handle, 0);
    return r;
}
long writebuf(snd_pcm_t *handle, SAMPLE_Data_Type *buf, long len, size_t *frames)
{
    static int bla = 0;
    long r;
    while (len > 0) {
        r = snd_pcm_writei(handle, buf, len);
        if (r == -EAGAIN)
            continue;
                // printf("write = %li\n", r);
        if (r < 0){
            printf("Return code: %ld\n", r);
            return r;
        }
                // showstat(handle, 0);
        buf += r * 4;
        len -= r;
        *frames += r;
    }
    return 0;
}


void 
runMonoAmplifier(SAMPLE_Data_Type *pfInput, SAMPLE_Data_Type * pfOutput, unsigned long SampleCount, float Gain) {
// 
      unsigned long lSampleIndex;
      int32_t temp_value;
      // printf("%f\n", Gain);
        SampleCount *= channels;        
      for (lSampleIndex = 0; lSampleIndex < SampleCount; lSampleIndex++) {
        // printf("%f\n", 20 * log10(Gain));
        // *pfOutput = *pfInput * Gain; //20*log10(x).
        temp_value = *pfInput * Gain;
        // *pfOutput = *pfInput;
        if (temp_value < MIN_VALUE){
            *pfOutput = MIN_VALUE;
            // printf("Under %d %d\n", (int)temp_value, (int)*pfOutput);
        }
        else{
            if (temp_value > MAX_VALUE){
                *pfOutput = MAX_VALUE;
                // printf("Over%d %d\n", (int)temp_value, (int)*pfOutput);
            }
            else{
                *pfOutput = temp_value;
            }
        }
        // printf("%d %d %f\n", *pfOutput, *pfInput, Gain);
        // printf("%A %A %A\n", *pfInput, *pfOutput, Gain);
        pfOutput++;
        pfInput++;
    }
}

void 
runDistortionEffect(SAMPLE_Data_Type *pfInput, SAMPLE_Data_Type * pfOutput, unsigned long SampleCount, SAMPLE_Data_Type above_treshold, SAMPLE_Data_Type below_treshold) {

      unsigned long lSampleIndex;
      SAMPLE_Data_Type input_sample;
      SampleCount *= channels;
      for (lSampleIndex = 0; lSampleIndex < SampleCount; lSampleIndex++) {
        input_sample = *pfInput;
        if (input_sample < below_treshold){

            *pfOutput = below_treshold;
        }
        else{
            if (input_sample > above_treshold){
                *pfOutput = above_treshold;       
            }
            else{
                *pfOutput = input_sample;
            }
        }
        // printf("%A %A %A\n", *pfInput, *pfOutput, Gain);
        pfOutput++;
        pfInput++;
    }

}

void 
runSimpleDelayLine(SAMPLE_Data_Type * pfInput, SAMPLE_Data_Type * pfOutput, unsigned long SampleCount, SAMPLE_Data_Type * pfBuffer,  float fDry,  float Delay, unsigned long* writePointer){  
  SAMPLE_Data_Type fInputSample;
  float fWet;
  unsigned long lBufferReadOffset;
  unsigned long lBufferSizeMinusOne;
  unsigned long lBufferWriteOffset;
  unsigned long lDelay;
  unsigned long lSampleIndex;

  SampleCount *= channels;
  // printf("%d\n", Delay);
  lBufferSizeMinusOne = DELAY_BUF_SIZE - 1;
  lDelay = (unsigned long)
  (LIMIT_BETWEEN_0_AND_MAX_DELAY(Delay* SAMPLE_RATE * channels));

  
  lBufferWriteOffset = *writePointer;
  lBufferReadOffset
  = (lBufferWriteOffset + DELAY_BUF_SIZE - lDelay) & lBufferSizeMinusOne;
  // printf("%lu %lu %d\n", lBufferWriteOffset, lBufferReadOffset, lDelay);
  fDry = LIMIT_BETWEEN_0_AND_1(fDry);
  fWet = 1.0 - fDry;
  // printf("%f\n", fDry);
  // printf("%f\n", fWet);
  for (lSampleIndex = 0; lSampleIndex < SampleCount; lSampleIndex++) {
    fInputSample = *(pfInput);
    *(pfOutput) =  (fDry * fInputSample + fWet * pfBuffer[((lSampleIndex + lBufferReadOffset) & lBufferSizeMinusOne)]);
    pfBuffer[((lSampleIndex + lBufferWriteOffset) & lBufferSizeMinusOne)] = fInputSample;
    // if (*pfInput - *pfOutput)
        // printf("%d %d %d %d %lu\n", (int)*pfInput, (int)(fDry * fInputSample), (int)(pfBuffer[((lSampleIndex + lBufferReadOffset) & lBufferSizeMinusOne)]), (int)(*pfOutput), lSampleIndex);
    pfInput++;
    pfOutput++;
    }

    *writePointer = ((*writePointer + SampleCount) & lBufferSizeMinusOne);
    // printf("%lu %d %d\n", *writePointer, SampleCount, lBufferSizeMinusOne);
}

void 
runReverbSource(SAMPLE_Data_Type * pfInput, SAMPLE_Data_Type * pfOutput, unsigned long SampleCount, SAMPLE_Data_Type * pfBuffer, float fDry,  SAMPLE_Data_Type Delay1, SAMPLE_Data_Type Delay2, unsigned long* writePointer){  
  
  SAMPLE_Data_Type fInputSample;
  float fWet;
  unsigned long lBufferReadOffset1;
  unsigned long lBufferReadOffset2;
  unsigned long lBufferSizeMinusOne;
  unsigned long lBufferWriteOffset;
  unsigned long lDelay1;
  unsigned long lDelay2;
  unsigned long lSampleIndex;

  // printf("%f\n", Delay);
  SampleCount *= channels;
  lBufferSizeMinusOne = DELAY_BUF_SIZE - 1;
  lDelay1 = (unsigned long)
  (LIMIT_BETWEEN_0_AND_MAX_DELAY(Delay1* SAMPLE_RATE * channels));
    lDelay2 = (unsigned long)
  (LIMIT_BETWEEN_0_AND_MAX_DELAY(Delay2* SAMPLE_RATE * channels));

  
  lBufferWriteOffset = *writePointer;
  lBufferReadOffset1
  = (lBufferWriteOffset + DELAY_BUF_SIZE - lDelay1) & lBufferSizeMinusOne;
  lBufferReadOffset2
  = (lBufferWriteOffset + DELAY_BUF_SIZE - lDelay2) & lBufferSizeMinusOne;
  fDry = LIMIT_BETWEEN_0_AND_1(fDry);
  fWet = 1 - fDry;
  // printf("%f\n", fDry);
  // printf("%f\n", fWet);
  for (lSampleIndex = 0; lSampleIndex < SampleCount; lSampleIndex++) {
    fInputSample = *(pfInput);
    *(pfOutput) =  fDry * fInputSample + fWet * (pfBuffer[((lSampleIndex + lBufferReadOffset1) & lBufferSizeMinusOne)] + pfBuffer[(lSampleIndex + lBufferReadOffset2) & lBufferSizeMinusOne]);
    pfBuffer[((lSampleIndex + lBufferWriteOffset) & lBufferSizeMinusOne)] = fInputSample;
    // printf("%A %A\n", *pfInput, *pfOutput);
    pfInput++;
    pfOutput++;
    }

    *writePointer = ((*writePointer + SampleCount) & lBufferSizeMinusOne);
}

void *printToUI(void *vargp)
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
    // printf("%s\n", concat_string);
    while(1){
        read(fd[0], &effect_idx, sizeof(effect_idx));
        effect_idx -= (int)'0';
        read(fd[0], &no_args, sizeof(no_args));
        no_args -= (int)'0';
        for (int i = 0; i < no_args; i++){

            read(fd[0], &c, sizeof(char));
            arg_length_count = 0;
            while(c != '}'){
                read(fd[0], &c, sizeof(char));
                arg_string[arg_length_count++] = c;
            }
            arg_string[arg_length_count - 1] = 0;
            arguments[i] = atof(arg_string);
        }

        pthread_mutex_lock(&lock);
        effectArray[no_effects_applied].idx = effect_idx;
        for (int i = 0; i <no_args; i++){
            effectArray[no_effects_applied].args[i] = arguments[i];
        }
        no_effects_applied++;
        pthread_mutex_unlock(&lock);
    }

}