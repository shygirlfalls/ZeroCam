#pragma once

#include <3ds.h>
#include <stdbool.h>

bool faceTrackerSubmit(const u16 *frame, int cropLeft, int cropTop, int cropWidth,
                       int cropHeight, bool mirrored);
bool faceTrackerGet(float *centerX, float *centerY, float *size);
void faceTrackerInvalidate(void);
void faceTrackerShutdown(void);