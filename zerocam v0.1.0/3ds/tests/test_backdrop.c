#include "../source/backdrop.h"

#include <assert.h>
#include <stdio.h>

static uint16_t frame[BACKDROP_FRAME_WIDTH * BACKDROP_FRAME_HEIGHT];
static BackdropMask mask;

static void fillFrame(uint16_t color)
{
    for (int index = 0; index < BACKDROP_FRAME_WIDTH * BACKDROP_FRAME_HEIGHT; index++)
        frame[index] = color;
}

static void fillRectangle(int left, int top, int right, int bottom, uint16_t color)
{
    for (int y = top; y < bottom; y++)
        for (int x = left; x < right; x++)
            frame[y * BACKDROP_FRAME_WIDTH + x] = color;
}

int main(void)
{
    fillFrame(0);
    backdropCapture(&mask, frame, 0, 0, 400, 240, false);
    backdropUpdate(&mask, frame, 0, 0, 400, 240, false, 20);
    assert(mask.calibrated && mask.maskReady);
    assert(backdropAlpha(&mask, 100, 100) == 0);

    frame[(1 * BACKDROP_FRAME_WIDTH) + 1] = 0x1082;
    backdropUpdate(&mask, frame, 0, 0, 400, 240, false, 12);
    assert(backdropAlpha(&mask, 0, 0) == 0);

    fillFrame(0);
    fillFrame(0x2104);
    backdropUpdate(&mask, frame, 0, 0, 400, 240, false, 12);
    assert(backdropAlpha(&mask, 100, 100) > 240);

    fillFrame(0);
    fillRectangle(80, 60, 160, 140, 0xffff);
    backdropUpdate(&mask, frame, 0, 0, 400, 240, false, 20);
    int firstAlpha = backdropAlpha(&mask, 120, 100);
    assert(firstAlpha > 240);
    for (int iteration = 0; iteration < 6; iteration++)
        backdropUpdate(&mask, frame, 0, 0, 400, 240, false, 20);
    assert(backdropAlpha(&mask, 120, 100) > 240);
    assert(backdropAlpha(&mask, 80, 100) > 0 && backdropAlpha(&mask, 80, 100) < 255);
    assert(backdropAlpha(&mask, 220, 100) == 0);

    fillFrame(0);
    fillRectangle(200, 60, 280, 140, 0xffff);
    backdropUpdate(&mask, frame, 0, 0, 400, 240, false, 20);
    assert(backdropAlpha(&mask, 120, 100) < firstAlpha);
    assert(backdropAlpha(&mask, 240, 100) > 0);

    fillFrame(0);
    backdropCapture(&mask, frame, 100, 60, 200, 120, true);
    fillRectangle(260, 100, 280, 130, 0xffff);
    backdropUpdate(&mask, frame, 100, 60, 200, 120, true, 20);
    assert(backdropAlpha(&mask, 60, 100) > 200);
    assert(backdropAlpha(&mask, 340, 100) == 0);

    fillFrame(0);
    backdropCapture(&mask, frame, 0, 0, 400, 240, false);
    fillFrame(0x2104);
    backdropUpdate(&mask, frame, 0, 0, 400, 240, false, 68);
    assert(backdropAlpha(&mask, 100, 100) == 0);
    backdropUpdate(&mask, frame, 0, 0, 400, 240, false, 4);
    assert(backdropAlpha(&mask, 100, 100) > 0);
    puts("Backdrop mask tests passed");
    return 0;
}