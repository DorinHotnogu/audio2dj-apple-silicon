#pragma once
#include <mach/mach.h>
#include <servers/bootstrap.h>
#include <stdint.h>
#define A2_SERVICE "local.audio2dj.bridge"
#define A2_FRAMES 1024
#define A2_MESSAGE_ID 0x4132444a
// Versioned, fixed-size, non-complex one-way PCM message: stereo float32 at 48 kHz.
typedef struct { mach_msg_header_t header; uint32_t version; uint32_t frames; uint32_t sampleRate; uint32_t pair; float samples[A2_FRAMES*2]; } A2Message;
