#pragma once

#include <cstdint>
#include "libretro/libretro.h"


typedef void (*retro_video_refresh_t)(const void*, unsigned, unsigned, size_t);
typedef void (*retro_input_poll_t)(void);
typedef int16_t(*retro_input_state_t)(unsigned, unsigned, unsigned, unsigned);
typedef bool (*retro_environment_t)(unsigned, void*);
