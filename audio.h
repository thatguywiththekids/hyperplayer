#ifndef AUDIO_H
#define AUDIO_H

#include <stdbool.h>

typedef struct HP_AudioStream_Opaque* HP_AudioStream;

HP_AudioStream hp_audio_open(int sampleRate, int channels);
void hp_audio_close(HP_AudioStream stream);
void hp_audio_clear(HP_AudioStream stream);
int hp_audio_get_queued_frames(HP_AudioStream stream);
bool hp_audio_write(HP_AudioStream stream, const void *data, int frames);

#endif // AUDIO_H
