#include "audio.h"
#include <SDL3/SDL.h>
#include <stdlib.h>

struct HP_AudioStream_Opaque {
    SDL_AudioStream *sdlStream;
};

HP_AudioStream hp_audio_open(int sampleRate, int channels)
{
    SDL_AudioSpec spec = { SDL_AUDIO_S16LE, channels, sampleRate };
    SDL_AudioStream *sdlStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    if (!sdlStream) {
        return NULL;
    }

    SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(sdlStream));

    HP_AudioStream stream = (HP_AudioStream)malloc(sizeof(struct HP_AudioStream_Opaque));
    if (!stream) {
        SDL_DestroyAudioStream(sdlStream);
        return NULL;
    }
    stream->sdlStream = sdlStream;
    return stream;
}

void hp_audio_close(HP_AudioStream stream)
{
    if (stream) {
        if (stream->sdlStream) {
            SDL_DestroyAudioStream(stream->sdlStream);
        }
        free(stream);
    }
}

void hp_audio_clear(HP_AudioStream stream)
{
    if (stream && stream->sdlStream) {
        SDL_ClearAudioStream(stream->sdlStream);
    }
}

int hp_audio_get_queued_frames(HP_AudioStream stream)
{
    if (stream && stream->sdlStream) {
        // 16-bit stereo = 2 channels * 2 bytes = 4 bytes per frame
        return SDL_GetAudioStreamQueued(stream->sdlStream) / 4;
    }
    return 0;
}

bool hp_audio_write(HP_AudioStream stream, const void *data, int frames)
{
    if (stream && stream->sdlStream) {
        // 16-bit stereo = 2 channels * 2 bytes = 4 bytes per frame
        return SDL_PutAudioStreamData(stream->sdlStream, data, frames * 4);
    }
    return false;
}
