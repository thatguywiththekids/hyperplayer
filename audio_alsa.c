#include "audio.h"
#include <alsa/asoundlib.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include <time.h>

struct HP_AudioStream_Opaque {
    snd_pcm_t *pcm;
    int rate;
    int channels;
    
    // Ring buffer (16-bit interleaved PCM frames)
    int16_t *ring_buffer;
    int capacity_frames;
    int read_pos;
    int write_pos;
    int queued_frames;
    
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    pthread_t thread;
    volatile bool running;
};

static void* alsa_playback_thread(void *arg)
{
    HP_AudioStream stream = (HP_AudioStream)arg;
    int16_t *temp_buf = malloc(1024 * stream->channels * sizeof(int16_t));
    if (!temp_buf) return NULL;

    while (stream->running) {
        pthread_mutex_lock(&stream->mutex);
        while (stream->queued_frames == 0 && stream->running) {
            pthread_cond_wait(&stream->cond, &stream->mutex);
        }
        if (!stream->running) {
            pthread_mutex_unlock(&stream->mutex);
            break;
        }

        int frames_to_write = stream->queued_frames;
        if (frames_to_write > 1024) frames_to_write = 1024;

        // Copy frames from circular ring buffer to contiguous temp buffer
        for (int i = 0; i < frames_to_write; ++i) {
            int idx = (stream->read_pos + i) % stream->capacity_frames;
            for (int c = 0; c < stream->channels; ++c) {
                temp_buf[i * stream->channels + c] = stream->ring_buffer[idx * stream->channels + c];
            }
        }
        pthread_mutex_unlock(&stream->mutex);

        // Perform blocking write to ALSA device in this background thread
        snd_pcm_sframes_t written = snd_pcm_writei(stream->pcm, temp_buf, frames_to_write);
        if (written < 0) {
            if (written == -EPIPE) {
                snd_pcm_prepare(stream->pcm);
                written = snd_pcm_writei(stream->pcm, temp_buf, frames_to_write);
            } else {
                // Sleep for 10ms on other errors to avoid busy spinning
                struct timespec ts = {0, 10000000};
                nanosleep(&ts, NULL);
            }
        }

        pthread_mutex_lock(&stream->mutex);
        if (written > 0) {
            stream->read_pos = (stream->read_pos + written) % stream->capacity_frames;
            stream->queued_frames -= written;
        } else {
            // Discard frames if write failed completely to avoid blocking the queue
            stream->read_pos = (stream->read_pos + frames_to_write) % stream->capacity_frames;
            stream->queued_frames -= frames_to_write;
        }
        pthread_mutex_unlock(&stream->mutex);
    }

    free(temp_buf);
    return NULL;
}

HP_AudioStream hp_audio_open(int sampleRate, int channels)
{
    snd_pcm_t *pcm = NULL;
    int err = snd_pcm_open(&pcm, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) {
        return NULL;
    }

    err = snd_pcm_set_params(pcm,
                             SND_PCM_FORMAT_S16_LE,
                             SND_PCM_ACCESS_RW_INTERLEAVED,
                             channels,
                             sampleRate,
                             1,       // soft-resample
                             100000); // 100ms hardware device buffer
    if (err < 0) {
        snd_pcm_close(pcm);
        return NULL;
    }

    HP_AudioStream stream = (HP_AudioStream)malloc(sizeof(struct HP_AudioStream_Opaque));
    if (!stream) {
        snd_pcm_close(pcm);
        return NULL;
    }

    stream->pcm = pcm;
    stream->rate = sampleRate;
    stream->channels = channels;
    
    // Allocate 1-second ring buffer capacity to comfortably absorb spikes
    stream->capacity_frames = sampleRate; 
    stream->ring_buffer = malloc(stream->capacity_frames * channels * sizeof(int16_t));
    if (!stream->ring_buffer) {
        free(stream);
        snd_pcm_close(pcm);
        return NULL;
    }

    stream->read_pos = 0;
    stream->write_pos = 0;
    stream->queued_frames = 0;
    stream->running = true;

    pthread_mutex_init(&stream->mutex, NULL);
    pthread_cond_init(&stream->cond, NULL);

    if (pthread_create(&stream->thread, NULL, alsa_playback_thread, stream) != 0) {
        pthread_mutex_destroy(&stream->mutex);
        pthread_cond_destroy(&stream->cond);
        free(stream->ring_buffer);
        free(stream);
        snd_pcm_close(pcm);
        return NULL;
    }

    return stream;
}

void hp_audio_close(HP_AudioStream stream)
{
    if (stream) {
        stream->running = false;
        pthread_mutex_lock(&stream->mutex);
        pthread_cond_signal(&stream->cond);
        pthread_mutex_unlock(&stream->mutex);

        pthread_join(stream->thread, NULL);

        pthread_mutex_destroy(&stream->mutex);
        pthread_cond_destroy(&stream->cond);

        if (stream->pcm) {
            snd_pcm_drain(stream->pcm);
            snd_pcm_close(stream->pcm);
        }
        if (stream->ring_buffer) {
            free(stream->ring_buffer);
        }
        free(stream);
    }
}

void hp_audio_clear(HP_AudioStream stream)
{
    if (stream) {
        pthread_mutex_lock(&stream->mutex);
        stream->read_pos = 0;
        stream->write_pos = 0;
        stream->queued_frames = 0;
        pthread_mutex_unlock(&stream->mutex);

        snd_pcm_drop(stream->pcm);
        snd_pcm_prepare(stream->pcm);
    }
}

int hp_audio_get_queued_frames(HP_AudioStream stream)
{
    if (!stream) return 0;
    pthread_mutex_lock(&stream->mutex);
    int queued = stream->queued_frames;
    pthread_mutex_unlock(&stream->mutex);
    return queued;
}

bool hp_audio_write(HP_AudioStream stream, const void *data, int frames)
{
    if (!stream || !stream->pcm || frames <= 0) return false;
    const int16_t *src = (const int16_t *)data;

    pthread_mutex_lock(&stream->mutex);
    
    int free_space = stream->capacity_frames - stream->queued_frames;
    if (frames > free_space) {
        pthread_mutex_unlock(&stream->mutex);
        return false;
    }

    for (int i = 0; i < frames; ++i) {
        int idx = (stream->write_pos + i) % stream->capacity_frames;
        for (int c = 0; c < stream->channels; ++c) {
            stream->ring_buffer[idx * stream->channels + c] = src[i * stream->channels + c];
        }
    }

    stream->write_pos = (stream->write_pos + frames) % stream->capacity_frames;
    stream->queued_frames += frames;

    pthread_cond_signal(&stream->cond);
    pthread_mutex_unlock(&stream->mutex);

    return true;
}
