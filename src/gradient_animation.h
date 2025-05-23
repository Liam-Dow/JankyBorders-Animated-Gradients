#pragma once

#include "animation.h" // For struct animation
#include "border.h"    // For struct settings
#include <CoreVideo/CoreVideo.h> // For CVDisplayLinkRef, CVTimeStamp, CVOptionFlags, CVReturn, kCVReturnSuccess

// State for the gradient animation
struct gradient_animation_state {
    int current_interpolation_step;
    double time_accumulator_usec;    // Accumulates frame_time in microseconds
    double step_duration_usec;       // Calculated duration of one interpolation step in microseconds

    uint32_t current_tl_color;
    uint32_t current_br_color;
    uint32_t next_tl_color;
    uint32_t next_br_color;

    // Pointer to the color palette from g_settings
    uint32_t* color_palette; // This will point to g_settings.parsed_gradient_colors
    int num_palette_colors;
    int palette_total_steps; // This will be g_settings.animated_gradient_steps
};

// Initializes the gradient animation state and starts the animation if enabled
void gradient_animation_init_and_start(struct animation* animator,
                                       struct gradient_animation_state* anim_state,
                                       struct settings* settings);

// The CVDisplayLink callback function for gradient animation
CVReturn gradient_animation_callback(CVDisplayLinkRef displayLink,
                                     const CVTimeStamp* now,
                                     const CVTimeStamp* outputTime,
                                     CVOptionFlags flagsIn,
                                     CVOptionFlags* flagsOut,
                                     void* displayLinkContext);

// Stops the gradient animation
void gradient_animation_stop(struct animation* animator);
