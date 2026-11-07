#include "face_tracker.h"
#include "picornt.h"

#define TRACK_WIDTH 100
#define TRACK_HEIGHT 60
#define TRACK_FRAME_WIDTH 400
#define TRACK_FRAME_HEIGHT 240
#define TRACK_MAX_DETECTIONS 64

extern const u8 facefinder_bin[];

typedef struct {
    Thread thread;
    LightLock lock;
    LightEvent event;
    u8 pixels[TRACK_WIDTH * TRACK_HEIGHT];
    u64 submittedTick;
    u64 lastSubmitTick;
    u64 lastSeenTick;
    u32 generation;
    u32 submittedGeneration;
    int cropLeft;
    int cropTop;
    int cropWidth;
    int cropHeight;
    bool mirrored;
    bool initialized;
    bool attempted;
    bool pending;
    bool busy;
    bool stopping;
    bool valid;
    float centerX;
    float centerY;
    float size;
} FaceTracker;

static FaceTracker tracker;

static void faceTrackerWorker(void *argument)
{
    (void)argument;
    for (;;) {
        LightEvent_Wait(&tracker.event);
        LightLock_Lock(&tracker.lock);
        if (tracker.stopping) {
            LightLock_Unlock(&tracker.lock);
            break;
        }
        if (!tracker.pending) {
            LightLock_Unlock(&tracker.lock);
            continue;
        }
        tracker.pending = false;
        tracker.busy = true;
        u32 generation = tracker.submittedGeneration;
        u64 frameTick = tracker.submittedTick;
        LightLock_Unlock(&tracker.lock);

        float detections[TRACK_MAX_DETECTIONS * 4];
        int count = find_objects(detections, TRACK_MAX_DETECTIONS, (void *)facefinder_bin,
                                 0.0f, tracker.pixels, TRACK_HEIGHT, TRACK_WIDTH, TRACK_WIDTH,
                                 1.2f, 0.15f, 16.0f, 58.0f);
        count = cluster_detections(detections, count);
        int best = -1;
        float bestScore = 4.0f;
        for (int index = 0; index < count; index++) {
            float score = detections[index * 4 + 3];
            if (score > bestScore) {
                best = index;
                bestScore = score;
            }
        }

        LightLock_Lock(&tracker.lock);
        if (best >= 0 && generation == tracker.generation && !tracker.stopping) {
            float centerX = detections[best * 4 + 1] * 4.0f;
            float centerY = detections[best * 4] * 4.0f;
            float size = detections[best * 4 + 2] * 4.0f;
            if (tracker.valid && frameTick - tracker.lastSeenTick < (u64)SYSCLOCK_ARM11 * 2) {
                tracker.centerX = tracker.centerX * 0.6f + centerX * 0.4f;
                tracker.centerY = tracker.centerY * 0.6f + centerY * 0.4f;
                tracker.size = tracker.size * 0.6f + size * 0.4f;
            } else {
                tracker.centerX = centerX;
                tracker.centerY = centerY;
                tracker.size = size;
            }
            tracker.lastSeenTick = frameTick;
            tracker.valid = true;
        }
        tracker.busy = false;
        LightLock_Unlock(&tracker.lock);
    }
}

static bool ensureFaceTracker(void)
{
    if (tracker.attempted) return tracker.initialized;
    tracker.attempted = true;
    LightLock_Init(&tracker.lock);
    LightEvent_Init(&tracker.event, RESET_ONESHOT);
    tracker.thread = threadCreate(faceTrackerWorker, NULL, 64 * 1024, 0x31, 0, false);
    tracker.initialized = tracker.thread != NULL;
    return tracker.initialized;
}

bool faceTrackerSubmit(const u16 *frame, int cropLeft, int cropTop, int cropWidth,
                       int cropHeight, bool mirrored)
{
    if (!frame || !ensureFaceTracker()) return false;
    u64 now = svcGetSystemTick();
    LightLock_Lock(&tracker.lock);
    if (tracker.cropLeft != cropLeft || tracker.cropTop != cropTop ||
        tracker.cropWidth != cropWidth || tracker.cropHeight != cropHeight || tracker.mirrored != mirrored) {
        tracker.generation++;
        tracker.valid = false;
        tracker.cropLeft = cropLeft;
        tracker.cropTop = cropTop;
        tracker.cropWidth = cropWidth;
        tracker.cropHeight = cropHeight;
        tracker.mirrored = mirrored;
    }
    if (tracker.busy || tracker.pending ||
        (tracker.lastSubmitTick && now - tracker.lastSubmitTick < SYSCLOCK_ARM11 / 4)) {
        LightLock_Unlock(&tracker.lock);
        return true;
    }
    for (int y = 0; y < TRACK_HEIGHT; y++) {
        int sourceY = cropTop + (y * 4 + 2) * cropHeight / TRACK_FRAME_HEIGHT;
        for (int x = 0; x < TRACK_WIDTH; x++) {
            int sourceX = cropLeft + (x * 4 + 2) * cropWidth / TRACK_FRAME_WIDTH;
            if (mirrored) sourceX = TRACK_FRAME_WIDTH - 1 - sourceX;
            u16 pixel = frame[sourceY * TRACK_FRAME_WIDTH + sourceX];
            int red = ((pixel >> 11) & 31) * 255 / 31;
            int green = ((pixel >> 5) & 63) * 255 / 63;
            int blue = (pixel & 31) * 255 / 31;
            tracker.pixels[y * TRACK_WIDTH + x] = (u8)((red * 77 + green * 150 + blue * 29) >> 8);
        }
    }
    tracker.submittedGeneration = tracker.generation;
    tracker.submittedTick = now;
    tracker.lastSubmitTick = now;
    tracker.pending = true;
    LightLock_Unlock(&tracker.lock);
    LightEvent_Signal(&tracker.event);
    return true;
}

bool faceTrackerGet(float *centerX, float *centerY, float *size)
{
    if (!tracker.initialized) return false;
    u64 now = svcGetSystemTick();
    LightLock_Lock(&tracker.lock);
    bool valid = tracker.valid && now - tracker.lastSeenTick < (u64)SYSCLOCK_ARM11 * 2;
    if (valid) {
        *centerX = tracker.centerX;
        *centerY = tracker.centerY;
        *size = tracker.size;
    }
    LightLock_Unlock(&tracker.lock);
    return valid;
}

void faceTrackerInvalidate(void)
{
    if (!tracker.initialized) return;
    LightLock_Lock(&tracker.lock);
    tracker.generation++;
    tracker.valid = false;
    LightLock_Unlock(&tracker.lock);
}

void faceTrackerShutdown(void)
{
    if (!tracker.initialized) return;
    LightLock_Lock(&tracker.lock);
    tracker.stopping = true;
    LightLock_Unlock(&tracker.lock);
    LightEvent_Signal(&tracker.event);
    threadJoin(tracker.thread, U64_MAX);
    threadFree(tracker.thread);
    tracker.thread = NULL;
    tracker.initialized = false;
}