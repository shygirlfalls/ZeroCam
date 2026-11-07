#pragma once

#include <stdbool.h>
#include <stdint.h>

#define BACKDROP_FRAME_WIDTH 400
#define BACKDROP_FRAME_HEIGHT 240
#define BACKDROP_SCALE 4
#define BACKDROP_WIDTH (BACKDROP_FRAME_WIDTH / BACKDROP_SCALE)
#define BACKDROP_HEIGHT (BACKDROP_FRAME_HEIGHT / BACKDROP_SCALE)
#define BACKDROP_PIXELS (BACKDROP_WIDTH * BACKDROP_HEIGHT)

typedef struct {
    uint8_t reference[BACKDROP_PIXELS][3];
    uint8_t rawMask[BACKDROP_PIXELS];
    uint8_t mask[BACKDROP_PIXELS];
    bool calibrated;
    bool maskReady;
} BackdropMask;

void backdropCapture(BackdropMask *mask, const uint16_t *frame, int left, int top,
                     int width, int height, bool mirrored);
void backdropUpdate(BackdropMask *mask, const uint16_t *frame, int left, int top,
                    int width, int height, bool mirrored, int threshold);

static inline uint8_t backdropAlpha(const BackdropMask *mask, int x, int y)
{
    int gridX = x / BACKDROP_SCALE;
    int gridY = y / BACKDROP_SCALE;
    int nextX = gridX + 1 < BACKDROP_WIDTH ? gridX + 1 : gridX;
    int nextY = gridY + 1 < BACKDROP_HEIGHT ? gridY + 1 : gridY;
    int fractionX = x % BACKDROP_SCALE;
    int fractionY = y % BACKDROP_SCALE;
    int upper = mask->mask[gridY * BACKDROP_WIDTH + gridX] * (BACKDROP_SCALE - fractionX) +
                mask->mask[gridY * BACKDROP_WIDTH + nextX] * fractionX;
    int lower = mask->mask[nextY * BACKDROP_WIDTH + gridX] * (BACKDROP_SCALE - fractionX) +
                mask->mask[nextY * BACKDROP_WIDTH + nextX] * fractionX;
    return (uint8_t)((upper * (BACKDROP_SCALE - fractionY) + lower * fractionY) /
                     (BACKDROP_SCALE * BACKDROP_SCALE));
}