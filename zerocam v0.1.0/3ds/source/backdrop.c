#include "backdrop.h"

#include <stdlib.h>

static void sampleCell(const uint16_t *frame, const int columns[2], const int rows[2], uint8_t color[3])
{
    int red = 0, green = 0, blue = 0;
    for (int row = 0; row < 2; row++) {
        for (int column = 0; column < 2; column++) {
            uint16_t pixel = frame[rows[row] * BACKDROP_FRAME_WIDTH + columns[column]];
            red += ((pixel >> 11) & 31) * 255 / 31;
            green += ((pixel >> 5) & 63) * 255 / 63;
            blue += (pixel & 31) * 255 / 31;
        }
    }
    color[0] = (uint8_t)(red / 4);
    color[1] = (uint8_t)(green / 4);
    color[2] = (uint8_t)(blue / 4);
}

static void sampleCoordinates(int left, int top, int width, int height, bool mirrored,
                              int columns[BACKDROP_WIDTH][2], int rows[BACKDROP_HEIGHT][2])
{
    for (int x = 0; x < BACKDROP_WIDTH; x++) {
        for (int sample = 0; sample < 2; sample++) {
            int position = x * BACKDROP_SCALE + 1 + sample * 2;
            int source = left + position * width / BACKDROP_FRAME_WIDTH;
            columns[x][sample] = mirrored ? BACKDROP_FRAME_WIDTH - 1 - source : source;
        }
    }
    for (int y = 0; y < BACKDROP_HEIGHT; y++) {
        for (int sample = 0; sample < 2; sample++) {
            int position = y * BACKDROP_SCALE + 1 + sample * 2;
            rows[y][sample] = top + position * height / BACKDROP_FRAME_HEIGHT;
        }
    }
}

void backdropCapture(BackdropMask *mask, const uint16_t *frame, int left, int top,
                     int width, int height, bool mirrored)
{
    int columns[BACKDROP_WIDTH][2], rows[BACKDROP_HEIGHT][2];
    sampleCoordinates(left, top, width, height, mirrored, columns, rows);
    for (int y = 0; y < BACKDROP_HEIGHT; y++)
        for (int x = 0; x < BACKDROP_WIDTH; x++)
            sampleCell(frame, columns[x], rows[y], mask->reference[y * BACKDROP_WIDTH + x]);
    mask->calibrated = true;
    mask->maskReady = false;
}

void backdropUpdate(BackdropMask *mask, const uint16_t *frame, int left, int top,
                    int width, int height, bool mirrored, int threshold)
{
    if (!mask->calibrated) return;
    int columns[BACKDROP_WIDTH][2], rows[BACKDROP_HEIGHT][2];
    sampleCoordinates(left, top, width, height, mirrored, columns, rows);
    for (int y = 0; y < BACKDROP_HEIGHT; y++) {
        for (int x = 0; x < BACKDROP_WIDTH; x++) {
            int index = y * BACKDROP_WIDTH + x;
            uint8_t color[3];
            sampleCell(frame, columns[x], rows[y], color);
            int difference = (abs((int)color[0] - mask->reference[index][0]) +
                              2 * abs((int)color[1] - mask->reference[index][1]) +
                              abs((int)color[2] - mask->reference[index][2])) / 4;
            int alpha = (difference - threshold) * 20;
            if (alpha < 0) alpha = 0;
            if (alpha > 255) alpha = 255;
            mask->rawMask[index] = (uint8_t)alpha;
        }
    }
    for (int y = 0; y < BACKDROP_HEIGHT; y++) {
        for (int x = 0; x < BACKDROP_WIDTH; x++) {
            int sum = 0, weightSum = 0;
            for (int offsetY = -1; offsetY <= 1; offsetY++) {
                int sampleY = y + offsetY;
                if (sampleY < 0 || sampleY >= BACKDROP_HEIGHT) continue;
                for (int offsetX = -1; offsetX <= 1; offsetX++) {
                    int sampleX = x + offsetX;
                    if (sampleX < 0 || sampleX >= BACKDROP_WIDTH) continue;
                    int weight = (offsetX == 0 ? 2 : 1) * (offsetY == 0 ? 2 : 1);
                    sum += mask->rawMask[sampleY * BACKDROP_WIDTH + sampleX] * weight;
                    weightSum += weight;
                }
            }
            int index = y * BACKDROP_WIDTH + x;
            int target = sum / weightSum;
            if (!mask->maskReady || target >= mask->mask[index])
                mask->mask[index] = (uint8_t)target;
            else
                mask->mask[index] = (uint8_t)((mask->mask[index] + target * 3) / 4);
        }
    }
    mask->maskReady = true;
}