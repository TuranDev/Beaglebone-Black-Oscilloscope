#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>
#include <alsa/asoundlib.h>
#include <math.h>
#include <alloca.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>

#include "inputs.h"
#include "display.h"
#include "processing.h"

int gShouldStop = 0;

//Variables For Oscilloscope
float timeDiv = 1;
int offset = 0;

//Used by both
int voltageDiv = 1;

//Variables For Signal Generator
int frequency[2] = { 100, 100 }; //50Hz-20000Hz default
int waveType[2] = { 0, 0 }; //0=sin,1=square,2=channel off
float amplitude[2] = { 1, 1 };//0-1

//Menu and Mode Variables
int menu = 0;
int mode = -1;

char *pdevice = "hw:0,0";
char *cdevice = "hw:0,0";
snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE;
int rate = 96000;
int channels = 2;
int buffer_size = 2000; /* auto */
int period_size = 8; /* auto */
int latency_min = 32; /* in frames / 2 */
int latency_max = 2048; /* in frames / 2 */
int loop_sec = 30; /* seconds */
int block = 0; /* block mode */
int use_poll = 1;
int resample = 1;
int should_stop = 0;
snd_output_t *output = NULL;

// Handle Ctrl-C
void interruptHandler(int var) {
	gShouldStop = 1;
}

int setparams_stream(snd_pcm_t *handle, snd_pcm_hw_params_t *params,
		const char *id) {
	int err;
	unsigned int rrate;
	err = snd_pcm_hw_params_any(handle, params);
	if (err < 0) {
		printf(
				"Broken configuration for %s PCM: no configurations available: %s\n",
				snd_strerror(err), id);
		return err;
	}
	err = snd_pcm_hw_params_set_rate_resample(handle, params, resample);
	if (err < 0) {
		printf("Resample setup failed for %s (val %i): %s\n", id, resample,
				snd_strerror(err));
		return err;
	}
	err = snd_pcm_hw_params_set_access(handle, params,
			SND_PCM_ACCESS_RW_INTERLEAVED);
	if (err < 0) {
		printf("Access type not available for %s: %s\n", id, snd_strerror(err));
		return err;
	}
	err = snd_pcm_hw_params_set_format(handle, params, format);
	if (err < 0) {
		printf("Sample format not available for %s: %s\n", id,
				snd_strerror(err));
		return err;
	}
	err = snd_pcm_hw_params_set_channels(handle, params, channels);
	if (err < 0) {
		printf("Channels count (%i) not available for %s: %s\n", channels, id,
				snd_strerror(err));
		return err;
	}
	rrate = rate;
	err = snd_pcm_hw_params_set_rate_near(handle, params, &rrate, 0);
	if (err < 0) {
		printf("Rate %iHz not available for %s: %s\n", rate, id,
				snd_strerror(err));
		return err;
	}
	if ((int) rrate != rate) {
		printf("Rate doesn't match (requested %iHz, get %iHz)\n", rate, err);
		return -EINVAL;
	}
	return 0;
}

int setparams_bufsize(snd_pcm_t *handle, snd_pcm_hw_params_t *params,
		snd_pcm_hw_params_t *tparams, snd_pcm_uframes_t bufsize, const char *id) {
	int err;
	snd_pcm_uframes_t periodsize;
	snd_pcm_hw_params_copy(params, tparams);
	periodsize = bufsize * 2;
	err = snd_pcm_hw_params_set_buffer_size_near(handle, params, &periodsize);
	if (err < 0) {
		printf("Unable to set buffer size %li for %s: %s\n", bufsize * 2, id,
				snd_strerror(err));
		return err;
	}
	if (period_size > 0)
		periodsize = period_size;
	else
		periodsize /= 2;
	err = snd_pcm_hw_params_set_period_size_near(handle, params, &periodsize,
			0);
	if (err < 0) {
		printf("Unable to set period size %li for %s: %s\n", periodsize, id,
				snd_strerror(err));
		return err;
	}
	return 0;
}

int setparams_set(snd_pcm_t *handle, snd_pcm_hw_params_t *params,
		snd_pcm_sw_params_t *swparams, const char *id) {
	int err;
	snd_pcm_uframes_t val;
	err = snd_pcm_hw_params(handle, params);
	if (err < 0) {
		printf("Unable to set hw params for %s: %s\n", id, snd_strerror(err));
		return err;
	}
	err = snd_pcm_sw_params_current(handle, swparams);
	if (err < 0) {
		printf("Unable to determine current swparams for %s: %s\n", id,
				snd_strerror(err));
		return err;
	}
	err = snd_pcm_sw_params_set_start_threshold(handle, swparams, 0x7fffffff);
	if (err < 0) {
		printf("Unable to set start threshold mode for %s: %s\n", id,
				snd_strerror(err));
		return err;
	}
	if (!block)
		val = 4;
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

// Set the parameters for the capture and playback streams
int setparams(snd_pcm_t *phandle, snd_pcm_t *chandle, int *bufsize) {
	int err, last_bufsize = *bufsize;
	snd_pcm_hw_params_t *pt_params, *ct_params; /* templates with rate, format and channels */
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
		printf("Unable to set parameters for playback stream: %s\n",
				snd_strerror(err));
		exit(0);
	}
	if ((err = setparams_stream(chandle, ct_params, "capture")) < 0) {
		printf("Unable to set parameters for playback stream: %s\n",
				snd_strerror(err));
		exit(0);
	}
	if (buffer_size > 0) {
		*bufsize = buffer_size;
		goto __set_it;
	}
	__again: if (buffer_size > 0)
		return -1;
	if (last_bufsize == *bufsize)
		*bufsize += 4;
	last_bufsize = *bufsize;
	if (*bufsize > latency_max)
		return -1;
	__set_it: if ((err = setparams_bufsize(phandle, p_params, pt_params,
			*bufsize, "playback")) < 0) {
		printf("Unable to set sw parameters for playback stream: %s\n",
				snd_strerror(err));
		exit(0);
	}
	if ((err = setparams_bufsize(chandle, c_params, ct_params, *bufsize,
			"capture")) < 0) {
		printf("Unable to set sw parameters for playback stream: %s\n",
				snd_strerror(err));
		exit(0);
	}
	snd_pcm_hw_params_get_period_size(p_params, &p_psize, NULL);
	if (p_psize > (unsigned int) *bufsize)
		*bufsize = p_psize;
	snd_pcm_hw_params_get_period_size(c_params, &c_psize, NULL);
	if (c_psize > (unsigned int) *bufsize)
		*bufsize = c_psize;
	snd_pcm_hw_params_get_period_time(p_params, &p_time, NULL);
	snd_pcm_hw_params_get_period_time(c_params, &c_time, NULL);
	if (p_time != c_time)
		goto __again;
	snd_pcm_hw_params_get_buffer_size(p_params, &p_size);
	if (p_psize * 2 > p_size) {
		snd_pcm_hw_params_get_periods_min(p_params, &val, NULL);
		if (val > 2) {
			printf("playback device does not support 2 periods per buffer\n");
			exit(0);
		}
		goto __again;
	}
	snd_pcm_hw_params_get_buffer_size(c_params, &c_size);
	if (c_psize * 2 > c_size) {
		snd_pcm_hw_params_get_periods_min(c_params, &val, NULL);
		if (val > 2) {
			printf("capture device does not support 2 periods per buffer\n");
			exit(0);
		}
		goto __again;
	}
	if ((err = setparams_set(phandle, p_params, p_swparams, "playback")) < 0) {
		printf("Unable to set sw parameters for playback stream: %s\n",
				snd_strerror(err));
		exit(0);
	}
	if ((err = setparams_set(chandle, c_params, c_swparams, "capture")) < 0) {
		printf("Unable to set sw parameters for playback stream: %s\n",
				snd_strerror(err));
		exit(0);
	}
	if ((err = snd_pcm_prepare(phandle)) < 0) {
		printf("Prepare error: %s\n", snd_strerror(err));
		exit(0);
	}
	//snd_pcm_dump(phandle, output);
	//snd_pcm_dump(chandle, output);
	fflush(stdout);
	return 0;
}

// Set the scheduler to the highest priority
void setscheduler(void) {
	struct sched_param sched_param;
	if (sched_getparam(0, &sched_param) < 0) {
		printf("Scheduler getparam failed...\n");
		return;
	}
	sched_param.sched_priority = sched_get_priority_max(SCHED_RR);
	if (!sched_setscheduler(0, SCHED_RR, &sched_param)) {
		printf("Scheduler set to Round Robin with priority %i...\n",
				sched_param.sched_priority);
		fflush(stdout);
		return;
	}
	printf("!!!Scheduler set to Round Robin with priority %i FAILED!!!\n",
			sched_param.sched_priority);
}

// Read from the audio capture device
long readbuf(snd_pcm_t *handle, char *buf, long len, size_t *frames) {
	long r;
	if (!block) {
		do {
			r = snd_pcm_readi(handle, buf, len);
		} while (r == -EAGAIN);
		if (r > 0) {
			*frames += r;
		}
		// printf("read = %li\n", r);
	} else {
		int frame_bytes = (snd_pcm_format_width(format) / 8) * channels;
		do {
			r = snd_pcm_readi(handle, buf, len);
			if (r > 0) {
				buf += r * frame_bytes;
				len -= r;
				*frames += r;
			}
			// printf("r = %li, len = %li\n", r, len);
		} while (r >= 1 && len > 0);
	}
	// showstat(handle, 0);
	return r;
}

// Write to the audio playback device
long writebuf(snd_pcm_t *handle, char *buf, long len, size_t *frames) {
	long r;
	while (len > 0) {
		r = snd_pcm_writei(handle, buf, len);
		if (r == -EAGAIN)
			continue;
		// printf("write = %li\n", r);
		if (r < 0)
			return r;
		// showstat(handle, 0);
		buf += r * 4;
		len -= r;
		*frames += r;
	}
	return 0;
}

void *mainLoop(void *data) {

	snd_pcm_t *phandle, *chandle;
	int err, latency;
	int ok;
	ssize_t r;
	size_t frames_in, frames_out;

	char *buffer;

	err = snd_output_stdio_attach(&output, stdout, 0);
	if (err < 0) {
		printf("Output failed: %s\n", snd_strerror(err));
		return 0;
	}

	// Set latency
	latency = latency_min - 4;

	// Allocate buffers
	buffer = malloc((latency_max * snd_pcm_format_width(format) / 8) * 2);

	// Set the priority of the audio thread and use round-robin scheduler
	setscheduler();

	// Print stats
	printf("Playback device is %s\n", pdevice);
	printf("Capture device is %s\n", cdevice);
	printf("Parameters are %iHz, %s, %i channels, %s mode\n", rate,
			snd_pcm_format_name(format), channels,
			block ? "blocking" : "non-blocking");
	printf("Poll mode: %s\n", use_poll ? "yes" : "no");

	// Open capture and playback devices
	if ((err = snd_pcm_open(&phandle, pdevice, SND_PCM_STREAM_PLAYBACK,
			block ? 0 : SND_PCM_NONBLOCK)) < 0) {
		printf("Playback open error: %s\n", snd_strerror(err));
		return 0;
	}
	if ((err = snd_pcm_open(&chandle, cdevice, SND_PCM_STREAM_CAPTURE,
			block ? 0 : SND_PCM_NONBLOCK)) < 0) {
		printf("Record open error: %s\n", snd_strerror(err));
		return 0;
	}

	while (!should_stop) {
		frames_in = frames_out = 0;
		if (setparams(phandle, chandle, &latency) < 0)
			break;
		if ((err = snd_pcm_link(chandle, phandle)) < 0) {
			printf("Streams link error: %s\n", snd_strerror(err));
			exit(0);
		}
		if (snd_pcm_format_set_silence(format, buffer, latency * channels)
				< 0) {
			fprintf(stderr, "silence error\n");
			break;
		}
		if (writebuf(phandle, buffer, latency, &frames_out) < 0) {
			fprintf(stderr, "write error\n");
			break;
		}
		if (writebuf(phandle, buffer, latency, &frames_out) < 0) {
			fprintf(stderr, "write error\n");
			break;
		}
		if ((err = snd_pcm_start(chandle)) < 0) {
			printf("Go error: %s\n", snd_strerror(err));
			exit(0);
		}

		ok = 1;
		while (ok && !should_stop) {
			if (use_poll) {
				/* use poll to wait for next event */
				snd_pcm_wait(chandle, 1000);
			}
			if ((r = readbuf(chandle, buffer, latency, &frames_in)) < 0)
				ok = 0;
			else {
				short *shortBuffer = (short *) buffer;

				process(shortBuffer, r, channels, rate);

				if (writebuf(phandle, buffer, r, &frames_out) < 0)
					ok = 0;
			}
		}

		snd_pcm_drop(chandle);
		snd_pcm_nonblock(phandle, 0);
		snd_pcm_drain(phandle);
		snd_pcm_nonblock(phandle, !block ? 1 : 0);

		if (ok) {
			break;
		}
		printf("Underrun; trying again\n");
		snd_pcm_unlink(chandle);
		snd_pcm_hw_free(phandle);
		snd_pcm_hw_free(chandle);
	}

	snd_pcm_close(phandle);
	snd_pcm_close(chandle);
	free(buffer);

	pthread_exit(NULL);

}

int main(int argc, char *argv[]) {

	pthread_t inputThread, processingThread;

	// Set up interrupt handler to catch Control-C
	signal(SIGINT, interruptHandler);

	inputInitialise();
	displayInitialise();

	memcpy(displayBuffer, &menuBuffer[0], 1024);

	pthread_create(&inputThread, NULL, inputCheck, NULL);
	pthread_create(&processingThread, NULL, mainLoop, NULL);

	(void) pthread_join(processingThread, NULL);
	(void) pthread_join(inputThread, NULL);

	clearDisplay();
	inputCleanup();

	printf("End Program\n");
	return 0;

}
