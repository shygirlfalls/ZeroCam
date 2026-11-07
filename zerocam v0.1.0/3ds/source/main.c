#include <3ds.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <malloc.h>
#include <errno.h>
#include <setjmp.h>
#include <jpeglib.h>
#include <png.h>
#include "backdrop.h"
#include "face_tracker.h"
#include <sys/dirent.h>
#include <sys/stat.h>
#include <time.h>

#define FRAME_WIDTH 400
#define FRAME_HEIGHT 240
#define FRAME_PIXELS (FRAME_WIDTH * FRAME_HEIGHT)
#define FRAME_BYTES (FRAME_PIXELS * sizeof(u16))
#define LEGACY_DIRECTORY "sdmc:/3ds/ZeroCam"
#define PHOTO_DIRECTORY LEGACY_DIRECTORY "/Gallery"
#define GAMEBOY_FRAME_DIRECTORY LEGACY_DIRECTORY "/Frames"
#define GREEN_BACKGROUND_DIRECTORY LEGACY_DIRECTORY "/Backgrounds"
#define GREEN_BUILTIN_BACKGROUNDS 4
#define GREEN_MAX_IMPORTED_BACKGROUNDS 16
#define GREEN_BACKGROUND_PAGE_SIZE 4
#define GREEN_THUMB_WIDTH 26
#define GREEN_THUMB_HEIGHT 16
#define GREEN_LIST_LEFT 202
#define GREEN_LIST_TOP 52
#define GREEN_LIST_ROW_HEIGHT 20
#define GREEN_SLIDER_LEFT 75
#define GREEN_SLIDER_RIGHT 306
#define GREEN_SLIDER_TOP 157
#define GREEN_SLIDER_ROW_HEIGHT 13
#define GREEN_CAPTURE_LEFT 96
#define GREEN_CAPTURE_WIDTH 96
#define GREEN_CONTROL_LEFT 5
#define GREEN_CONTROL_TOP 205
#define GREEN_CONTROL_WIDTH 57
#define GREEN_CONTROL_PITCH 62
#define GREEN_CONTROL_HEIGHT 29
#define GAMEBOY_FRAME_WIDTH 160
#define GAMEBOY_FRAME_MAX_HEIGHT 224
#define GAMEBOY_MAX_IMPORTED_FRAMES 16
#define GAMEBOY_BUILTIN_FRAMES 3
#define GAMEBOY_CONTROL_LEFT 4
#define GAMEBOY_CONTROL_TOP 42
#define GAMEBOY_CONTROL_WIDTH 34
#define GAMEBOY_CONTROL_HEIGHT 20
#define GAMEBOY_CONTROL_PITCH 23
#define GAMEBOY_FRAME_TOP 160
#define GAMEBOY_SLIDER_TOP 180
#define GAMEBOY_SLIDER_HEIGHT 20
#define GAMEBOY_SLIDER_LEFT 74
#define GAMEBOY_SLIDER_RIGHT 306
#define MAX_GALLERY_ITEMS 60
#define MOV_MAX_FRAMES 450
#define MOV_FRAME_INTERVAL_MS 71
#define MOV_MAX_DURATION_MS 30000
#define MOV_WIDTH 288
#define MOV_HEIGHT 176
#define MOV_QUEUE_CAPACITY 8
#define MIC_BUFFER_SIZE 0x30000
#define MOV_AUDIO_CAPACITY 0x100000
#define MOV_AUDIO_RATE 16364
#define HEADER_TITLE_HEIGHT 14
#define HEADER_MENU_TOP HEADER_TITLE_HEIGHT
#define HEADER_MENU_HEIGHT 13
#define HEADER_STATUS_TOP (HEADER_MENU_TOP + HEADER_MENU_HEIGHT)
#define HEADER_STATUS_HEIGHT 9
#define MENU_POPUP_TOP HEADER_STATUS_TOP
#define TOOLBAR_TOP (PREVIEW_TOP + 1)
#define TOOLBAR_SLOT_HEIGHT 15
#define TOOLBAR_BUTTON_HEIGHT 12
#define TOOLBAR_TOOL_COUNT 8
#define SLIDER_TRACK_LEFT 22
#define SLIDER_TRACK_RIGHT 195
#define SLIDER_TOUCH_TOP 184
#define SLIDER_ROW_HEIGHT 17
#define SLIDER_COUNT 3
#define CIRCLE_ZOOM_DEADZONE 20
#define CIRCLE_ZOOM_MAX 156
#define CIRCLE_ZOOM_PER_SECOND 1.5f
#define FILTER_SWATCH_LEFT 70
#define FILTER_SWATCH_TOP 166
#define FILTER_SWATCH_PITCH 19
#define FILTER_SWATCH_ROW_HEIGHT 15
#define PREVIEW_LEFT 43
#define PREVIEW_TOP (HEADER_STATUS_TOP + HEADER_STATUS_HEIGHT + 3)
#define PREVIEW_WIDTH 270
#define PREVIEW_HEIGHT (158 - PREVIEW_TOP)
#define PAINT_TOUCH_MARGIN 2
#define DREAM_SCALE 4
#define BRUSH_COLOR_COUNT 8
#define GALLERY_COLUMNS 4
#define GALLERY_ROWS 2
#define GALLERY_PAGE_SIZE (GALLERY_COLUMNS * GALLERY_ROWS)
#define GALLERY_THUMB_SIZE 52
#define GALLERY_GRID_LEFT 15
#define GALLERY_GRID_TOP 58
#define GALLERY_CELL_WIDTH 75
#define GALLERY_CELL_HEIGHT 68
#define DRAW_HISTORY_LIMIT 4
#define EDIT_MENU_LEFT 42
#define EDIT_MENU_TOP (PREVIEW_TOP + 2)
#define EDIT_MENU_WIDTH 160
#define EDIT_MENU_ROW_HEIGHT 13
#define EDIT_MENU_ITEM_COUNT (FRAME_STYLE_COUNT + 2)
#define TIMER_MENU_LEFT 40
#define TIMER_MENU_TOP (TOOLBAR_TOP + 2 * TOOLBAR_SLOT_HEIGHT)
#define TIMER_MENU_WIDTH 100
#define TIMER_MENU_ROW_HEIGHT 14
#define TIMER_MENU_ITEM_COUNT 4
#define C32(r, g, b) ((u32)(r) | ((u32)(g) << 8) | ((u32)(b) << 16) | 0xff000000U)

typedef enum {
    FILTER_NORMAL,
    FILTER_XP_BLUE,
    FILTER_SEPIA,
    FILTER_MONO,
    FILTER_VIVID,
    FILTER_VHS,
    FILTER_THERMAL,
    FILTER_PIXEL,
    FILTER_NEGATIVE,
    FILTER_DREAM,
    FILTER_GAMEBOY,
    FILTER_HEARTS,
    FILTER_GREEN_SCREEN,
    FILTER_COUNT
} Filter;

typedef enum {
    FRAME_NONE,
    FRAME_HEARTS,
    FRAME_STARS,
    FRAME_FILM,
    FRAME_RAINBOW,
    FRAME_STYLE_COUNT
} FrameStyle;

static const char *filterNames[FILTER_COUNT] = {
    "Normal", "XP Blue", "Sepia", "Mono", "Vivid", "VHS", "Thermal",
    "Pixel", "Negative", "Dream", "Game Boy", "Hearts", "Cutout"
};
static const u16 brushColors[BRUSH_COLOR_COUNT] = {
    0x0000, 0x001f, 0x07e0, 0xf800, 0x07ff, 0xf81f, 0xffe0, 0xffff
};
static const u8 gameboyPalettes[4][4][3] = {
    {{15, 56, 15}, {48, 98, 48}, {139, 172, 15}, {203, 220, 141}},
    {{29, 43, 83}, {126, 37, 83}, {255, 119, 168}, {255, 241, 232}},
    {{53, 40, 121}, {111, 79, 37}, {184, 199, 111}, {255, 255, 255}},
    {{0, 0, 0}, {0, 255, 255}, {255, 85, 255}, {255, 255, 255}}
};
static const char *gameboyPaletteNames[4] = {"DMG", "PICO", "C64", "CGA"};
static int gameboyPaletteIndex;
static u8 gameboyFramePixels[GAMEBOY_FRAME_WIDTH * GAMEBOY_FRAME_MAX_HEIGHT * 4];
static u16 gameboyFrameMap[FRAME_PIXELS];
static char gameboyFramePaths[GAMEBOY_MAX_IMPORTED_FRAMES][128];
static size_t gameboyFrameCount;
static int gameboyFrameIndex;
static int gameboyFrameHeight;
static bool gameboyFrameLoaded;
static bool gameboyFramesScanned;
static bool isVideoFile(const char *path)
{
    const char *extension = strrchr(path, '.');
    return extension && strcmp(extension, ".mov") == 0;
}
static bool isGalleryMedia(const char *path)
{
    const char *extension = strrchr(path, '.');
    return extension && (strcmp(extension, ".jpg") == 0 || strcmp(extension, ".bmp") == 0 ||
                         strcmp(extension, ".mov") == 0);
}
typedef struct {
    const char *name;
    u8 red;
    u8 green;
    u8 blue;
} HudTheme;
static const HudTheme hudThemes[] = {
    {"BLUE", 24, 82, 177}, {"GREEN", 35, 103, 72},
    {"PINK", 163, 52, 111}, {"YELLOW", 137, 93, 0},
    {"RED", 150, 54, 54}, {"CHARCOAL", 68, 75, 84}
};
#define HUD_THEME_COUNT (sizeof(hudThemes) / sizeof(hudThemes[0]))
static size_t hudThemeIndex;

static u16 *cameraFrame;
static u16 processedFrame[FRAME_PIXELS];
static u16 greenScreenBackground[FRAME_PIXELS];
static u16 greenScreenScratch[FRAME_PIXELS];
static int greenScreenBackgroundIndex;
static bool greenScreenBackgroundReady;
static bool greenScreenBackgroundsScanned;
static int greenScreenThreshold = 10;
static BackdropMask backdropMask;
static int backdropCropWidth, backdropCropHeight;
static bool backdropMirrored;
static size_t greenScreenImportedCount;
static int greenScreenPageStart;
static char greenScreenPaths[GREEN_MAX_IMPORTED_BACKGROUNDS][128];
static u16 greenScreenThumbnails[GREEN_BUILTIN_BACKGROUNDS + GREEN_MAX_IMPORTED_BACKGROUNDS]
                                  [GREEN_THUMB_WIDTH * GREEN_THUMB_HEIGHT];
static bool greenScreenThumbnailValid[GREEN_BUILTIN_BACKGROUNDS + GREEN_MAX_IMPORTED_BACKGROUNDS];
static bool greenScreenThumbnailAttempted[GREEN_BUILTIN_BACKGROUNDS + GREEN_MAX_IMPORTED_BACKGROUNDS];
extern const u8 bg_beach_bin[], bg_beach_bin_end[];
extern const u8 bg_road_bin[], bg_road_bin_end[];
extern const u8 bg_earth_bin[], bg_earth_bin_end[];
extern const u8 bg_mountains_bin[], bg_mountains_bin_end[];
static const char *greenScreenBuiltinNames[GREEN_BUILTIN_BACKGROUNDS] = {
    "BEACH", "ROAD", "EARTH", "MOUNTAINS"
};
static void selectGreenScreenBackground(int index);
static bool scanGreenScreenBackgrounds(void);
static void updateGreenScreenThumbnails(void);
static u8 dreamBlur[FRAME_HEIGHT / DREAM_SCALE][FRAME_WIDTH / DREAM_SCALE][3];
static u8 dreamGlow[FRAME_HEIGHT / DREAM_SCALE][FRAME_WIDTH / DREAM_SCALE];
static u16 galleryFrame[FRAME_PIXELS];
static u16 galleryThumbnails[GALLERY_PAGE_SIZE][GALLERY_THUMB_SIZE * GALLERY_THUMB_SIZE];
static bool galleryThumbnailValid[GALLERY_PAGE_SIZE];
static size_t galleryPageStart = (size_t)-1;
static u16 drawOverlay[FRAME_PIXELS];
static u8 drawAlpha[FRAME_PIXELS];
typedef struct {
    u16 colors[FRAME_PIXELS];
    u8 alpha[FRAME_PIXELS];
} DrawingSnapshot;
static DrawingSnapshot undoHistory[DRAW_HISTORY_LIMIT];
static DrawingSnapshot redoHistory[DRAW_HISTORY_LIMIT];
static size_t undoCount;
static size_t redoCount;
static u16 drawColor565 = 0x001f;
static char galleryFiles[MAX_GALLERY_ITEMS][96];
static char lastPhotoPath[96];
static u32 cameraTransferBytes;
static u32 cameraSelect = SELECT_OUT1;
static bool cameraInitialized;
static bool cameraCapturing;
static bool cameraReady;
static bool mirrorPreview;
static bool showHud = true;
static bool timestampEnabled;
static bool drawEnabled;
static bool textPlacementPending;
static bool textColorPicking;
static char canvasText[33];
static bool brushPaletteOpen;
static bool eyedropperMode;
static bool galleryOpen;
static bool galleryImageLoaded;
static bool gallerySelected;
static int galleryDeleteMode;
static bool previousTouchValid;
static bool videoMode;
static bool menuOpen;
static bool editingMenuOpen;
static bool timerMenuOpen;
static unsigned captureTimerSeconds;
static u64 captureTimerDeadline;
static char captureTimerStatus[24];
static int timerMenuItem;
static int menuIndex;
static int menuItem;
static int editingMenuItem;
static bool eraserEnabled;
static float zoom = 1.0f;
static float brightness = 1.0f;
static float contrast = 1.0f;
static float saturation = 1.0f;
static float gameboyDither = 0.5f;
static Filter currentFilter = FILTER_NORMAL;
static FrameStyle currentFrame = FRAME_NONE;
static unsigned frameNumber;
static unsigned photoNumber = 1;
static unsigned videoNumber = 1;
static size_t galleryCount;
static size_t galleryIndex;
static const char *photoStatus = "";
static int previousTouchX;
static int previousTouchY;

typedef struct {
    u16 frame[FRAME_PIXELS];
    char path[96];
    Thread worker;
    u64 startTick;
    bool active;
    bool done;
    bool saved;
} PhotoSave;
static PhotoSave photoSave;

typedef struct {
    FILE *file;
    u32 offsets[MOV_MAX_FRAMES];
    u32 sizes[MOV_MAX_FRAMES];
    u32 durations[MOV_MAX_FRAMES];
    u32 frameCount;
    u32 frameIndex;
    u64 frameTick;
    u32 elapsedMs;
    u32 audioOffset;
    u32 audioBytes;
    u32 audioRate;
    u8 *audioData;
    ndspWaveBuf audioWave;
    bool audioReady;
    bool audioInvalid;
    bool playing;
    bool finished;
} GalleryVideo;
static GalleryVideo galleryVideo;

typedef struct {
    FILE *file;
    char path[96];
    long mdatOffset;
    u32 offsets[MOV_MAX_FRAMES];
    u32 sizes[MOV_MAX_FRAMES];
    u32 durations[MOV_MAX_FRAMES];
    u32 frameCount;
    u64 startTick;
    u64 firstFrameTick;
    u64 lastFrameTick;
    u64 lastQueuedTick;
    u64 frameTicks[MOV_QUEUE_CAPACITY];
    u16 frameBuffers[MOV_QUEUE_CAPACITY][FRAME_PIXELS];
    unsigned queueHead;
    unsigned queueTail;
    unsigned queueCount;
    Thread worker;
    LightLock lock;
    LightEvent event;
    u32 previousCpuLimit;
    bool cpuLimitChanged;
    bool busy;
    bool stopRequested;
    bool failed;
    u8 *micBuffer;
    u8 *audioData;
    u32 micDataSize;
    u32 micReadPos;
    u32 audioBytes;
    u32 audioOffset;
    u64 audioStartTick;
    bool micSampling;
    bool audioFailed;
    bool active;
} MovieWriter;

static MovieWriter movie;
#if 0
    u8 *framebuffer = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);
    bottomFill(framebuffer, 0, 0, 320, 240, 212, 208, 200);
    bottomFill(framebuffer, 0, 0, 320, 18, 24, 82, 177);
    bottomText(framebuffer, 7, 5, galleryOpen ? "ZERO CAM GALLERY" : "ZERO CAM UNTITLED", 255, 255, 255);
    bottomFill(framebuffer, 0, 20, 320, 18, 236, 233, 216);
    bottomText(framebuffer, 7, 24, menuOpen && menuIndex == 0 ? "FILE V" : "FILE", 20, 24, 34);
    bottomText(framebuffer, 58, 24, menuOpen && menuIndex == 1 ? "EDIT V" : "EDIT", 20, 24, 34);
    bottomText(framebuffer, 112, 24, menuOpen && menuIndex == 2 ? "EXTRAS V" : "EXTRAS", 20, 24, 34);
    bottomText(framebuffer, 260, 24, "GALLERY", 25, 83, 180);

    if (galleryOpen) {
        bottomFill(framebuffer, 4, 42, 312, 174, 128, 128, 128);
        bottomFill(framebuffer, 6, 44, 308, 170, 6, 9, 17);
        bottomText(framebuffer, 86, 116, galleryCount ? "PHOTO GALLERY" : "GALLERY EMPTY", 245, 245, 245);
        bottomText(framebuffer, 8, 226, "D PAD BROWSE   B BACK", 20, 24, 34);
        return;
    }

    if (menuOpen) {
        bottomFill(framebuffer, 3, 41, 101, 48, 128, 128, 128);
        bottomFill(framebuffer, 4, 42, 99, 46, 236, 233, 216);
        if (menuIndex == 0) {
            bottomText(framebuffer, 8, 47, "SAVE CURRENT IMAGE", 20, 24, 34);
            bottomText(framebuffer, 8, 66, "BACK", 20, 24, 34);
        } else if (menuIndex == 1) {
            bottomText(framebuffer, 8, 47, "FILTER", 20, 24, 34);
            bottomText(framebuffer, 8, 66, "BACK", 20, 24, 34);
        } else {
            bottomText(framebuffer, 8, 47, "HUD / MIRROR", 20, 24, 34);
            bottomText(framebuffer, 8, 66, "CAMERA / BACK", 20, 24, 34);
        }
    }

    bottomFill(framebuffer, 3, 40, 314, 13, 225, 222, 214);
    char status[48];
    snprintf(status, sizeof(status), "%u SAVED   %s CAMERA   %s", (unsigned)galleryCount,
             cameraSelect == SELECT_OUT1 ? "OUTER" : "INNER",
             movie.active ? "REC" : cameraReady ? "LIVE" : "DEMO");
    bottomText(framebuffer, 7, 43, status, 20, 24, 34);

    bottomFill(framebuffer, 3, 55, 34, 111, 128, 128, 128);
    bottomFill(framebuffer, 4, 56, 32, 109, 236, 233, 216);
    bottomFill(framebuffer, 40, 55, 278, 113, 80, 80, 80);
    bottomFill(framebuffer, 42, 57, 274, 109, 0, 0, 0);
    drawBottomToolIcons(framebuffer);
    renderBottomPreview(framebuffer, cameraReady);

    bottomText(framebuffer, 7, 169, filterNames[currentFilter], 65, 65, 75);
    bottomFill(framebuffer, 3, 179, 314, 17, 236, 233, 216);
    for (int index = 0; index < FILTER_COUNT; index++) {
        int x = 7 + index * 25;
        u32 swatch = filterColors[index];
        bottomFill(framebuffer, x, 181, 18, 13, 65, 65, 65);
        bottomFill(framebuffer, x + 1, 182, 16, 11,
                   (u8)(swatch & 0xff), (u8)((swatch >> 8) & 0xff), (u8)((swatch >> 16) & 0xff));
        if (index == currentFilter) bottomRect(framebuffer, x - 1, 180, 20, 15, 25);
    }

    bottomFill(framebuffer, 3, 198, 314, 27, 236, 233, 216);
    for (int slider = 0; slider < 3; slider++) {
        float value = slider == 0 ? brightness : slider == 1 ? contrast : saturation;
        int y = 201 + slider * 7;
        int knob = 58 + (int)((value - 0.5f) * 170.0f / 1.5f);
        bottomText(framebuffer, 7, y, slider == 0 ? "B" : slider == 1 ? "C" : "S", 25, 45, 80);
        bottomFill(framebuffer, 22, y + 3, 182, 1, 130, 130, 130);
        bottomFill(framebuffer, knob - 2, y + 1, 5, 5, 40, 105, 210);
    }
    bottomFill(framebuffer, 252, 199, 63, 24, 110, 110, 110);
    bottomFill(framebuffer, 254, 201, 59, 20, 236, 233, 216);
    bottomRect(framebuffer, 271, 206, 24, 10, 25);
    bottomCircle(framebuffer, 283, 211, 3, 25);
    bottomText(framebuffer, 7, 229, videoMode ? "MOV" : "JPG", 20, 24, 34);
    bottomText(framebuffer, 75, 229, "ZOOM", 20, 24, 34);
    bottomText(framebuffer, 157, 229, "MEM", 20, 24, 34);
    }

    bottomFill(framebuffer, 3, 47, 34, 116, 128, 128, 128);
    bottomFill(framebuffer, 4, 48, 32, 114, 236, 233, 216);
    for (int icon = 0; icon < 8; icon++)
        bottomRect(framebuffer, 5, 49 + icon * 14, 30, 12, 100);
    drawBottomToolIcons(framebuffer);
    renderBottomPreview(framebuffer, cameraReady);

    bottomText(framebuffer, 42, 162, filterNames[currentFilter], 65, 65, 75);
    bottomFill(framebuffer, 3, 164, 314, 17, 236, 233, 216);
    static const u32 swatches[FILTER_COUNT] = {
        C32(238,238,238), C32(80,145,235), C32(172,125,80), C32(128,128,128),
        C32(245,70,70), C32(20,200,55), C32(120,115,150), C32(255,115,35),
        C32(150,90,230), C32(20,20,20), C32(245,165,205), C32(30,240,145)
    };
    for (int index = 0; index < FILTER_COUNT; index++) {
        int x = 7 + index * 25;
        bottomFill(framebuffer, x, 166, 18, 13, 65, 65, 65);
        bottomFill(framebuffer, x + 1, 167, 16, 11,
                   (u8)(swatches[index] & 0xff), (u8)((swatches[index] >> 8) & 0xff),
                   (u8)((swatches[index] >> 16) & 0xff));
        if (index == currentFilter) bottomRect(framebuffer, x - 1, 165, 20, 15, 25);
    }

    bottomFill(framebuffer, 3, 183, 314, 42, 236, 233, 216);
    for (int slider = 0; slider < 3; slider++) {
        int y = 188 + slider * 11;
        float value = slider == 0 ? brightness : slider == 1 ? contrast : saturation;
        int knob = 58 + (int)((value - 0.5f) * 170.0f / 1.5f);
        bottomText(framebuffer, 9, y, slider == 0 ? "B" : slider == 1 ? "C" : "S", 25, 45, 80);
        bottomFill(framebuffer, 52, y + 3, 178, 2, 130, 130, 130);
        bottomFill(framebuffer, knob - 3, y, 7, 8, 40, 105, 210);
    }
    bottomFill(framebuffer, 254, 184, 60, 39, 100, 100, 100);
    bottomFill(framebuffer, 256, 186, 56, 35, 236, 233, 216);
    bottomRect(framebuffer, 270, 195, 25, 17, 25);
    bottomFill(framebuffer, 277, 191, 10, 5, 25, 25, 25);
    bottomCircle(framebuffer, 282, 203, 5, 25);
    bottomText(framebuffer, 229, 196, "A", 20, 24, 34);
    bottomFill(framebuffer, 3, 227, 314, 11, 225, 222, 214);
    bottomText(framebuffer, 7, 229, videoMode ? "VIDEO 10 FPS" : "PHOTO", 20, 24, 34);
    bottomText(framebuffer, 104, 229, "ZOOM", 20, 24, 34);
    bottomText(framebuffer, 181, 229, "MEM", 20, 24, 34);
        bottomText(framebuffer, 92, 116, galleryCount ? "PHOTO GALLERY" : "GALLERY EMPTY", 245, 245, 245);
        bottomText(framebuffer, 8, 226, "D PAD BROWSE   B CLOSE", 20, 24, 34);
        return;
    }

    bottomFill(framebuffer, 3, 47, 34, 116, 128, 128, 128);
    bottomFill(framebuffer, 4, 48, 32, 114, 236, 233, 216);
    drawBottomToolIcons(framebuffer);
    renderBottomPreview(framebuffer, cameraReady);
    bottomText(framebuffer, 42, 162, filterNames[currentFilter], 65, 65, 75);

    bottomFill(framebuffer, 3, 164, 314, 17, 236, 233, 216);
    for (int index = 0; index < FILTER_COUNT; index++) {
        int x = 7 + index * 25;
        bottomFill(framebuffer, x, 166, 18, 13, 65, 65, 65);
        bottomFill(framebuffer, x + 1, 167, 16, 11,
                   (u8)(filterColors[index] & 0xff),
                   (u8)((filterColors[index] >> 8) & 0xff),
                   (u8)((filterColors[index] >> 16) & 0xff));
        if (index == currentFilter) bottomRect(framebuffer, x - 1, 165, 20, 15, 25);
    }

    bottomFill(framebuffer, 3, 183, 314, 42, 236, 233, 216);
    for (int slider = 0; slider < 3; slider++) {
        int y = 188 + slider * 11;
        float value = slider == 0 ? brightness : slider == 1 ? contrast : saturation;
        int knob = 58 + (int)((value - 0.5f) * 170.0f / 1.5f);
        bottomText(framebuffer, 9, y, slider == 0 ? "B" : slider == 1 ? "C" : "S", 25, 45, 80);
        bottomFill(framebuffer, 52, y + 3, 178, 2, 130, 130, 130);
        bottomFill(framebuffer, knob - 3, y, 7, 8, 40, 105, 210);
    }
    bottomFill(framebuffer, 254, 184, 60, 39, 100, 100, 100);
    bottomFill(framebuffer, 256, 186, 56, 35, 236, 233, 216);
    bottomRect(framebuffer, 270, 195, 25, 17, 25);
    bottomFill(framebuffer, 277, 191, 10, 5, 25, 25, 25);
    bottomCircle(framebuffer, 282, 203, 5, 25);
    bottomText(framebuffer, 229, 196, "A", 20, 24, 34);
    bottomFill(framebuffer, 3, 227, 314, 11, 225, 222, 214);
    bottomText(framebuffer, 7, 229, videoMode ? "VIDEO 10 FPS" : "PHOTO", 20, 24, 34);
    bottomText(framebuffer, 104, 229, "ZOOM", 20, 24, 34);
    bottomText(framebuffer, 181, 229, "MEM", 20, 24, 34);
}
#endif

static void formatClock(char *buffer, size_t size, const char *format)
{
    time_t now = time(NULL);
    struct tm *local = localtime(&now);
    if (!local || strftime(buffer, size, format, local) == 0)
        snprintf(buffer, size, "00:00");
}

static const u8 smallFont[26][7] = {
    {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},
    {30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
    {14,17,16,23,17,17,15},{17,17,17,31,17,17,17},{14,4,4,4,4,4,14},
    {7,2,2,2,18,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
    {17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},
    {30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
    {15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},
    {17,17,17,17,17,10,4},{17,17,17,21,21,21,10},{17,17,10,4,10,17,17},
    {17,17,10,4,4,4,4},{31,1,2,4,8,16,31}
};

static const u8 digitFont[10][7] = {
    {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},
    {30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
    {14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},
    {14,17,17,15,1,1,14}
};

static void bottomPixel(u8 *framebuffer, int x, int y, u8 red, u8 green, u8 blue)
{
    if (x < 0 || x >= 320 || y < 0 || y >= 240) return;
    size_t offset = ((size_t)x * 240 + 239 - y) * 3;
    framebuffer[offset] = blue;
    framebuffer[offset + 1] = green;
    framebuffer[offset + 2] = red;
}

static void bottomFill(u8 *framebuffer, int x, int y, int width, int height,
                       u8 red, u8 green, u8 blue)
{
    for (int row = 0; row < height; row++)
        for (int column = 0; column < width; column++)
            bottomPixel(framebuffer, x + column, y + row, red, green, blue);
}

static void bottomStatusText(u8 *framebuffer, int x, int y, const char *text,
                             u8 red, u8 green, u8 blue)
{
    static const u8 letters[26][5] = {
        {2,5,7,5,5}, {6,5,6,5,6}, {3,4,4,4,3}, {6,5,5,5,6},
        {7,4,6,4,7}, {7,4,6,4,4}, {3,4,5,5,3}, {5,5,7,5,5},
        {7,2,2,2,7}, {1,1,1,5,2}, {5,5,6,5,5}, {4,4,4,4,7},
        {5,7,7,5,5}, {5,7,7,7,5}, {2,5,5,5,2}, {6,5,6,4,4},
        {2,5,5,3,1}, {6,5,6,5,5}, {3,4,2,1,6}, {7,2,2,2,2},
        {5,5,5,5,7}, {5,5,5,5,2}, {5,5,7,7,5}, {5,5,2,5,5},
        {5,5,2,2,2}, {7,1,2,4,7}
    };
    static const u8 digits[10][5] = {
        {7,5,5,5,7}, {2,6,2,2,7}, {6,1,7,4,7}, {6,1,3,1,6},
        {5,5,7,1,1}, {7,4,6,1,6}, {3,4,7,5,7}, {7,1,2,2,2},
        {7,5,7,5,7}, {7,5,7,1,6}
    };
    for (; *text; text++, x += 4) {
        char character = *text;
        if (character >= 'a' && character <= 'z') character -= 'a' - 'A';
        const u8 *glyph = NULL;
        if (character >= 'A' && character <= 'Z') glyph = letters[character - 'A'];
        else if (character >= '0' && character <= '9') glyph = digits[character - '0'];
        if (!glyph) continue;
        for (int row = 0; row < 5; row++)
            for (int column = 0; column < 3; column++)
                if (glyph[row] & (1 << (2 - column)))
                    bottomPixel(framebuffer, x + column, y + row, red, green, blue);
    }
}

static void bottomText(u8 *framebuffer, int x, int y, const char *text,
                       u8 red, u8 green, u8 blue)
{
    for (; *text; text++, x += 6) {
        char character = *text;
        if (character >= 'a' && character <= 'z') character -= 'a' - 'A';
        const u8 *glyph = NULL;
        if (character >= 'A' && character <= 'Z') glyph = smallFont[character - 'A'];
        else if (character >= '0' && character <= '9') glyph = digitFont[character - '0'];
        if (glyph) {
            for (int row = 0; row < 7; row++)
                for (int column = 0; column < 5; column++)
                    if (glyph[row] & (1 << (4 - column)))
                        bottomPixel(framebuffer, x + column, y + row, red, green, blue);
        } else if (character == ':') {
            bottomPixel(framebuffer, x + 2, y + 2, red, green, blue);
            bottomPixel(framebuffer, x + 2, y + 5, red, green, blue);
        } else if (character == '.') {
            bottomPixel(framebuffer, x + 2, y + 6, red, green, blue);
        } else if (character == '-') {
            bottomFill(framebuffer, x + 1, y + 3, 4, 1, red, green, blue);
        } else if (character == '_') {
            bottomFill(framebuffer, x, y + 6, 5, 1, red, green, blue);
        } else if (character == '+') {
            bottomFill(framebuffer, x + 1, y + 3, 4, 1, red, green, blue);
            bottomFill(framebuffer, x + 2, y + 2, 1, 4, red, green, blue);
        } else if (character == '/') {
            for (int step = 0; step < 6; step++)
                bottomPixel(framebuffer, x + 1 + step / 2, y + 6 - step, red, green, blue);
        } else if (character == 'X') {
            for (int step = 0; step < 6; step++) {
                bottomPixel(framebuffer, x + step, y + step, red, green, blue);
                bottomPixel(framebuffer, x + 5 - step, y + step, red, green, blue);
            }
        }
    }
}

static void bottomLine(u8 *framebuffer, int x0, int y0, int x1, int y1,
                       u8 red, u8 green, u8 blue)
{
    int deltaX = abs(x1 - x0), stepX = x0 < x1 ? 1 : -1;
    int deltaY = -abs(y1 - y0), stepY = y0 < y1 ? 1 : -1;
    int error = deltaX + deltaY;
    for (;;) {
        bottomPixel(framebuffer, x0, y0, red, green, blue);
        if (x0 == x1 && y0 == y1) break;
        int doubledError = error * 2;
        if (doubledError >= deltaY) { error += deltaY; x0 += stepX; }
        if (doubledError <= deltaX) { error += deltaX; y0 += stepY; }
    }
}

static void bottomCircle(u8 *framebuffer, int cx, int cy, int radius, u8 shade)
{
    int inner = (radius - 1) * (radius - 1);
    int outer = radius * radius;
    for (int y = -radius; y <= radius; y++)
        for (int x = -radius; x <= radius; x++) {
            int distance = x * x + y * y;
            if (distance >= inner && distance <= outer)
                bottomPixel(framebuffer, cx + x, cy + y, shade, shade, shade);
        }
}

static void bottomRect(u8 *framebuffer, int x, int y, int width, int height, u8 shade)
{
    bottomLine(framebuffer, x, y, x + width, y, shade, shade, shade);
    bottomLine(framebuffer, x, y + height, x + width, y + height, shade, shade, shade);
    bottomLine(framebuffer, x, y, x, y + height, shade, shade, shade);
    bottomLine(framebuffer, x + width, y, x + width, y + height, shade, shade, shade);
}

static void drawBottomToolIcons(u8 *framebuffer)
{
    const u8 ink = 18, highlight = 248;
    for (int icon = 0; icon < TOOLBAR_TOOL_COUNT; icon++) {
        int x = 10, y = TOOLBAR_TOP + icon * TOOLBAR_SLOT_HEIGHT;
        bool selected = (icon == 0 && !videoMode) || (icon == 1 && videoMode) ||
                        (icon == 2 && (captureTimerSeconds || timerMenuOpen)) ||
                        (icon == 3 && drawEnabled && !eraserEnabled && !textPlacementPending) ||
                        (icon == 4 && drawEnabled && eraserEnabled && !textPlacementPending) ||
                        (icon == 5 && textPlacementPending);
        bottomFill(framebuffer, 6, y, 29, TOOLBAR_BUTTON_HEIGHT, selected ? 190 : 225,
                   selected ? 211 : 222, selected ? 238 : 214);
        bottomRect(framebuffer, 6, y, 28, TOOLBAR_BUTTON_HEIGHT - 1, 128);
        if (icon == 0) {
            bottomFill(framebuffer, x + 3, y + 4, 14, 6, ink, ink, ink);
            bottomFill(framebuffer, x + 7, y + 2, 6, 2, ink, ink, ink);
            bottomFill(framebuffer, x + 9, y + 6, 3, 2, highlight, highlight, highlight);
        } else if (icon == 1) {
            bottomFill(framebuffer, x + 3, y + 3, 10, 7, ink, ink, ink);
            bottomLine(framebuffer, x + 13, y + 5, x + 18, y + 3, ink, ink, ink);
            bottomLine(framebuffer, x + 13, y + 8, x + 18, y + 10, ink, ink, ink);
            bottomLine(framebuffer, x + 18, y + 3, x + 18, y + 10, ink, ink, ink);
        } else if (icon == 2) {
            bottomCircle(framebuffer, x + 10, y + 6, 4, ink);
            bottomLine(framebuffer, x + 10, y + 3, x + 10, y + 6, ink, ink, ink);
            bottomLine(framebuffer, x + 10, y + 6, x + 13, y + 7, ink, ink, ink);
        } else if (icon == 3) {
            bottomLine(framebuffer, x + 5, y + 9, x + 15, y + 2, ink, ink, ink);
            bottomLine(framebuffer, x + 7, y + 10, x + 17, y + 3, ink, ink, ink);
            bottomLine(framebuffer, x + 4, y + 10, x + 7, y + 10, ink, ink, ink);
        } else if (icon == 4) {
            bottomLine(framebuffer, x + 4, y + 7, x + 13, y + 2, ink, ink, ink);
            bottomLine(framebuffer, x + 13, y + 2, x + 18, y + 6, ink, ink, ink);
            bottomLine(framebuffer, x + 18, y + 6, x + 10, y + 10, ink, ink, ink);
            bottomLine(framebuffer, x + 10, y + 10, x + 4, y + 7, ink, ink, ink);
        } else if (icon == 5) {
            bottomText(framebuffer, x + 8, y + 3, "T", ink, ink, ink);
        } else {
            bottomCircle(framebuffer, x + 9, y + 5, 3, ink);
            bottomLine(framebuffer, x + 11, y + 7, x + 15, y + 10, ink, ink, ink);
            bottomLine(framebuffer, x + 7, y + 5, x + 11, y + 5, ink, ink, ink);
            if (icon == 6) bottomLine(framebuffer, x + 9, y + 3, x + 9, y + 7, ink, ink, ink);
        }
    }
}

static void topPixel(u8 *framebuffer, int x, int y, u8 red, u8 green, u8 blue)
{
    if (x < 0 || x >= FRAME_WIDTH || y < 0 || y >= FRAME_HEIGHT) return;
    size_t offset = ((size_t)x * FRAME_HEIGHT + FRAME_HEIGHT - 1 - y) * 3;
    framebuffer[offset] = blue;
    framebuffer[offset + 1] = green;
    framebuffer[offset + 2] = red;
}

static void topFill(u8 *framebuffer, int x, int y, int width, int height, u8 red, u8 green, u8 blue)
{
    for (int dy = 0; dy < height; dy++)
        for (int dx = 0; dx < width; dx++)
            topPixel(framebuffer, x + dx, y + dy, red, green, blue);
}

static void topText(u8 *framebuffer, int x, int y, int scale, const char *text, u8 red, u8 green, u8 blue)
{
    for (; *text; text++, x += 6 * scale) {
        char character = *text;
        if (character >= 'a' && character <= 'z') character -= 'a' - 'A';
        if (character == ' ') continue;
        if (character < 'A' || character > 'Z') continue;
        const u8 *glyph = smallFont[character - 'A'];
        for (int row = 0; row < 7; row++)
            for (int column = 0; column < 5; column++)
                if (glyph[row] & (1 << (4 - column)))
                    topFill(framebuffer, x + column * scale, y + row * scale,
                            scale, scale, red, green, blue);
    }
}

static void topCameraIcon(u8 *framebuffer, int x, int y)
{
    topFill(framebuffer, x + 2, y + 5, 16, 10, 25, 28, 38);
    topFill(framebuffer, x + 6, y + 3, 6, 3, 25, 28, 38);
    topFill(framebuffer, x + 7, y + 7, 6, 6, 245, 245, 245);
    topFill(framebuffer, x + 9, y + 9, 2, 2, 25, 28, 38);
}

static u8 quantizePreviewColor(u8 red, u8 green, u8 blue)
{
    static const u8 colors[8][3] = {
        {0, 0, 0}, {220, 40, 40}, {40, 180, 60}, {220, 190, 40},
        {50, 80, 220}, {200, 50, 200}, {40, 190, 200}, {235, 235, 235}
    };
    unsigned bestDistance = UINT32_MAX;
    u8 best = 0;
    for (u8 index = 0; index < 8; index++) {
        int dr = (int)red - colors[index][0];
        int dg = (int)green - colors[index][1];
        int db = (int)blue - colors[index][2];
        unsigned distance = (unsigned)(dr * dr + dg * dg + db * db);
        if (distance < bestDistance) { bestDistance = distance; best = index; }
    }
    return best;
}

static void renderBottomPreview(u8 *framebuffer, bool live)
{
    bottomFill(framebuffer, 40, PREVIEW_TOP - 2, 278, PREVIEW_HEIGHT + 4, 95, 95, 95);
    bottomFill(framebuffer, 42, PREVIEW_TOP - 1, 274, PREVIEW_HEIGHT + 2, 0, 0, 0);
    if (!live) return;

    const int cropHeight = FRAME_WIDTH * PREVIEW_HEIGHT / PREVIEW_WIDTH;
    const int cropTop = (FRAME_HEIGHT - cropHeight) / 2;
    for (int y = 0; y < PREVIEW_HEIGHT; y++) {
        int sourceY = cropTop + y * cropHeight / PREVIEW_HEIGHT;
        for (int x = 0; x < PREVIEW_WIDTH; x++) {
            int sourceX = x * FRAME_WIDTH / PREVIEW_WIDTH;
            u16 pixel = processedFrame[sourceY * FRAME_WIDTH + sourceX];
            u8 red = (u8)((pixel & 0x1f) * 255 / 31);
            u8 green = (u8)(((pixel >> 5) & 0x3f) * 255 / 63);
            u8 blue = (u8)(((pixel >> 11) & 0x1f) * 255 / 31);
            bottomPixel(framebuffer, PREVIEW_LEFT + x, PREVIEW_TOP + y, red, green, blue);
        }
    }
}

#if 0
static const u32 colorShell = C32(212, 208, 200);
static const u32 colorPanel = C32(236, 233, 216);
static const u32 colorBorder = C32(128, 128, 128);
static const u32 colorTitle = C32(25, 83, 180);
static const u32 colorInk = C32(20, 24, 34);
static const u32 colorBlue = C32(34, 102, 205);
static const u32 filterColors[FILTER_COUNT] = {
    C32(238, 238, 238), C32(80, 145, 235), C32(172, 125, 80), C32(128, 128, 128),
    C32(245, 70, 70), C32(20, 200, 55), C32(120, 115, 150), C32(255, 115, 35),
    C32(150, 90, 230), C32(20, 20, 20), C32(245, 165, 205), C32(30, 240, 145)
};

static void uiText(float x, float y, float scale, u32 color, const char *value)
{
    C2D_Text text;
    C2D_TextParse(&text, uiTextBuffer, value);
    C2D_DrawText(&text, C2D_WithColor, x, y, 0.5f, scale, scale, color);
}

static void uiLine(float x0, float y0, float x1, float y1, u32 color, float thickness)
{
    C2D_DrawLine(x0, y0, color, x1, y1, color, thickness, 0.4f);
}

static void uiIcon(int icon, float x, float y, float size, u32 color)
{
    float centerX = x + size * 0.5f;
    float centerY = y + size * 0.5f;
    switch (icon) {
    case 0:
        C2D_DrawRectSolid(x + 3, y + 7, 0.3f, size - 6, size - 13, color);
        C2D_DrawRectSolid(x + 8, y + 4, 0.3f, size * 0.28f, 4, color);
        C2D_DrawCircleSolid(centerX, centerY, 0.2f, size * 0.2f, colorShell);
        C2D_DrawCircleSolid(centerX, centerY, 0.1f, size * 0.1f, color);
        break;
    case 1:
        C2D_DrawRectSolid(x + 3, y + 5, 0.3f, size * 0.58f, size - 10, color);
        C2D_DrawTriangle(x + size * 0.58f, y + 8, color,
                         x + size - 2, y + 3, color,
                         x + size - 2, y + size - 3, color, 0.3f);
        break;
    case 2:
        uiLine(x + 3, centerY - 3, x + size - 3, centerY - 3, color, 2);
        uiLine(x + 3, centerY - 3, x + 7, centerY - 7, color, 2);
        uiLine(x + 3, centerY - 3, x + 7, centerY + 1, color, 2);
        uiLine(x + size - 3, centerY + 4, x + 3, centerY + 4, color, 2);
        uiLine(x + size - 3, centerY + 4, x + size - 7, centerY, color, 2);
        uiLine(x + size - 3, centerY + 4, x + size - 7, centerY + 8, color, 2);
        break;
    case 3:
        uiLine(x + 5, y + size - 5, x + size - 7, y + 6, color, 4);
        uiLine(x + size - 9, y + 5, x + size - 4, y + 10, color, 4);
        break;
    case 4:
        C2D_DrawTriangle(x + 5, y + 8, color,
                         x + size - 5, y + 4, color,
                         x + size - 7, y + size - 5, color, 0.3f);
        C2D_DrawTriangle(x + 5, y + 8, color,
                         x + size - 7, y + size - 5, color,
                         x + 3, y + size - 9, color, 0.3f);
        break;
    case 5:
        C2D_DrawRectSolid(x + 7, y + 6, 0.3f, size - 14, 3, color);
        C2D_DrawRectSolid(x + 9, y + 9, 0.3f, size - 18, size - 14, color);
        C2D_DrawRectSolid(x + 5, y + size - 6, 0.3f, size - 10, 2, color);
        break;
    case 6:
    case 7:
        C2D_DrawCircle(centerX - 2, centerY - 2, 0.3f, size * 0.28f,
                       color, color, color, color);
        uiLine(centerX + 3, centerY + 3, x + size - 2, y + size - 2, color, 3);
        uiLine(centerX - 6, centerY - 2, centerX + 2, centerY - 2, color, 2);
        if (icon == 6) uiLine(centerX - 2, centerY - 6, centerX - 2, centerY + 2, color, 2);
        break;
    }
}

static void uiButton(float x, float y, float width, float height, int icon, bool active)
{
    C2D_DrawRectSolid(x, y, 0.8f, width, height, colorBorder);
    C2D_DrawRectSolid(x + 1, y + 1, 0.7f, width - 2, height - 2,
                      active ? C2D_Color32(181, 202, 235, 255) : colorPanel);
    uiIcon(icon, x + (width - 22) * 0.5f, y + (height - 22) * 0.5f, 22, colorInk);
}

static void drawBottomUi(void)
{
    C2D_TextBufClear(uiTextBuffer);
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    C2D_TargetClear(bottomTarget, C2D_Color32(39, 43, 53, 255));
    C2D_SceneBegin(bottomTarget);

    C2D_DrawRectSolid(0, 0, 0.9f, 320, 240, colorShell);
    C2D_DrawRectSolid(0, 0, 0.8f, 320, 20, colorTitle);
    uiText(7, 3, 0.43f, C2D_Color32(255, 255, 255, 255), galleryOpen ? "ZeroCam - Gallery" : "ZeroCam - Untitled");
    char clock[16];
    char status[80];
    formatClock(clock, sizeof(clock), "%H:%M");
    uiText(278, 3, 0.4f, C2D_Color32(255, 255, 255, 255), clock);

    C2D_DrawRectSolid(0, 20, 0.8f, 320, 18, colorPanel);
    uiText(7, 22, 0.38f, colorInk, "File");
    uiText(58, 22, 0.38f, colorInk, "Edit");
    uiText(111, 22, 0.38f, colorInk, "Extras");
    uiText(260, 22, 0.38f, colorBlue, "Gallery");
    C2D_DrawRectSolid(0, 38, 0.8f, 320, 15, C2D_Color32(225, 222, 214, 255));

    if (galleryOpen) {
        snprintf(status, sizeof(status), "%u saved", (unsigned)galleryCount);
        uiText(7, 40, 0.36f, colorInk, status);
        C2D_DrawRectSolid(8, 57, 0.6f, 304, 123, colorBorder);
        C2D_DrawRectSolid(10, 59, 0.5f, 300, 119, C2D_Color32(12, 16, 25, 255));
        if (galleryCount) {
            snprintf(status, sizeof(status), "Photo %u", (unsigned)(galleryIndex + 1));
            uiText(118, 110, 0.55f, C2D_Color32(240, 240, 245, 255), status);
        } else {
            uiText(105, 110, 0.45f, C2D_Color32(240, 240, 245, 255), "Gallery is empty");
        }
        uiText(10, 226, 0.35f, colorInk, "D-PAD: browse photos     B: close");
        C3D_FrameEnd(0);
        return;
    }

    snprintf(status, sizeof(status), "%u saved", (unsigned)galleryCount);
    uiText(7, 40, 0.34f, colorInk, status);
    uiText(112, 40, 0.34f, colorInk, cameraSelect == SELECT_OUT1 ? "Outer camera" : "Inner camera");
    uiText(265, 40, 0.34f, cameraReady ? C2D_Color32(20, 120, 45, 255) : C2D_Color32(170, 20, 20, 255),
           cameraReady ? "LIVE" : "DEMO");

    for (int tool = 0; tool < 8; tool++)
        uiButton(4, 56 + tool * 13, 28, 12, tool, tool == 0 ? !videoMode : tool == 1 ? videoMode : false);

    C2D_DrawRectSolid(36, 55, 0.7f, 280, 103, colorBorder);
    C2D_DrawRectSolid(38, 57, 0.6f, 276, 99, C2D_Color32(3, 6, 13, 255));
    if (!cameraReady)
        uiText(101, 101, 0.4f, C2D_Color32(220, 220, 225, 255), "Camera blocked - demo feed");

    C2D_DrawRectSolid(259, 137, 0.8f, 54, 18, colorBorder);
    C2D_DrawRectSolid(260, 138, 0.7f, 52, 16, colorPanel);
    uiIcon(0, 263, 135, 22, colorInk);
    uiText(282, 141, 0.29f, colorInk, cameraSelect == SELECT_OUT1 ? "OUT" : "IN");

    C2D_DrawRectSolid(3, 162, 0.8f, 314, 18, colorPanel);
    for (int filter = 0; filter < FILTER_COUNT; filter++) {
        float x = 30 + filter * 23;
        C2D_DrawRectSolid(x - 1, 163, 0.5f, 19, 16, colorBorder);
        C2D_DrawRectSolid(x, 164, 0.4f, 17, 14, filterColors[filter]);
        if (filter == currentFilter)
            C2D_DrawRectSolid(x - 1, 162, 0.3f, 19, 2, colorBlue);
    }

    C2D_DrawRectSolid(3, 182, 0.8f, 314, 43, colorPanel);
    for (int slider = 0; slider < 3; slider++) {
        float y = 188 + slider * 11;
        float value = slider == 0 ? brightness : slider == 1 ? contrast : saturation;
        float knob = 50 + (value - 0.5f) * 145 / 1.5f;
        C2D_DrawRectSolid(50, y + 3, 0.4f, 145, 2, colorBorder);
        C2D_DrawCircleSolid(knob, y + 4, 0.3f, 4, colorBlue);
        if (slider == 0) {
            C2D_DrawCircle(19, y + 4, 0.3f, 5, colorInk, colorInk, colorInk, colorInk);
            uiLine(19, y - 3, 19, y - 1, colorInk, 1.5f);
            uiLine(19, y + 9, 19, y + 11, colorInk, 1.5f);
        } else if (slider == 1) {
            C2D_DrawCircle(19, y + 4, 0.3f, 5, colorInk, colorInk, colorInk, colorInk);
            uiLine(19, y, 19, y + 8, colorInk, 1.5f);
        } else {
            C2D_DrawCircleSolid(16, y + 2, 0.3f, 2, C2D_Color32(255, 80, 80, 255));
            C2D_DrawCircleSolid(22, y + 2, 0.3f, 2, C2D_Color32(30, 160, 70, 255));
            C2D_DrawCircleSolid(19, y + 7, 0.3f, 2, C2D_Color32(45, 100, 220, 255));
        }
    }
    C2D_DrawRectSolid(255, 185, 0.4f, 57, 34, colorBorder);
    C2D_DrawRectSolid(256, 186, 0.3f, 55, 32, colorPanel);
    uiIcon(0, 271, 191, 24, colorInk);
    uiText(306, 195, 0.3f, colorInk, "A");
    C2D_DrawRectSolid(3, 227, 0.8f, 314, 11, C32(225, 222, 214));
    snprintf(status, sizeof(status), "%s   Zoom: %.1fx   Mem: %u/60",
             videoMode ? "Video" : "Photo", zoom, (unsigned)galleryCount);
    uiText(7, 227, 0.32f, colorInk, status);
    C3D_FrameEnd(0);
}
#endif

#if 0
static void drawBottomUi(void)
{
    char row[64];
    char clock[16];
    consoleSelect(&bottomConsole);
    consoleClear();
    formatClock(clock, sizeof(clock), "%H:%M");
    snprintf(row, sizeof(row), " ZeroCam - [%s]                    %s", galleryOpen ? "Gallery" : "Untitled", clock);
    printConsoleRow(0, XP_TITLE, row);
    printConsoleRow(1, XP_LIGHT, " File     Edit     Extras                         Gallery ");

    if (galleryOpen) {
        snprintf(row, sizeof(row), " %u saved     %s", (unsigned)galleryCount,
                 galleryCount ? strrchr(galleryFiles[galleryIndex], '/') + 1 : "No saved photos");
        printConsoleRow(2, XP_LIGHT, row);
        printConsoleRow(8, XP_PREVIEW, galleryCount ? "             PHOTO GALLERY             " : "           GALLERY IS EMPTY            ");
        printConsoleRow(25, XP_LIGHT, " D-PAD: browse photos              B: close ");
        printConsoleRow(27, XP_LIGHT, " START: exit                              ");
        return;
    }

    snprintf(row, sizeof(row), " %u saved       %s camera       %s", (unsigned)galleryCount,
             cameraSelect == SELECT_OUT1 ? "Outer" : "Inner",
             movie.active ? "REC" : cameraReady ? "LIVE" : "DEMO FEED");
    printConsoleRow(2, XP_LIGHT, row);

    static const char *tools[8] = {"(@)", "[> ]", "<->", "/  ", "<=>", "[X]", "+", "-"};
    for (int line = 0; line < 8; line++) {
        printConsoleRow(4 + line * 2, XP_LIGHT, "");
        printConsoleSegment(4 + line * 2, 0, 5, XP_TOOL, tools[line]);
        printConsoleRow(5 + line * 2, XP_LIGHT, "");
        printConsoleSegment(5 + line * 2, 0, 5, XP_TOOL, "");
    }
    snprintf(row, sizeof(row), " %s | B:%.1f  C:%.1f  S:%.1f", filterNames[currentFilter], brightness, contrast, saturation);
    printConsoleRow(20, XP_LIGHT, row);
    printConsoleRow(21, XP_LIGHT, " *  [---------o---------] ");
    printConsoleRow(22, XP_LIGHT, " <> [---------o---------] ");
    printConsoleRow(23, XP_LIGHT, " oO [---------o---------] ");
    printConsoleRow(24, XP_LIGHT, "");

    static const char *swatchStyles[FILTER_COUNT] = {
        "\x1b[47;30m", "\x1b[44;37m", "\x1b[43;30m", "\x1b[47;30m",
        "\x1b[41;37m", "\x1b[42;30m", "\x1b[45;37m", "\x1b[41;37m",
        "\x1b[45;37m", "\x1b[40;37m", "\x1b[45;37m", "\x1b[42;30m"
    };
    printConsoleRow(25, XP_LIGHT, "");
    for (int index = 0; index < FILTER_COUNT; index++)
        printConsoleSegment(25, 1 + index * 3, 2,
                            index == currentFilter ? XP_BLUE : swatchStyles[index], "  ");
    printConsoleSegment(26, 32, 6, XP_TOOL, "(@)");
    printConsoleRow(28, XP_LIGHT, " Photo  Zoom: D-PAD    Mem: see gallery");
}
#endif

static void drawGameboyUi(u8 *framebuffer)
{
    static const char *builtinNames[GAMEBOY_BUILTIN_FRAMES] = {"NONE", "PIXEL", "STRIPES"};
    static const int paletteLeft[4] = {4, 42, 80, 118};
    const char *controls[] = {"TIMER", "CAM", videoMode ? "VIDEO" : "PHOTO", "SHOT", "BACK"};
    bottomFill(framebuffer, 0, HEADER_STATUS_TOP + HEADER_STATUS_HEIGHT, 320,
               240 - HEADER_STATUS_TOP - HEADER_STATUS_HEIGHT, 185, 195, 164);
    for (int index = 0; index < 5; index++) {
        int y = GAMEBOY_CONTROL_TOP + index * GAMEBOY_CONTROL_PITCH;
        bottomFill(framebuffer, GAMEBOY_CONTROL_LEFT, y, GAMEBOY_CONTROL_WIDTH, GAMEBOY_CONTROL_HEIGHT,
                   221, 229, 204);
        bottomRect(framebuffer, GAMEBOY_CONTROL_LEFT, y, GAMEBOY_CONTROL_WIDTH - 1,
                   GAMEBOY_CONTROL_HEIGHT - 1, 70);
        bottomText(framebuffer, GAMEBOY_CONTROL_LEFT + 2, y + 6, controls[index], 24, 50, 25);
    }
    bottomFill(framebuffer, PREVIEW_LEFT - 2, PREVIEW_TOP - 2, PREVIEW_WIDTH + 4,
               PREVIEW_HEIGHT + 4, 48, 72, 43);
    bottomFill(framebuffer, PREVIEW_LEFT - 1, PREVIEW_TOP - 1, PREVIEW_WIDTH + 2,
               PREVIEW_HEIGHT + 2, 15, 56, 15);
    if (cameraReady) {
        int width = FRAME_WIDTH * PREVIEW_HEIGHT / FRAME_HEIGHT;
        int left = PREVIEW_LEFT + (PREVIEW_WIDTH - width) / 2;
        for (int y = 0; y < PREVIEW_HEIGHT; y++) {
            int sourceY = y * FRAME_HEIGHT / PREVIEW_HEIGHT;
            for (int x = 0; x < width; x++) {
                int sourceX = x * FRAME_WIDTH / width;
                u16 pixel = processedFrame[sourceY * FRAME_WIDTH + sourceX];
                bottomPixel(framebuffer, left + x, PREVIEW_TOP + y,
                            (u8)((pixel & 0x1f) * 255 / 31),
                            (u8)(((pixel >> 5) & 0x3f) * 255 / 63),
                            (u8)(((pixel >> 11) & 0x1f) * 255 / 31));
            }
        }
    }
    bottomFill(framebuffer, 3, GAMEBOY_FRAME_TOP, 314, 18, 221, 229, 204);
    for (int palette = 0; palette < 4; palette++) {
        int left = paletteLeft[palette];
        bool selected = palette == gameboyPaletteIndex;
        bottomFill(framebuffer, left, GAMEBOY_FRAME_TOP + 1, 36, 16,
                   selected ? 48 : 205, selected ? 98 : 211, selected ? 48 : 190);
        bottomRect(framebuffer, left, GAMEBOY_FRAME_TOP + 1, 35, 15,
                   selected ? 24 : 70);
        for (int shade = 0; shade < 4; shade++)
            bottomFill(framebuffer, left + 3 + shade * 7, GAMEBOY_FRAME_TOP + 3, 7, 4,
                       gameboyPalettes[palette][shade][0], gameboyPalettes[palette][shade][1],
                       gameboyPalettes[palette][shade][2]);
        bottomText(framebuffer, left + 4, GAMEBOY_FRAME_TOP + 10,
                   gameboyPaletteNames[palette], selected ? 255 : 24,
                   selected ? 255 : 50, selected ? 255 : 25);
    }
    bottomRect(framebuffer, 160, GAMEBOY_FRAME_TOP + 1, 25, 15, 70);
    bottomLine(framebuffer, 175, GAMEBOY_FRAME_TOP + 4, 170, GAMEBOY_FRAME_TOP + 9, 24, 50, 25);
    bottomLine(framebuffer, 170, GAMEBOY_FRAME_TOP + 9, 175, GAMEBOY_FRAME_TOP + 14, 24, 50, 25);
    bottomRect(framebuffer, 290, GAMEBOY_FRAME_TOP + 1, 25, 15, 70);
    bottomLine(framebuffer, 300, GAMEBOY_FRAME_TOP + 4, 305, GAMEBOY_FRAME_TOP + 9, 24, 50, 25);
    bottomLine(framebuffer, 305, GAMEBOY_FRAME_TOP + 9, 300, GAMEBOY_FRAME_TOP + 14, 24, 50, 25);
    bottomText(framebuffer, 190, GAMEBOY_FRAME_TOP + 5, "FRAME", 24, 50, 25);
    char name[27];
    if (gameboyFrameIndex < GAMEBOY_BUILTIN_FRAMES) {
        snprintf(name, sizeof(name), "%s", builtinNames[gameboyFrameIndex]);
    } else {
        const char *path = gameboyFramePaths[gameboyFrameIndex - GAMEBOY_BUILTIN_FRAMES];
        const char *base = strrchr(path, '/');
        snprintf(name, sizeof(name), "%.16s", base ? base + 1 : path);
    }
    bottomText(framebuffer, 220, GAMEBOY_FRAME_TOP + 5, name, 24, 50, 25);

    bottomFill(framebuffer, 3, GAMEBOY_SLIDER_TOP, 314, 60, 221, 229, 204);
    static const char *labels[3] = {"BRIGHT", "CONTRAST", "DITHER"};
    for (int slider = 0; slider < 3; slider++) {
        int y = GAMEBOY_SLIDER_TOP + slider * GAMEBOY_SLIDER_HEIGHT + 6;
        float value = slider == 0 ? (brightness - 0.5f) / 1.5f :
                      slider == 1 ? (contrast - 0.5f) / 1.5f : gameboyDither;
        int knob = GAMEBOY_SLIDER_LEFT + (int)(value * (GAMEBOY_SLIDER_RIGHT - GAMEBOY_SLIDER_LEFT));
        bottomText(framebuffer, 7, y, labels[slider], 24, 50, 25);
        bottomFill(framebuffer, GAMEBOY_SLIDER_LEFT, y + 3,
                   GAMEBOY_SLIDER_RIGHT - GAMEBOY_SLIDER_LEFT + 1, 2, 85, 110, 75);
        bottomFill(framebuffer, knob - 2, y, 6, 8, 48, 98, 48);
    }
}

static const char *greenScreenBackgroundName(int index)
{
    if (index < GREEN_BUILTIN_BACKGROUNDS) return greenScreenBuiltinNames[index];
    const char *path = greenScreenPaths[index - GREEN_BUILTIN_BACKGROUNDS];
    const char *base = strrchr(path, '/');
    return base ? base + 1 : path;
}

static void drawGreenScreenUi(u8 *framebuffer)
{
    bottomFill(framebuffer, 0, HEADER_STATUS_TOP + HEADER_STATUS_HEIGHT, 320,
               240 - HEADER_STATUS_TOP - HEADER_STATUS_HEIGHT, 212, 208, 200);
    bottomFill(framebuffer, 3, 38, 194, 118, 80, 80, 80);
    bottomFill(framebuffer, 5, 40, 190, 114, 0, 0, 0);
    if (cameraReady) {
        for (int y = 0; y < 114; y++) {
            int sourceY = y * FRAME_HEIGHT / 114;
            for (int x = 0; x < 190; x++) {
                int sourceX = x * FRAME_WIDTH / 190;
                u16 pixel = processedFrame[sourceY * FRAME_WIDTH + sourceX];
                bottomPixel(framebuffer, 5 + x, 40 + y,
                            (u8)((pixel & 31) * 255 / 31),
                            (u8)(((pixel >> 5) & 63) * 255 / 63),
                            (u8)(((pixel >> 11) & 31) * 255 / 31));
            }
        }
    }
    bottomFill(framebuffer, 7, 42, 80, 11, 20, 24, 34);
    bottomText(framebuffer, 10, 44, backdropMask.calibrated ? "BG READY" : "NO BG", 255, 255, 255);
    bottomFill(framebuffer, GREEN_CAPTURE_LEFT, 41, GREEN_CAPTURE_WIDTH, 13, 220, 217, 207);
    bottomRect(framebuffer, GREEN_CAPTURE_LEFT, 41, GREEN_CAPTURE_WIDTH - 1, 12, 100);
    bottomText(framebuffer, GREEN_CAPTURE_LEFT + 5, 44,
               backdropMask.calibrated ? "RECAPTURE" : "CAPTURE BG", 20, 24, 34);
    bottomFill(framebuffer, GREEN_LIST_LEFT, 38, 115, 118, 236, 233, 216);
    bottomText(framebuffer, GREEN_LIST_LEFT + 4, 41, "BACKGROUNDS", 20, 24, 34);
    int total = GREEN_BUILTIN_BACKGROUNDS + (int)greenScreenImportedCount;
    for (int row = 0; row < GREEN_BACKGROUND_PAGE_SIZE; row++) {
        int index = greenScreenPageStart + row;
        if (index >= total) break;
        int top = GREEN_LIST_TOP + row * GREEN_LIST_ROW_HEIGHT;
        bool selected = index == greenScreenBackgroundIndex && greenScreenBackgroundReady;
        bottomFill(framebuffer, GREEN_LIST_LEFT + 2, top, 111, 18,
                   selected ? 40 : 220, selected ? 100 : 217, selected ? 60 : 207);
        for (int y = 0; y < GREEN_THUMB_HEIGHT; y++) {
            for (int x = 0; x < GREEN_THUMB_WIDTH; x++) {
                u16 pixel = greenScreenThumbnailValid[index] ?
                            greenScreenThumbnails[index][y * GREEN_THUMB_WIDTH + x] : 0;
                bottomPixel(framebuffer, GREEN_LIST_LEFT + 3 + x, top + 1 + y,
                            (u8)((pixel & 31) * 255 / 31),
                            (u8)(((pixel >> 5) & 63) * 255 / 63),
                            (u8)(((pixel >> 11) & 31) * 255 / 31));
            }
        }
        char name[14];
        snprintf(name, sizeof(name), "%.13s", greenScreenBackgroundName(index));
        bottomText(framebuffer, GREEN_LIST_LEFT + 32, top + 5, name,
                   selected ? 255 : 20, selected ? 255 : 24, selected ? 255 : 34);
    }
    bottomRect(framebuffer, GREEN_LIST_LEFT + 2, 134, 31, 19, 100);
    bottomText(framebuffer, GREEN_LIST_LEFT + 7, 140, "PREV", 20, 24, 34);
    bottomRect(framebuffer, GREEN_LIST_LEFT + 80, 134, 31, 19, 100);
    bottomText(framebuffer, GREEN_LIST_LEFT + 84, 140, "NEXT", 20, 24, 34);
    char page[24];
    snprintf(page, sizeof(page), "%d/%d", greenScreenPageStart / GREEN_BACKGROUND_PAGE_SIZE + 1,
             (total + GREEN_BACKGROUND_PAGE_SIZE - 1) / GREEN_BACKGROUND_PAGE_SIZE);
    bottomText(framebuffer, GREEN_LIST_LEFT + 44, 140, page, 20, 24, 34);

    static const char *sliderLabels[] = {"SENS", "BRIGHT", "CONTRAST"};
    for (int slider = 0; slider < 3; slider++) {
        int rowTop = GREEN_SLIDER_TOP + slider * GREEN_SLIDER_ROW_HEIGHT;
        bottomFill(framebuffer, 3, rowTop, 314, GREEN_SLIDER_ROW_HEIGHT, 236, 233, 216);
        bottomText(framebuffer, 8, rowTop + 3, sliderLabels[slider], 20, 24, 34);
        bottomFill(framebuffer, GREEN_SLIDER_LEFT, rowTop + 6,
                   GREEN_SLIDER_RIGHT - GREEN_SLIDER_LEFT + 1, 2, 110, 110, 110);
        float position = slider == 0 ? (40.0f - greenScreenThreshold) / 40.0f :
                         slider == 1 ? (brightness - 0.5f) / 1.5f :
                                       (contrast - 0.5f) / 1.5f;
        int knob = GREEN_SLIDER_LEFT + (int)(position * (GREEN_SLIDER_RIGHT - GREEN_SLIDER_LEFT));
        bottomFill(framebuffer, knob - 2, rowTop + 3, 6, 8, 40, 100, 60);
    }

    const char *controls[] = {"TIMER", "CAM", videoMode ? "VIDEO" : "PHOTO", "SHOT", "BACK"};
    for (int index = 0; index < 5; index++) {
        int left = GREEN_CONTROL_LEFT + index * GREEN_CONTROL_PITCH;
        bottomFill(framebuffer, left, GREEN_CONTROL_TOP, GREEN_CONTROL_WIDTH, GREEN_CONTROL_HEIGHT, 236, 233, 216);
        bottomRect(framebuffer, left, GREEN_CONTROL_TOP, GREEN_CONTROL_WIDTH - 1, GREEN_CONTROL_HEIGHT - 1, 100);
        bottomText(framebuffer, left + 4, GREEN_CONTROL_TOP + 12, controls[index], 20, 24, 34);
    }
}

static void drawBottomUi(void)
{
    static const u32 swatches[FILTER_COUNT] = {
        C32(238,238,238), C32(80,145,235), C32(172,125,80), C32(128,128,128),
        C32(245,70,70), C32(120,115,150), C32(255,115,35), C32(150,90,230),
        C32(20,20,20), C32(245,165,205), C32(139,172,15), C32(245,80,150), C32(20,200,55)
    };
    u8 *framebuffer = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);
    char status[48];
    char title[53];
    if (galleryOpen) {
        snprintf(title, sizeof(title), "ZERO CAM GALLERY");
    } else if (lastPhotoPath[0]) {
        const char *name = strrchr(lastPhotoPath, '/');
        name = name ? name + 1 : lastPhotoPath;
        const char *extension = strrchr(name, '.');
        size_t length = extension ? (size_t)(extension - name) : strlen(name);
        if (length > 23)
            snprintf(title, sizeof(title), "ZERO CAM %.*s...", 20, name);
        else
            snprintf(title, sizeof(title), "ZERO CAM %.*s", (int)length, name);
    } else {
        snprintf(title, sizeof(title), "ZERO CAM UNTITLED");
    }

    bottomFill(framebuffer, 0, 0, 320, 240, 212, 208, 200);
    const HudTheme *theme = &hudThemes[hudThemeIndex];
    bottomFill(framebuffer, 0, 0, 320, HEADER_TITLE_HEIGHT, theme->red, theme->green, theme->blue);
    bottomText(framebuffer, 7, 3, title, 255, 255, 255);
    bottomStatusText(framebuffer, 221, 5, "a project by churchzero", 220, 232, 255);
    bottomFill(framebuffer, 0, HEADER_MENU_TOP, 320, HEADER_MENU_HEIGHT, 236, 233, 216);
    if (galleryOpen) {
        bottomText(framebuffer, 7, HEADER_MENU_TOP + 3, gallerySelected ? "SELECTED" : "A SELECT", 20, 24, 34);
        bottomText(framebuffer, 95, HEADER_MENU_TOP + 3, galleryVideo.playing ? "Y PAUSE" : "Y PLAY", 20, 24, 34);
        bottomText(framebuffer, 250, HEADER_MENU_TOP + 3, "B BACK", 20, 24, 34);
    } else {
        bottomText(framebuffer, 7, HEADER_MENU_TOP + 3, menuOpen && menuIndex == 0 ? "FILE+" : "FILE", 20, 24, 34);
        bottomText(framebuffer, 58, HEADER_MENU_TOP + 3, menuOpen && menuIndex == 1 ? "EDIT+" : "EDIT", 20, 24, 34);
        bottomText(framebuffer, 112, HEADER_MENU_TOP + 3, menuOpen && menuIndex == 2 ? "EXTRAS+" : "EXTRAS", 20, 24, 34);
        bottomText(framebuffer, 260, HEADER_MENU_TOP + 3, "X GALLERY", theme->red, theme->green, theme->blue);
    }
    bottomFill(framebuffer, 0, HEADER_STATUS_TOP, 320, HEADER_STATUS_HEIGHT, 225, 222, 214);
    snprintf(status, sizeof(status), "%u SAVED  %s  %s  %s", (unsigned)galleryCount,
             cameraSelect == SELECT_OUT1 ? "OUTER" : "INNER", movie.active ? "REC" : "LIVE",
             photoSave.active ? "SAVING" : photoStatus);
    bottomStatusText(framebuffer, 6, HEADER_STATUS_TOP + 2, status, 20, 24, 34);
    if (photoSave.active) {
        static const int offsets[] = {0, 1, 2, 1};
        unsigned phase = (unsigned)((svcGetSystemTick() - photoSave.startTick) /
                                    (SYSCLOCK_ARM11 / 8)) % 4;
        int left = 6 + (int)strlen(status) * 4 + 2;
        for (int dot = 0; dot < 3; dot++)
            bottomFill(framebuffer, left + dot * 4, HEADER_STATUS_TOP + 3 + offsets[(phase + dot) % 4],
                       2, 2, 20, 24, 34);
    }

    if (galleryOpen) {
        bottomFill(framebuffer, 4, 53, 312, 163, 128, 128, 128);
        bottomFill(framebuffer, 6, 55, 308, 159, 212, 208, 200);
        if (!galleryCount) bottomText(framebuffer, 88, 123, "GALLERY EMPTY", 20, 24, 34);
        for (size_t slot = 0; slot < GALLERY_PAGE_SIZE && galleryPageStart + slot < galleryCount; slot++) {
            int x = GALLERY_GRID_LEFT + (slot % GALLERY_COLUMNS) * GALLERY_CELL_WIDTH;
            int y = GALLERY_GRID_TOP + (slot / GALLERY_COLUMNS) * GALLERY_CELL_HEIGHT;
            bool selected = galleryPageStart + slot == galleryIndex;
            bottomFill(framebuffer, x - 2, y - 2, GALLERY_THUMB_SIZE + 4, GALLERY_THUMB_SIZE + 4,
                       selected ? 25 : 120, selected ? 83 : 120, selected ? 180 : 120);
            for (int row = 0; row < GALLERY_THUMB_SIZE; row++) {
                for (int column = 0; column < GALLERY_THUMB_SIZE; column++) {
                    u16 pixel = galleryThumbnailValid[slot] ?
                                galleryThumbnails[slot][row * GALLERY_THUMB_SIZE + column] : 0;
                    bottomPixel(framebuffer, x + column, y + row,
                                (u8)((pixel & 0x1f) * 255 / 31),
                                (u8)(((pixel >> 5) & 0x3f) * 255 / 63),
                                (u8)(((pixel >> 11) & 0x1f) * 255 / 31));
                }
            }
            if (isVideoFile(galleryFiles[galleryPageStart + slot])) {
                bottomFill(framebuffer, x + 1, y + GALLERY_THUMB_SIZE - 10, 32, 9, 20, 24, 34);
                bottomText(framebuffer, x + 2, y + GALLERY_THUMB_SIZE - 9, "VIDEO", 255, 255, 255);
            }
        }
        if (galleryCount) {
            if (galleryPageStart > 0) {
                bottomFill(framebuffer, 10, 192, 52, 18, 236, 233, 216);
                bottomRect(framebuffer, 10, 192, 51, 17, 100);
                bottomText(framebuffer, 22, 197, "PREV", 20, 24, 34);
            }
            if (galleryPageStart + GALLERY_PAGE_SIZE < galleryCount) {
                bottomFill(framebuffer, 258, 192, 52, 18, 236, 233, 216);
                bottomRect(framebuffer, 258, 192, 51, 17, 100);
                bottomText(framebuffer, 270, 197, "NEXT", 20, 24, 34);
            }
            snprintf(status, sizeof(status), "PAGE %u OF %u",
                     (unsigned)(galleryPageStart / GALLERY_PAGE_SIZE + 1),
                     (unsigned)((galleryCount + GALLERY_PAGE_SIZE - 1) / GALLERY_PAGE_SIZE));
            bottomText(framebuffer, 126, 197, status, 20, 24, 34);
        }
        bottomFill(framebuffer, 3, 213, 314, 26, 236, 233, 216);
        bottomFill(framebuffer, 7, 216, 139, 20, 220, 216, 207);
        bottomRect(framebuffer, 7, 216, 138, 19, 100);
        bottomText(framebuffer, 35, 222, "DELETE SELECTED", 20, 24, 34);
        bottomFill(framebuffer, 165, 216, 149, 20, 220, 216, 207);
        bottomRect(framebuffer, 165, 216, 148, 19, 100);
        bottomText(framebuffer, 205, 222, "DELETE ALL", 20, 24, 34);
        if (galleryDeleteMode) {
            bottomFill(framebuffer, 27, 85, 266, 78, 70, 70, 70);
            bottomFill(framebuffer, 29, 87, 262, 74, 236, 233, 216);
            bottomFill(framebuffer, 29, 87, 262, 13, theme->red, theme->green, theme->blue);
            bottomText(framebuffer, 35, 90, "CONFIRM DELETE", 255, 255, 255);
            bottomText(framebuffer, 35, 106,
                       galleryDeleteMode == 2 ? "ALL PHOTOS AND VIDEOS" : "DELETE SELECTED", 20, 24, 34);
            if (galleryDeleteMode == 1 && galleryIndex < galleryCount) {
                const char *name = strrchr(galleryFiles[galleryIndex], '/');
                snprintf(status, sizeof(status), "%.32s", name ? name + 1 : galleryFiles[galleryIndex]);
                bottomText(framebuffer, 35, 118, status, 20, 24, 34);
            }
            bottomFill(framebuffer, 63, 135, 80, 19, 210, 206, 198);
            bottomRect(framebuffer, 63, 135, 79, 18, 100);
            bottomText(framebuffer, 94, 141, "YES", 20, 24, 34);
            bottomFill(framebuffer, 177, 135, 80, 19, 210, 206, 198);
            bottomRect(framebuffer, 177, 135, 79, 18, 100);
            bottomText(framebuffer, 197, 141, "CANCEL", 20, 24, 34);
        }
        return;
    }

    if (currentFilter == FILTER_GAMEBOY) {
        drawGameboyUi(framebuffer);
    } else if (currentFilter == FILTER_GREEN_SCREEN) {
        drawGreenScreenUi(framebuffer);
    } else {
    bottomFill(framebuffer, 3, PREVIEW_TOP - 2, 34, PREVIEW_HEIGHT + 4, 128, 128, 128);
    bottomFill(framebuffer, 4, PREVIEW_TOP - 1, 32, PREVIEW_HEIGHT + 2, 236, 233, 216);
    drawBottomToolIcons(framebuffer);
    renderBottomPreview(framebuffer, cameraReady);
    if (brushPaletteOpen) {
        bottomFill(framebuffer, 45, 54, 203, 27, 100, 100, 100);
        bottomFill(framebuffer, 46, 55, 201, 25, 236, 233, 216);
        for (int index = 0; index < BRUSH_COLOR_COUNT; index++) {
            u16 color = brushColors[index];
            int x = 49 + index * 17;
            bottomFill(framebuffer, x, 60, 14, 14, 50, 50, 50);
            bottomFill(framebuffer, x + 1, 61, 12, 12,
                       (u8)((color & 0x1f) * 255 / 31),
                       (u8)(((color >> 5) & 0x3f) * 255 / 63),
                       (u8)(((color >> 11) & 0x1f) * 255 / 31));
            if (color == drawColor565) bottomRect(framebuffer, x - 1, 59, 15, 15, 25);
        }
        bottomFill(framebuffer, 190, 59, 54, 17, 100, 100, 100);
        bottomFill(framebuffer, 191, 60, 52, 15, 236, 233, 216);
        bottomText(framebuffer, textColorPicking ? 198 : 204, 64,
               textColorPicking ? "CANCEL" : "PICK", 20, 24, 34);
    } else if (eyedropperMode) {
        bottomFill(framebuffer, 44, 51, 65, 10, 236, 233, 216);
        bottomText(framebuffer, 47, 53, "PICK COLOR", 20, 24, 34);
    }

    bottomFill(framebuffer, 3, FILTER_SWATCH_TOP, 314, FILTER_SWATCH_ROW_HEIGHT, 236, 233, 216);
    bottomText(framebuffer, 5, FILTER_SWATCH_TOP + 2, filterNames[currentFilter], 65, 65, 75);
    for (int index = 0; index < FILTER_COUNT; index++) {
        int x = FILTER_SWATCH_LEFT + index * FILTER_SWATCH_PITCH;
        bottomFill(framebuffer, x, FILTER_SWATCH_TOP + 1, 14, 12, 65, 65, 65);
        bottomFill(framebuffer, x + 1, FILTER_SWATCH_TOP + 2, 12, 10,
                   (u8)(swatches[index] & 0xff), (u8)((swatches[index] >> 8) & 0xff),
                   (u8)((swatches[index] >> 16) & 0xff));
        if (index == currentFilter) bottomRect(framebuffer, x - 1, FILTER_SWATCH_TOP, 16, 14, 25);
    }

    bottomFill(framebuffer, 3, 183, 314, 56, 236, 233, 216);
    for (int slider = 0; slider < SLIDER_COUNT; slider++) {
        int y = SLIDER_TOUCH_TOP + 5 + slider * SLIDER_ROW_HEIGHT;
        float value = slider == 0 ? brightness : slider == 1 ? contrast : saturation;
        int knob = SLIDER_TRACK_LEFT + (int)((value - 0.5f) *
                   (SLIDER_TRACK_RIGHT - SLIDER_TRACK_LEFT) / 1.5f);
        bottomText(framebuffer, 7, y, slider == 0 ? "B" : slider == 1 ? "C" : "S", 25, 45, 80);
        bottomFill(framebuffer, SLIDER_TRACK_LEFT, y + 3,
                   SLIDER_TRACK_RIGHT - SLIDER_TRACK_LEFT + 1, 2, 130, 130, 130);
        bottomFill(framebuffer, knob - 2, y + 1, 6, 7, 40, 105, 210);
    }
    bottomFill(framebuffer, 202, 184, 46, 18, 100, 100, 100);
    bottomFill(framebuffer, 203, 185, 44, 16, 236, 233, 216);
    bottomText(framebuffer, 207, 190, cameraSelect == SELECT_OUT1 ? "B OUT" : "B IN", 20, 24, 34);
    bottomFill(framebuffer, 202, 204, 46, 19, 100, 100, 100);
    bottomFill(framebuffer, 203, 205, 44, 17, 236, 233, 216);
    bottomText(framebuffer, 207, 210, videoMode ? "VIDEO" : "PHOTO", 20, 24, 34);
    bottomFill(framebuffer, 251, 184, 64, 39, 100, 100, 100);
    bottomFill(framebuffer, 253, 186, 60, 35, 236, 233, 216);
    bottomRect(framebuffer, 272, 198, 22, 16, 25);
    bottomFill(framebuffer, 278, 194, 9, 5, 25, 25, 25);
    bottomCircle(framebuffer, 283, 205, 4, 25);
    bottomFill(framebuffer, 260, 226, 56, 13, 100, 100, 100);
    bottomFill(framebuffer, 261, 227, 54, 11, 236, 233, 216);
    bottomText(framebuffer, 272, 229, "RESET", 20, 24, 34);
    }

    if (menuOpen) {
        static const char *fileItems[] = {"SAVE CURRENT IMAGE", "RENAME PHOTO"};
        static const char *editItems[] = {"UNDO DRAWING", "REDO DRAWING", "NEXT FILTER"};
        char themeItem[24];
        snprintf(themeItem, sizeof(themeItem), "THEME %s", theme->name);
        const char *extrasItems[] = {"MIRROR", "HUD", "SWITCH CAMERA", themeItem,
                         timestampEnabled ? "TIMESTAMP ON" : "TIMESTAMP OFF"};
        const char **items = menuIndex == 0 ? fileItems : menuIndex == 1 ? editItems : extrasItems;
        int itemCount = menuIndex == 0 ? 2 : menuIndex == 1 ? 3 : 5;
        bottomFill(framebuffer, 3, MENU_POPUP_TOP - 1, 152, itemCount * 13 + 2, 120, 120, 120);
        bottomFill(framebuffer, 4, MENU_POPUP_TOP, 150, itemCount * 13, 236, 233, 216);
        for (int item = 0; item < itemCount; item++) {
            int y = MENU_POPUP_TOP + item * 13;
            if (item == menuItem) bottomFill(framebuffer, 5, y, 147, 12, theme->red, theme->green, theme->blue);
            bottomText(framebuffer, 8, y + 3, items[item],
                       item == menuItem ? 255 : 20, item == menuItem ? 255 : 24,
                       item == menuItem ? 255 : 34);
        }
    }
    if (editingMenuOpen) {
        const char *items[EDIT_MENU_ITEM_COUNT] = {
            "NO FRAME", "HEARTS", "STARS", "FILM", "RAINBOW",
            mirrorPreview ? "MIRROR ON" : "MIRROR OFF",
            timestampEnabled ? "STAMP ON" : "STAMP OFF"
        };
        bottomFill(framebuffer, EDIT_MENU_LEFT, EDIT_MENU_TOP, EDIT_MENU_WIDTH,
                   EDIT_MENU_ITEM_COUNT * EDIT_MENU_ROW_HEIGHT + 2, 100, 100, 100);
        bottomFill(framebuffer, EDIT_MENU_LEFT + 1, EDIT_MENU_TOP + 1, EDIT_MENU_WIDTH - 2,
                   EDIT_MENU_ITEM_COUNT * EDIT_MENU_ROW_HEIGHT, 236, 233, 216);
        for (int item = 0; item < EDIT_MENU_ITEM_COUNT; item++) {
            int y = EDIT_MENU_TOP + 1 + item * EDIT_MENU_ROW_HEIGHT;
            bool selected = item == editingMenuItem;
            if (selected) bottomFill(framebuffer, EDIT_MENU_LEFT + 2, y, EDIT_MENU_WIDTH - 4, 12,
                                     theme->red, theme->green, theme->blue);
            bottomText(framebuffer, EDIT_MENU_LEFT + 6, y + 3, items[item],
                       selected ? 255 : 20, selected ? 255 : 24, selected ? 255 : 34);
            if (item < FRAME_STYLE_COUNT && item == currentFrame)
                bottomText(framebuffer, EDIT_MENU_LEFT + 132, y + 3, "ON",
                           selected ? 255 : 20, selected ? 255 : 24, selected ? 255 : 34);
        }
    }
    if (timerMenuOpen) {
        static const char *items[TIMER_MENU_ITEM_COUNT] = {"OFF", "4 SECONDS", "6 SECONDS", "8 SECONDS"};
        bottomFill(framebuffer, TIMER_MENU_LEFT, TIMER_MENU_TOP, TIMER_MENU_WIDTH,
                   TIMER_MENU_ITEM_COUNT * TIMER_MENU_ROW_HEIGHT + 2, 100, 100, 100);
        bottomFill(framebuffer, TIMER_MENU_LEFT + 1, TIMER_MENU_TOP + 1, TIMER_MENU_WIDTH - 2,
                   TIMER_MENU_ITEM_COUNT * TIMER_MENU_ROW_HEIGHT, 236, 233, 216);
        for (int item = 0; item < TIMER_MENU_ITEM_COUNT; item++) {
            int y = TIMER_MENU_TOP + 1 + item * TIMER_MENU_ROW_HEIGHT;
            bool selected = item == timerMenuItem;
            if (selected) bottomFill(framebuffer, TIMER_MENU_LEFT + 2, y, TIMER_MENU_WIDTH - 4, 13,
                                     theme->red, theme->green, theme->blue);
            bottomText(framebuffer, TIMER_MENU_LEFT + 6, y + 3, items[item],
                       selected ? 255 : 20, selected ? 255 : 24, selected ? 255 : 34);
        }
    }
}

static void drawTopUi(u8 *framebuffer, bool cameraReady)
{
    if (!showHud) return;
    if (!cameraReady) topFill(framebuffer, 0, 0, FRAME_WIDTH, FRAME_HEIGHT, 8, 12, 22);
    const HudTheme *theme = &hudThemes[hudThemeIndex];
    topFill(framebuffer, 0, 0, FRAME_WIDTH, 20, theme->red, theme->green, theme->blue);
    topText(framebuffer, 8, 4, 2, "ZERO CAM PREVIEW", 255, 255, 255);
    if (movie.active)
        topFill(framebuffer, 344, 7, 5, 5, 235, 35, 35);
    topFill(framebuffer, 0, 218, FRAME_WIDTH, 22, 192, 192, 192);
    topFill(framebuffer, 0, 218, FRAME_WIDTH, 1, 255, 255, 255);
    topFill(framebuffer, 0, 219, FRAME_WIDTH, 1, 223, 223, 223);
    topFill(framebuffer, 0, 238, FRAME_WIDTH, 1, 128, 128, 128);
    topFill(framebuffer, 0, 239, FRAME_WIDTH, 1, 64, 64, 64);
    topFill(framebuffer, 5, 221, 48, 16, 192, 192, 192);
    topFill(framebuffer, 5, 221, 48, 1, 255, 255, 255);
    topFill(framebuffer, 5, 221, 1, 16, 255, 255, 255);
    topFill(framebuffer, 5, 236, 48, 1, 96, 96, 96);
    topFill(framebuffer, 52, 221, 1, 16, 96, 96, 96);
    topText(framebuffer, 15, 226, 1, "ZERO", 24, 24, 24);
    topFill(framebuffer, 59, 221, 98, 16, 176, 176, 176);
    topFill(framebuffer, 59, 221, 98, 1, 96, 96, 96);
    topFill(framebuffer, 59, 221, 1, 16, 96, 96, 96);
    topFill(framebuffer, 156, 221, 1, 16, 255, 255, 255);
    topFill(framebuffer, 157, 221, 1, 16, 223, 223, 223);
    topText(framebuffer, 66, 226, 1, filterNames[currentFilter], 24, 24, 24);
    topFill(framebuffer, 170, 221, 72, 16, 192, 192, 192);
    topFill(framebuffer, 170, 221, 72, 1, 255, 255, 255);
    topFill(framebuffer, 170, 221, 1, 16, 255, 255, 255);
    topFill(framebuffer, 241, 221, 1, 16, 96, 96, 96);
    topFill(framebuffer, 170, 236, 72, 1, 96, 96, 96);
    topText(framebuffer, 181, 226, 1, videoMode ? "VIDEO" : "PHOTO", 24, 24, 24);
    if (movie.active) {
        topFill(framebuffer, 252, 226, 5, 5, 210, 32, 32);
        topText(framebuffer, 262, 226, 1, "REC", 24, 24, 24);
    }
    char clock[8];
    formatClock(clock, sizeof(clock), "%H:%M");
    topFill(framebuffer, 350, 221, 45, 16, 224, 224, 224);
    topFill(framebuffer, 350, 221, 45, 1, 96, 96, 96);
    topFill(framebuffer, 350, 221, 1, 16, 96, 96, 96);
    topFill(framebuffer, 394, 221, 1, 16, 255, 255, 255);
    topFill(framebuffer, 350, 236, 45, 1, 255, 255, 255);
    for (int index = 0; clock[index]; index++) {
        int x = 357 + index * 6;
        if (clock[index] >= '0' && clock[index] <= '9') {
            const u8 *glyph = digitFont[clock[index] - '0'];
            for (int row = 0; row < 7; row++)
                for (int column = 0; column < 5; column++)
                    if (glyph[row] & (1 << (4 - column)))
                        topPixel(framebuffer, x + column, 225 + row, 24, 24, 24);
        } else if (clock[index] == ':') {
            topPixel(framebuffer, x + 2, 227, 24, 24, 24);
            topPixel(framebuffer, x + 2, 230, 24, 24, 24);
        }
    }
    if (!cameraReady)
        topText(framebuffer, 100, 112, 2, "CAMERA BLOCKED", 255, 255, 255);
}

static u8 clampByte(int value)
{
    if (value < 0) return 0;
    if (value > 255) return 255;
    return (u8)value;
}

static void applyFilter(Filter filter, int *red, int *green, int *blue, int x, int y)
{
    int r = *red;
    int g = *green;
    int b = *blue;
    int gray = (r * 77 + g * 150 + b * 29) >> 8;

    switch (filter) {
    case FILTER_XP_BLUE:
        r = (r * 4) / 5;
        g = (g * 9) / 10;
        b = (b * 6) / 5;
        break;
    case FILTER_SEPIA: {
        int originalR = r;
        int originalG = g;
        int originalB = b;
        r = (originalR * 393 + originalG * 769 + originalB * 189) / 1000;
        g = (originalR * 349 + originalG * 686 + originalB * 168) / 1000;
        b = (originalR * 272 + originalG * 534 + originalB * 131) / 1000;
        break;
    }
    case FILTER_MONO:
        r = g = b = gray;
        break;
    case FILTER_VIVID:
        r = gray + ((r - gray) * 3) / 2;
        g = gray + ((g - gray) * 3) / 2;
        b = gray + ((b - gray) * 3) / 2;
        break;
    case FILTER_VHS:
        if (y % 3 == 0) {
            r = (r * 4) / 5;
            g = (g * 4) / 5;
            b = (b * 4) / 5;
        }
        break;
    case FILTER_THERMAL:
        if (gray < 64) {
            r = gray / 2; g = 0; b = gray * 2;
        } else if (gray < 128) {
            r = gray; g = 0; b = 255 - gray;
        } else if (gray < 192) {
            r = 255; g = (gray - 128) * 4; b = 0;
        } else {
            r = 255; g = 255; b = (gray - 192) * 4;
        }
        break;
    case FILTER_NEGATIVE:
        r = 255 - r; g = 255 - g; b = 255 - b;
        break;
    case FILTER_NORMAL:
    case FILTER_PIXEL:
    case FILTER_HEARTS:
    case FILTER_DREAM:
    case FILTER_GREEN_SCREEN:
    case FILTER_GAMEBOY:
    case FILTER_COUNT:
        break;
    }

    int luminance = (r * 77 + g * 150 + b * 29) >> 8;
    r = (int)(((luminance + (r - luminance) * saturation - 128) * contrast + 128) * brightness);
    g = (int)(((luminance + (g - luminance) * saturation - 128) * contrast + 128) * brightness);
    b = (int)(((luminance + (b - luminance) * saturation - 128) * contrast + 128) * brightness);
    if (filter == FILTER_GAMEBOY) {
        static const u8 matrix[4][4] = {
            {0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}
        };
        int adjusted = clampByte((r * 77 + g * 150 + b * 29) >> 8);
        int gameboyX = x * 128 / FRAME_WIDTH;
        int gameboyY = y * 112 / FRAME_HEIGHT;
        adjusted += (int)((matrix[gameboyY & 3][gameboyX & 3] * 2 - 15) * 2.5f * gameboyDither);
        int shade = (clampByte(adjusted) * 3 + 127) / 255;
        r = gameboyPalettes[gameboyPaletteIndex][shade][0];
        g = gameboyPalettes[gameboyPaletteIndex][shade][1];
        b = gameboyPalettes[gameboyPaletteIndex][shade][2];
    }
    *red = clampByte(r);
    *green = clampByte(g);
    *blue = clampByte(b);
}

static void drawTimestamp(u8 *framebuffer)
{
    char timestamp[24];
    formatClock(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M");
    int left = FRAME_WIDTH - (int)strlen(timestamp) * 6 - 6;
    const int top = 38;
    for (int pass = 0; pass < 2; pass++) {
        for (size_t index = 0; timestamp[index]; index++) {
            char character = timestamp[index];
            int x = left + (int)index * 6;
            for (int row = 0; row < 7; row++) {
                for (int column = 0; column < 5; column++) {
                    bool filled = character >= '0' && character <= '9' &&
                                  (digitFont[character - '0'][row] & (1 << (4 - column)));
                    if (character == '-' && row == 3 && column >= 1 && column <= 3) filled = true;
                    if (character == ':' && column == 2 && (row == 2 || row == 5)) filled = true;
                    if (!filled) continue;
                    int radius = pass == 0 ? 1 : 0;
                    for (int offsetY = -radius; offsetY <= radius; offsetY++) {
                        for (int offsetX = -radius; offsetX <= radius; offsetX++) {
                            int targetX = x + column + offsetX;
                            int targetY = top + row + offsetY;
                            if (targetX < 0 || targetX >= FRAME_WIDTH || targetY < 0 || targetY >= FRAME_HEIGHT) continue;
                            u8 red = pass ? 255 : 0;
                            u8 green = pass ? 184 : 0;
                            u8 blue = pass ? 32 : 0;
                            processedFrame[targetY * FRAME_WIDTH + targetX] =
                                (u16)(((blue >> 3) << 11) | ((green >> 2) << 5) | (red >> 3));
                            topPixel(framebuffer, targetX, targetY, red, green, blue);
                        }
                    }
                }
            }
        }
    }
}

static void applyFramePixel(int x, int y, int *red, int *green, int *blue)
{
    bool side = x < 14 || x >= FRAME_WIDTH - 14;
    bool edge = y < 14 || y >= FRAME_HEIGHT - 14;
    if (!side && !edge) return;
    int localX = side ? (x < 14 ? x : x - (FRAME_WIDTH - 14)) : x % 28;
    int localY = side ? y % 28 : (y < 14 ? y : y - (FRAME_HEIGHT - 14));

    if (currentFrame == FRAME_HEARTS && localX < 14 && localY < 14) {
        static const u8 heart[7] = {0x36, 0x7f, 0x7f, 0x3e, 0x1c, 0x08, 0x00};
        if (heart[localY / 2] & (1 << (6 - localX / 2))) {
            *red = 250; *green = 90; *blue = 135;
        }
    } else if (currentFrame == FRAME_STARS && localX < 14 && localY < 14) {
        int dx = abs(localX - 6), dy = abs(localY - 6);
        if ((dx <= 1 && dy <= 6) || (dy <= 1 && dx <= 6) || (dx <= 3 && dy <= 3 && dx == dy)) {
            *red = 255; *green = 215; *blue = 70;
        }
    } else if (currentFrame == FRAME_FILM) {
        bool hole = side ? (localX >= 3 && localX < 11 && localY >= 4 && localY < 12) :
                           (localY >= 3 && localY < 11 && localX >= 4 && localX < 12);
        *red = *green = *blue = hole ? 235 : 20;
    } else if (currentFrame == FRAME_RAINBOW) {
        static const u8 colors[6][3] = {
            {240, 65, 75}, {255, 155, 45}, {245, 220, 70},
            {65, 190, 110}, {65, 150, 235}, {190, 100, 205}
        };
        int band = side ? (x < 14 ? x : FRAME_WIDTH - 1 - x) :
                          (y < 14 ? y : FRAME_HEIGHT - 1 - y);
        int color = (band / 2) % 6;
        *red = colors[color][0]; *green = colors[color][1]; *blue = colors[color][2];
    }
}

static void rebuildGameboyFrameMap(void)
{
    memset(gameboyFrameMap, 0, sizeof(gameboyFrameMap));
    if (!gameboyFrameIndex ||
        (gameboyFrameIndex >= GAMEBOY_BUILTIN_FRAMES && !gameboyFrameLoaded)) return;
    int height = gameboyFrameLoaded ? gameboyFrameHeight : 144;
    int width = GAMEBOY_FRAME_WIDTH * FRAME_HEIGHT / height;
    int left = (FRAME_WIDTH - width) / 2;
    int cameraTop = height == 224 ? 40 : 16;
    for (int y = 0; y < FRAME_HEIGHT; y++) {
        int sourceY = y * height / FRAME_HEIGHT;
        for (int x = 0; x < FRAME_WIDTH; x++) {
            int shade = 0, alpha = 255;
            if (x >= left && x < left + width) {
                int sourceX = (x - left) * GAMEBOY_FRAME_WIDTH / width;
                if (sourceX >= 16 && sourceX < 144 && sourceY >= cameraTop && sourceY < cameraTop + 112)
                    continue;
                if (gameboyFrameLoaded) {
                    const u8 *pixel = &gameboyFramePixels[(sourceY * GAMEBOY_FRAME_WIDTH + sourceX) * 4];
                    alpha = pixel[3];
                    int luminance = (pixel[0] * 77 + pixel[1] * 150 + pixel[2] * 29) >> 8;
                    shade = (luminance * 3 + 127) / 255;
                } else if (gameboyFrameIndex == 1) {
                    shade = ((sourceX / 8 + sourceY / 8) & 1) ? 1 : 2;
                } else {
                    shade = ((sourceX / 4) & 1) ? 0 : 3;
                }
            }
            gameboyFrameMap[y * FRAME_WIDTH + x] = (u16)((alpha << 2) | shade);
        }
    }
}

static void applyGameboyFramePixel(int x, int y, int *red, int *green, int *blue)
{
    u16 pixel = gameboyFrameMap[y * FRAME_WIDTH + x];
    int alpha = pixel >> 2;
    if (!alpha) return;
    int shade = pixel & 3;
    *red = (gameboyPalettes[gameboyPaletteIndex][shade][0] * alpha + *red * (255 - alpha)) / 255;
    *green = (gameboyPalettes[gameboyPaletteIndex][shade][1] * alpha + *green * (255 - alpha)) / 255;
    *blue = (gameboyPalettes[gameboyPaletteIndex][shade][2] * alpha + *blue * (255 - alpha)) / 255;
}

static void buildDreamBlur(void)
{
    static const s8 offsets[9][2] = {
        {0, 0}, {-6, 0}, {6, 0}, {0, -6}, {0, 6},
        {-6, -6}, {6, -6}, {-6, 6}, {6, 6}
    };
    static const u8 weights[9] = {4, 2, 2, 2, 2, 1, 1, 1, 1};
    for (int y = 0; y < FRAME_HEIGHT / DREAM_SCALE; y++) {
        for (int x = 0; x < FRAME_WIDTH / DREAM_SCALE; x++) {
            int red = 0, green = 0, blue = 0, glow = 0;
            for (int sample = 0; sample < 9; sample++) {
                int sourceX = x * DREAM_SCALE + DREAM_SCALE / 2 + offsets[sample][0];
                int sourceY = y * DREAM_SCALE + DREAM_SCALE / 2 + offsets[sample][1];
                if (sourceX < 0) sourceX = 0;
                if (sourceX >= FRAME_WIDTH) sourceX = FRAME_WIDTH - 1;
                if (sourceY < 0) sourceY = 0;
                if (sourceY >= FRAME_HEIGHT) sourceY = FRAME_HEIGHT - 1;
                u16 pixel = cameraFrame[sourceY * FRAME_WIDTH + sourceX];
                int sampleRed = ((pixel >> 11) & 0x1f) * 255 / 31;
                int sampleGreen = ((pixel >> 5) & 0x3f) * 255 / 63;
                int sampleBlue = (pixel & 0x1f) * 255 / 31;
                int highlight = ((sampleRed * 77 + sampleGreen * 150 + sampleBlue * 29) >> 8) - 150;
                red += sampleRed * weights[sample];
                green += sampleGreen * weights[sample];
                blue += sampleBlue * weights[sample];
                if (highlight > 0) glow += highlight * weights[sample];
            }
            dreamBlur[y][x][0] = (u8)(red / 16);
            dreamBlur[y][x][1] = (u8)(green / 16);
            dreamBlur[y][x][2] = (u8)(blue / 16);
            dreamGlow[y][x] = (u8)(glow / 16);
        }
    }
}

static void drawTrackedHearts(u8 *framebuffer)
{
    float centerX, centerY, faceSize;
    if (!faceTrackerGet(&centerX, &centerY, &faceSize)) return;
    static const s8 positions[7] = {-52, -36, -20, 0, 20, 36, 52};
    static const u8 arc[7] = {0, 10, 18, 22, 18, 10, 0};
    static const u8 colors[3][3] = {{245, 65, 140}, {255, 115, 175}, {255, 155, 195}};
    static const u8 heart[7] = {0x36, 0x7f, 0x7f, 0x7f, 0x3e, 0x1c, 0x08};
    int scale = (int)(faceSize / 55.0f);
    if (scale < 1) scale = 1;
    if (scale > 3) scale = 3;
    for (int item = 0; item < 7; item++) {
        int left = (int)(centerX + faceSize * positions[item] / 100.0f) - 7 * scale / 2;
        int top = (int)(centerY - faceSize / 2.0f - faceSize * (15 + arc[item]) / 100.0f) - 7 * scale / 2;
        for (int row = 0; row < 7; row++) {
            for (int column = 0; column < 7; column++) {
                if (!(heart[row] & (1 << (6 - column)))) continue;
                for (int offsetY = 0; offsetY < scale; offsetY++) {
                    for (int offsetX = 0; offsetX < scale; offsetX++) {
                        int x = left + column * scale + offsetX;
                        int y = top + row * scale + offsetY;
                        if (x < 0 || x >= FRAME_WIDTH || y < 0 || y >= FRAME_HEIGHT) continue;
                        u16 pixel = processedFrame[y * FRAME_WIDTH + x];
                        int red = ((pixel & 31) * 255 / 31 + colors[item % 3][0] * 3) / 4;
                        int green = (((pixel >> 5) & 63) * 255 / 63 + colors[item % 3][1] * 3) / 4;
                        int blue = (((pixel >> 11) & 31) * 255 / 31 + colors[item % 3][2] * 3) / 4;
                        processedFrame[y * FRAME_WIDTH + x] =
                            (u16)(((blue >> 3) << 11) | ((green >> 2) << 5) | (red >> 3));
                        topPixel(framebuffer, x, y, red, green, blue);
                    }
                }
            }
        }
    }
}

static void renderFrame(u8 *framebuffer)
{
    int cropWidth = (int)(FRAME_WIDTH / zoom);
    int cropHeight = (int)(FRAME_HEIGHT / zoom);
    int left = (FRAME_WIDTH - cropWidth) / 2;
    int top = (FRAME_HEIGHT - cropHeight) / 2;
    bool applyAdjustments = currentFilter != FILTER_NORMAL || brightness != 1.0f ||
                            contrast != 1.0f || saturation != 1.0f;
    static bool heartFilterWasActive;
    if (currentFilter == FILTER_HEARTS) {
        if (!heartFilterWasActive) faceTrackerInvalidate();
        heartFilterWasActive = true;
        if (!faceTrackerSubmit(cameraFrame, left, top, cropWidth, cropHeight, mirrorPreview))
            photoStatus = "FACE TRACK FAILED";
    } else if (heartFilterWasActive) {
        faceTrackerInvalidate();
        heartFilterWasActive = false;
    }
    if (currentFilter == FILTER_DREAM) buildDreamBlur();
    if (currentFilter == FILTER_GREEN_SCREEN && backdropMask.calibrated) {
        if (cropWidth != backdropCropWidth || cropHeight != backdropCropHeight ||
            mirrorPreview != backdropMirrored) {
            backdropMask.calibrated = false;
            photoStatus = "CAPTURE BG AGAIN";
        } else if (greenScreenBackgroundReady) {
            backdropUpdate(&backdropMask, cameraFrame, left, top, cropWidth, cropHeight,
                           mirrorPreview, greenScreenThreshold);
        }
    }

    for (int y = 0; y < FRAME_HEIGHT; y++) {
        for (int x = 0; x < FRAME_WIDTH; x++) {
            int sx = left + x * cropWidth / FRAME_WIDTH;
            int sy = top + y * cropHeight / FRAME_HEIGHT;

            if (currentFilter == FILTER_PIXEL) {
                sx = left + ((sx - left) / 5) * 5;
                sy = top + ((sy - top) / 5) * 5;
            } else if (currentFilter == FILTER_GAMEBOY) {
                sx = left + (x * 128 / FRAME_WIDTH) * cropWidth / 128;
                sy = top + (y * 112 / FRAME_HEIGHT) * cropHeight / 112;
            }

            if (mirrorPreview) sx = FRAME_WIDTH - 1 - sx;
            u16 pixel = cameraFrame[sy * FRAME_WIDTH + sx];
            int red = ((pixel >> 11) & 0x1f) * 255 / 31;
            int green = ((pixel >> 5) & 0x3f) * 255 / 63;
            int blue = (pixel & 0x1f) * 255 / 31;
            if (cameraSelect == SELECT_IN1) {
                red = red * 96 / 100;
                green = clampByte(green * 104 / 100);
                blue = blue * 93 / 100;
            }
            int greenKey = currentFilter == FILTER_GREEN_SCREEN && greenScreenBackgroundReady &&
                           backdropMask.calibrated && backdropMask.maskReady ?
                           255 - backdropAlpha(&backdropMask, x, y) : 0;
            if (currentFilter == FILTER_DREAM) {
                const u8 *blur = dreamBlur[sy / DREAM_SCALE][sx / DREAM_SCALE];
                int glow = dreamGlow[sy / DREAM_SCALE][sx / DREAM_SCALE];
                red = clampByte((red * 2 + blur[0] * 3) / 5 + glow * 3 / 4);
                green = clampByte((green * 2 + blur[1] * 3) / 5 + glow * 3 / 4);
                blue = clampByte((blue * 2 + blur[2] * 3) / 5 + glow * 3 / 4);
            }
            if (applyAdjustments) applyFilter(currentFilter, &red, &green, &blue, x, y);
            if (greenKey) {
                u16 background = greenScreenBackground[y * FRAME_WIDTH + x];
                int backgroundRed = (background & 31) * 255 / 31;
                int backgroundGreen = ((background >> 5) & 63) * 255 / 63;
                int backgroundBlue = ((background >> 11) & 31) * 255 / 31;
                red = (red * (255 - greenKey) + backgroundRed * greenKey + 127) / 255;
                green = (green * (255 - greenKey) + backgroundGreen * greenKey + 127) / 255;
                blue = (blue * (255 - greenKey) + backgroundBlue * greenKey + 127) / 255;
            }
            size_t drawIndex = (size_t)y * FRAME_WIDTH + x;
            u16 overlay = drawOverlay[drawIndex];
            int alpha = drawAlpha[drawIndex];
            if (alpha) {
                int overlayBlue = ((overlay >> 11) & 0x1f) * 255 / 31;
                int overlayGreen = ((overlay >> 5) & 0x3f) * 255 / 63;
                int overlayRed = (overlay & 0x1f) * 255 / 31;
                blue = (overlayBlue * alpha + blue * (255 - alpha) + 127) / 255;
                green = (overlayGreen * alpha + green * (255 - alpha) + 127) / 255;
                red = (overlayRed * alpha + red * (255 - alpha) + 127) / 255;
            }
            if (currentFrame != FRAME_NONE) applyFramePixel(x, y, &red, &green, &blue);
            if (currentFilter == FILTER_GAMEBOY) applyGameboyFramePixel(x, y, &red, &green, &blue);

            processedFrame[y * FRAME_WIDTH + x] =
                (u16)(((blue >> 3) << 11) | ((green >> 2) << 5) | (red >> 3));
            size_t offset = ((size_t)x * FRAME_HEIGHT + (FRAME_HEIGHT - 1 - y)) * 3;
            framebuffer[offset] = (u8)blue;
            framebuffer[offset + 1] = (u8)green;
            framebuffer[offset + 2] = (u8)red;
        }
    }
    if (currentFilter == FILTER_HEARTS) drawTrackedHearts(framebuffer);
    if (timestampEnabled) drawTimestamp(framebuffer);
}

static bool encodeJpeg(const u16 *pixels, unsigned char **output, unsigned long *outputSize, int quality,
                       int width, int height);

static void writeBe16(FILE *file, u16 value)
{
    fputc((value >> 8) & 0xff, file);
    fputc(value & 0xff, file);
}

static void writeBe32(FILE *file, u32 value)
{
    fputc((value >> 24) & 0xff, file);
    fputc((value >> 16) & 0xff, file);
    fputc((value >> 8) & 0xff, file);
    fputc(value & 0xff, file);
}

static long beginMovAtom(FILE *file, const char type[4])
{
    long offset = ftell(file);
    if (offset < 0) return -1;
    writeBe32(file, 0);
    fwrite(type, 1, 4, file);
    return offset;
}

static bool endMovAtom(FILE *file, long offset)
{
    long end = ftell(file);
    if (offset < 0 || end < offset || (unsigned long)(end - offset) > 0xffffffffUL) return false;
    if (fseek(file, offset, SEEK_SET) != 0) return false;
    writeBe32(file, (u32)(end - offset));
    return fseek(file, end, SEEK_SET) == 0;
}

static void writeMovMatrix(FILE *file)
{
    writeBe32(file, 0x00010000); writeBe32(file, 0); writeBe32(file, 0);
    writeBe32(file, 0); writeBe32(file, 0x00010000); writeBe32(file, 0);
    writeBe32(file, 0); writeBe32(file, 0); writeBe32(file, 0x40000000);
}

static bool writeMovieAudioTrack(FILE *file)
{
    u32 samples = movie.audioBytes / 2;
    u32 durationMs = (u32)((u64)samples * 1000 / MOV_AUDIO_RATE);
    long trak = beginMovAtom(file, "trak");
    long atom = beginMovAtom(file, "tkhd");
    writeBe32(file, 7); writeBe32(file, 0); writeBe32(file, 0); writeBe32(file, 2);
    writeBe32(file, 0); writeBe32(file, durationMs); writeBe32(file, 0); writeBe32(file, 0);
    writeBe16(file, 0); writeBe16(file, 0); writeBe16(file, 0x0100); writeBe16(file, 0);
    writeMovMatrix(file);
    writeBe32(file, 0); writeBe32(file, 0);
    if (!endMovAtom(file, atom)) return false;

    long mdia = beginMovAtom(file, "mdia");
    atom = beginMovAtom(file, "mdhd");
    writeBe32(file, 0); writeBe32(file, 0); writeBe32(file, 0);
    writeBe32(file, MOV_AUDIO_RATE); writeBe32(file, samples); writeBe16(file, 0x55c4); writeBe16(file, 0);
    if (!endMovAtom(file, atom)) return false;
    atom = beginMovAtom(file, "hdlr");
    writeBe32(file, 0); writeBe32(file, 0); fwrite("soun", 1, 4, file);
    writeBe32(file, 0); writeBe32(file, 0); writeBe32(file, 0);
    fwrite("SoundHandler\0", 1, 13, file);
    if (!endMovAtom(file, atom)) return false;

    long minf = beginMovAtom(file, "minf");
    atom = beginMovAtom(file, "smhd");
    writeBe32(file, 0); writeBe16(file, 0); writeBe16(file, 0);
    if (!endMovAtom(file, atom)) return false;
    long dinf = beginMovAtom(file, "dinf");
    atom = beginMovAtom(file, "dref");
    writeBe32(file, 0); writeBe32(file, 1);
    long url = beginMovAtom(file, "url ");
    writeBe32(file, 1);
    if (!endMovAtom(file, url) || !endMovAtom(file, atom) || !endMovAtom(file, dinf)) return false;

    long stbl = beginMovAtom(file, "stbl");
    atom = beginMovAtom(file, "stsd");
    writeBe32(file, 0); writeBe32(file, 1);
    long sample = beginMovAtom(file, "sowt");
    for (int index = 0; index < 6; index++) fputc(0, file);
    writeBe16(file, 1); writeBe16(file, 0); writeBe16(file, 0); writeBe32(file, 0);
    writeBe16(file, 1); writeBe16(file, 16); writeBe16(file, 0); writeBe16(file, 0);
    writeBe32(file, MOV_AUDIO_RATE << 16);
    if (!endMovAtom(file, sample) || !endMovAtom(file, atom)) return false;
    atom = beginMovAtom(file, "stts");
    writeBe32(file, 0); writeBe32(file, 1); writeBe32(file, samples); writeBe32(file, 1);
    if (!endMovAtom(file, atom)) return false;
    atom = beginMovAtom(file, "stsc");
    writeBe32(file, 0); writeBe32(file, 1); writeBe32(file, 1); writeBe32(file, samples); writeBe32(file, 1);
    if (!endMovAtom(file, atom)) return false;
    atom = beginMovAtom(file, "stsz");
    writeBe32(file, 0); writeBe32(file, 2); writeBe32(file, samples);
    if (!endMovAtom(file, atom)) return false;
    atom = beginMovAtom(file, "stco");
    writeBe32(file, 0); writeBe32(file, 1); writeBe32(file, movie.audioOffset);
    return endMovAtom(file, atom) && endMovAtom(file, stbl) && endMovAtom(file, minf) &&
           endMovAtom(file, mdia) && endMovAtom(file, trak);
}

static bool writeMovieIndex(FILE *file)
{
    u32 videoDuration = 0;
    for (u32 index = 0; index < movie.frameCount; index++) videoDuration += movie.durations[index];
    u32 audioDuration = (u32)((u64)(movie.audioBytes / 2) * 1000 / MOV_AUDIO_RATE);
    u32 duration = videoDuration > audioDuration ? videoDuration : audioDuration;
    long moov = beginMovAtom(file, "moov");
    long atom = beginMovAtom(file, "mvhd");
    writeBe32(file, 0); writeBe32(file, 0); writeBe32(file, 0);
    writeBe32(file, 1000); writeBe32(file, duration); writeBe32(file, 0x00010000);
    writeBe16(file, 0x0100); writeBe16(file, 0); writeBe32(file, 0); writeBe32(file, 0);
    writeMovMatrix(file);
    for (int i = 0; i < 6; i++) writeBe32(file, 0);
    writeBe32(file, movie.audioBytes ? 3 : 2);
    if (!endMovAtom(file, atom)) return false;

    long trak = beginMovAtom(file, "trak");
    atom = beginMovAtom(file, "tkhd");
    writeBe32(file, 7); writeBe32(file, 0); writeBe32(file, 0); writeBe32(file, 1);
    writeBe32(file, 0); writeBe32(file, videoDuration); writeBe32(file, 0); writeBe32(file, 0);
    writeBe16(file, 0); writeBe16(file, 0); writeBe16(file, 0); writeBe16(file, 0);
    writeMovMatrix(file);
    writeBe32(file, MOV_WIDTH << 16); writeBe32(file, MOV_HEIGHT << 16);
    if (!endMovAtom(file, atom)) return false;

    long mdia = beginMovAtom(file, "mdia");
    atom = beginMovAtom(file, "mdhd");
    writeBe32(file, 0); writeBe32(file, 0); writeBe32(file, 0);
    writeBe32(file, 1000); writeBe32(file, videoDuration); writeBe16(file, 0x55c4); writeBe16(file, 0);
    if (!endMovAtom(file, atom)) return false;
    atom = beginMovAtom(file, "hdlr");
    writeBe32(file, 0); writeBe32(file, 0); fwrite("vide", 1, 4, file);
    writeBe32(file, 0); writeBe32(file, 0); writeBe32(file, 0);
    fwrite("VideoHandler\0", 1, 13, file);
    if (!endMovAtom(file, atom)) return false;

    long minf = beginMovAtom(file, "minf");
    atom = beginMovAtom(file, "vmhd");
    writeBe32(file, 1); writeBe16(file, 0);
    writeBe16(file, 0); writeBe16(file, 0); writeBe16(file, 0);
    if (!endMovAtom(file, atom)) return false;

    long dinf = beginMovAtom(file, "dinf");
    atom = beginMovAtom(file, "dref");
    writeBe32(file, 0); writeBe32(file, 1);
    long url = beginMovAtom(file, "url ");
    writeBe32(file, 1);
    if (!endMovAtom(file, url) || !endMovAtom(file, atom) || !endMovAtom(file, dinf)) return false;

    long stbl = beginMovAtom(file, "stbl");
    atom = beginMovAtom(file, "stsd");
    writeBe32(file, 0); writeBe32(file, 1);
    long sample = beginMovAtom(file, "jpeg");
    for (int i = 0; i < 6; i++) fputc(0, file);
    writeBe16(file, 1); writeBe16(file, 0); writeBe16(file, 0); writeBe32(file, 0);
    writeBe32(file, 0); writeBe32(file, 0);
    writeBe16(file, MOV_WIDTH); writeBe16(file, MOV_HEIGHT);
    writeBe32(file, 0x00480000); writeBe32(file, 0x00480000);
    writeBe32(file, 0); writeBe16(file, 1);
    u8 compressorName[32] = {10, 'P', 'h', 'o', 't', 'o', ' ', 'J', 'P', 'E', 'G'};
    fwrite(compressorName, 1, sizeof(compressorName), file);
    writeBe16(file, 24); writeBe16(file, 0xffff);
    if (!endMovAtom(file, sample) || !endMovAtom(file, atom)) return false;

    atom = beginMovAtom(file, "stts");
    u32 runs = 0;
    for (u32 index = 0; index < movie.frameCount; index++)
        if (index == 0 || movie.durations[index] != movie.durations[index - 1]) runs++;
    writeBe32(file, 0); writeBe32(file, runs);
    for (u32 index = 0; index < movie.frameCount;) {
        u32 length = 1;
        while (index + length < movie.frameCount &&
               movie.durations[index + length] == movie.durations[index]) length++;
        writeBe32(file, length);
        writeBe32(file, movie.durations[index]);
        index += length;
    }
    if (!endMovAtom(file, atom)) return false;
    atom = beginMovAtom(file, "stsc");
    writeBe32(file, 0); writeBe32(file, 1); writeBe32(file, 1); writeBe32(file, 1); writeBe32(file, 1);
    if (!endMovAtom(file, atom)) return false;
    atom = beginMovAtom(file, "stsz");
    writeBe32(file, 0); writeBe32(file, 0); writeBe32(file, movie.frameCount);
    for (u32 i = 0; i < movie.frameCount; i++) writeBe32(file, movie.sizes[i]);
    if (!endMovAtom(file, atom)) return false;
    atom = beginMovAtom(file, "stco");
    writeBe32(file, 0); writeBe32(file, movie.frameCount);
    for (u32 i = 0; i < movie.frameCount; i++) writeBe32(file, movie.offsets[i]);
    if (!endMovAtom(file, atom)) return false;
    atom = beginMovAtom(file, "stss");
    writeBe32(file, 0); writeBe32(file, movie.frameCount);
    for (u32 i = 0; i < movie.frameCount; i++) writeBe32(file, i + 1);
    if (!endMovAtom(file, atom) || !endMovAtom(file, stbl) ||
        !endMovAtom(file, minf) || !endMovAtom(file, mdia) ||
        !endMovAtom(file, trak)) return false;
    if (movie.audioBytes && !writeMovieAudioTrack(file)) return false;
    if (!endMovAtom(file, moov)) return false;
    return !ferror(file);
}

static bool ensureDirectory(const char *path)
{
    struct stat info;
    if (stat(path, &info) == 0) return S_ISDIR(info.st_mode);
    return mkdir(path, 0777) == 0;
}

static bool ensureGalleryDirectory(void)
{
    return ensureDirectory("sdmc:/3ds") && ensureDirectory(LEGACY_DIRECTORY) &&
           ensureDirectory(PHOTO_DIRECTORY);
}

static int compareGameboyFramePaths(const void *left, const void *right)
{
    return strcmp((const char *)left, (const char *)right);
}

static void scanGameboyFrames(void)
{
    gameboyFrameCount = 0;
    gameboyFrameIndex = 0;
    gameboyFrameLoaded = false;
    rebuildGameboyFrameMap();
    gameboyFramesScanned = true;
    if (!ensureDirectory("sdmc:/3ds") || !ensureDirectory(LEGACY_DIRECTORY) ||
        !ensureDirectory(GAMEBOY_FRAME_DIRECTORY)) {
        photoStatus = "FRAME FOLDER ERROR";
        return;
    }
    DIR *directory = opendir(GAMEBOY_FRAME_DIRECTORY);
    if (!directory) {
        photoStatus = "FRAME FOLDER ERROR";
        return;
    }
    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL && gameboyFrameCount < GAMEBOY_MAX_IMPORTED_FRAMES) {
        const char *extension = strrchr(entry->d_name, '.');
        if (!extension || (strcmp(extension, ".png") != 0 && strcmp(extension, ".PNG") != 0)) continue;
        int length = snprintf(gameboyFramePaths[gameboyFrameCount], sizeof(gameboyFramePaths[0]),
                              "%s/%s", GAMEBOY_FRAME_DIRECTORY, entry->d_name);
        if (length >= 0 && (size_t)length < sizeof(gameboyFramePaths[0])) gameboyFrameCount++;
    }
    closedir(directory);
    qsort(gameboyFramePaths, gameboyFrameCount, sizeof(gameboyFramePaths[0]), compareGameboyFramePaths);
}

static void selectGameboyFrame(int direction)
{
    int count = GAMEBOY_BUILTIN_FRAMES + (int)gameboyFrameCount;
    gameboyFrameIndex = (gameboyFrameIndex + count + direction) % count;
    gameboyFrameLoaded = false;
    photoStatus = "";
    if (gameboyFrameIndex < GAMEBOY_BUILTIN_FRAMES) {
        rebuildGameboyFrameMap();
        return;
    }

    png_image image;
    memset(&image, 0, sizeof(image));
    image.version = PNG_IMAGE_VERSION;
    const char *path = gameboyFramePaths[gameboyFrameIndex - GAMEBOY_BUILTIN_FRAMES];
    if (!png_image_begin_read_from_file(&image, path)) {
        png_image_free(&image);
        photoStatus = "FRAME READ FAILED";
        rebuildGameboyFrameMap();
        return;
    }
    if (image.width != GAMEBOY_FRAME_WIDTH || (image.height != 144 && image.height != 224)) {
        png_image_free(&image);
        photoStatus = "BAD FRAME SIZE";
        rebuildGameboyFrameMap();
        return;
    }
    image.format = PNG_FORMAT_RGBA;
    gameboyFrameHeight = (int)image.height;
    gameboyFrameLoaded = png_image_finish_read(&image, NULL, gameboyFramePixels, 0, NULL) != 0;
    png_image_free(&image);
    if (!gameboyFrameLoaded) {
        photoStatus = "FRAME READ FAILED";
    }
    rebuildGameboyFrameMap();
}

static void collectMovieAudio(void)
{
    if (!movie.micSampling) return;
    u32 end = micGetLastSampleOffset();
    if (end >= movie.micDataSize) {
        movie.audioFailed = true;
        return;
    }
    while (movie.micReadPos != end) {
        u32 available = end > movie.micReadPos ? end - movie.micReadPos :
                        movie.micDataSize - movie.micReadPos;
        if (available > MOV_AUDIO_CAPACITY - movie.audioBytes) {
            movie.audioFailed = true;
            return;
        }
        memcpy(movie.audioData + movie.audioBytes, movie.micBuffer + movie.micReadPos, available);
        movie.audioBytes += available;
        movie.micReadPos = (movie.micReadPos + available) % movie.micDataSize;
    }
}

static bool startMovieMicrophone(void)
{
    movie.micBuffer = memalign(0x1000, MIC_BUFFER_SIZE);
    movie.audioData = malloc(MOV_AUDIO_CAPACITY);
    movie.micSampling = false;
    movie.audioFailed = false;
    movie.audioBytes = 0;
    movie.audioOffset = 0;
    movie.micReadPos = 0;
    if (!movie.micBuffer || !movie.audioData) goto fail;
    if (R_FAILED(micInit(movie.micBuffer, MIC_BUFFER_SIZE))) goto fail;
    movie.micDataSize = micGetSampleDataSize();
    if (!movie.micDataSize || R_FAILED(MICU_StartSampling(MICU_ENCODING_PCM16_SIGNED,
        MICU_SAMPLE_RATE_16360, 0, movie.micDataSize, true))) {
        micExit();
        goto fail;
    }
    movie.micSampling = true;
    movie.audioStartTick = svcGetSystemTick();
    return true;

fail:
    free(movie.micBuffer);
    free(movie.audioData);
    movie.micBuffer = NULL;
    movie.audioData = NULL;
    return false;
}

static void stopMovieMicrophone(void)
{
    if (movie.micSampling) {
        if (R_FAILED(MICU_StopSampling())) movie.audioFailed = true;
        collectMovieAudio();
        micExit();
        movie.micSampling = false;
    }
    free(movie.micBuffer);
    movie.micBuffer = NULL;
}

static void movieWorker(void *argument);

static bool startMovie(void)
{
    photoStatus = "VIDEO START FAILED";
    if (!ensureGalleryDirectory()) return false;
    for (unsigned attempt = 0; attempt < 10000; attempt++) {
        snprintf(movie.path, sizeof(movie.path), PHOTO_DIRECTORY "/VID_%04u.mov", videoNumber++);
        FILE *existing = fopen(movie.path, "rb");
        if (existing) { fclose(existing); continue; }
        movie.file = fopen(movie.path, "wb+");
        if (!movie.file) return false;
        break;
    }
    if (!movie.file) return false;
    memset(movie.offsets, 0, sizeof(movie.offsets));
    memset(movie.sizes, 0, sizeof(movie.sizes));
    memset(movie.durations, 0, sizeof(movie.durations));
    movie.frameCount = 0;
    writeBe32(movie.file, 20); fwrite("ftypqt  ", 1, 8, movie.file);
    writeBe32(movie.file, 0x00000200); fwrite("qt  ", 1, 4, movie.file);
    movie.mdatOffset = ftell(movie.file);
    writeBe32(movie.file, 0); fwrite("mdat", 1, 4, movie.file);
    if (ferror(movie.file) || movie.mdatOffset < 0) {
        fclose(movie.file); remove(movie.path); movie.file = NULL; return false;
    }
    if (!startMovieMicrophone()) {
        fclose(movie.file); remove(movie.path); movie.file = NULL;
        photoStatus = "MIC UNAVAILABLE";
        return false;
    }
    movie.startTick = svcGetSystemTick();
    movie.firstFrameTick = 0;
    movie.lastFrameTick = movie.startTick;
    movie.lastQueuedTick = movie.startTick;
    movie.queueHead = 0;
    movie.queueTail = 0;
    movie.queueCount = 0;
    movie.busy = false;
    movie.stopRequested = false;
    movie.failed = false;
    movie.worker = NULL;
    movie.cpuLimitChanged = false;
    LightLock_Init(&movie.lock);
    LightEvent_Init(&movie.event, RESET_ONESHOT);
    if (R_SUCCEEDED(APT_GetAppCpuTimeLimit(&movie.previousCpuLimit)) &&
        R_SUCCEEDED(APT_SetAppCpuTimeLimit(80))) {
        movie.cpuLimitChanged = true;
        movie.worker = threadCreate(movieWorker, NULL, 64 * 1024, 0x30, 1, false);
    }
    if (!movie.worker) {
        if (movie.cpuLimitChanged) APT_SetAppCpuTimeLimit(movie.previousCpuLimit);
        movie.cpuLimitChanged = false;
        movie.worker = threadCreate(movieWorker, NULL, 64 * 1024, 0x31, 0, false);
    }
    if (!movie.worker) {
        stopMovieMicrophone();
        free(movie.audioData);
        movie.audioData = NULL;
        fclose(movie.file);
        remove(movie.path);
        movie.file = NULL;
        return false;
    }
    movie.active = true;
    photoStatus = "";
    return true;
}

static u32 movieElapsedMs(u64 start, u64 end)
{
    return (u32)(((end - start) * 1000ULL) / SYSCLOCK_ARM11);
}

static void movieWorker(void *argument)
{
    (void)argument;
    for (;;) {
        LightLock_Lock(&movie.lock);
        if (!movie.queueCount) {
            bool stop = movie.stopRequested;
            LightLock_Unlock(&movie.lock);
            if (stop) break;
            LightEvent_Wait(&movie.event);
            continue;
        }
        unsigned buffer = movie.queueHead;
        movie.queueHead = (movie.queueHead + 1) % MOV_QUEUE_CAPACITY;
        movie.queueCount--;
        movie.busy = true;
        u64 frameTick = movie.frameTicks[buffer];
        LightLock_Unlock(&movie.lock);

        unsigned char *data = NULL;
        unsigned long size = 0;
        bool ok = encodeJpeg(movie.frameBuffers[buffer], &data, &size, 86, MOV_WIDTH, MOV_HEIGHT) &&
                  size <= 0xffffffffUL;
        long offset = ok ? ftell(movie.file) : -1;
        if (offset < 0 || (unsigned long)offset > 0xffffffffUL ||
            (ok && fwrite(data, 1, size, movie.file) != size)) ok = false;
        free(data);

        LightLock_Lock(&movie.lock);
        if (ok) {
            u32 index = movie.frameCount;
            movie.offsets[index] = (u32)offset;
            movie.sizes[index] = (u32)size;
            if (!index) {
                movie.firstFrameTick = frameTick;
            } else {
                u32 elapsed = movieElapsedMs(movie.lastFrameTick, frameTick);
                movie.durations[index - 1] = elapsed ? elapsed : 1;
            }
            movie.lastFrameTick = frameTick;
            movie.frameCount++;
        } else {
            movie.failed = true;
        }
        movie.busy = false;
        bool stop = movie.failed || (movie.stopRequested && !movie.queueCount);
        LightLock_Unlock(&movie.lock);
        if (stop) break;
    }
}

static bool finishMovie(void)
{
    if (!movie.active || !movie.file) return false;
    u64 stopTick = svcGetSystemTick();
    movie.active = false;
    stopMovieMicrophone();
    LightLock_Lock(&movie.lock);
    movie.stopRequested = true;
    LightLock_Unlock(&movie.lock);
    LightEvent_Signal(&movie.event);
    threadJoin(movie.worker, U64_MAX);
    threadFree(movie.worker);
    movie.worker = NULL;
    if (movie.cpuLimitChanged) APT_SetAppCpuTimeLimit(movie.previousCpuLimit);
    movie.cpuLimitChanged = false;
    if (movie.failed) photoStatus = "VIDEO WRITE FAILED";
    if (!movie.frameCount) {
        fclose(movie.file); remove(movie.path); movie.file = NULL;
        free(movie.audioData); movie.audioData = NULL; return false;
    }
    u32 lastDuration = movieElapsedMs(movie.lastFrameTick, stopTick);
    movie.durations[movie.frameCount - 1] = lastDuration ? lastDuration : 1;
    movie.audioBytes &= ~1U;
    u32 audioSkip = 0;
    if (movie.firstFrameTick > movie.audioStartTick) {
        u64 skippedSamples = (movie.firstFrameTick - movie.audioStartTick) * MOV_AUDIO_RATE / SYSCLOCK_ARM11;
        if (skippedSamples > movie.audioBytes / 2) skippedSamples = movie.audioBytes / 2;
        audioSkip = (u32)skippedSamples * 2;
        movie.audioBytes -= audioSkip;
    }
    u64 videoSamples = (stopTick - movie.firstFrameTick) * MOV_AUDIO_RATE / SYSCLOCK_ARM11;
    if (videoSamples < movie.audioBytes / 2) movie.audioBytes = (u32)videoSamples * 2;
    long audioOffset = ftell(movie.file);
    if (audioOffset < 0 || (unsigned long)audioOffset > 0xffffffffUL ||
        (movie.audioBytes && fwrite(movie.audioData + audioSkip, 1, movie.audioBytes, movie.file) != movie.audioBytes)) {
        fclose(movie.file); remove(movie.path); movie.file = NULL;
        free(movie.audioData); movie.audioData = NULL;
        photoStatus = "VIDEO WRITE FAILED";
        return false;
    }
    movie.audioOffset = (u32)audioOffset;
    if (movie.audioFailed || !movie.audioBytes) photoStatus = "AUDIO INCOMPLETE";
    long end = ftell(movie.file);
    if (end < movie.mdatOffset || fseek(movie.file, movie.mdatOffset, SEEK_SET) != 0) {
        fclose(movie.file); movie.file = NULL;
        free(movie.audioData); movie.audioData = NULL; return false;
    }
    writeBe32(movie.file, (u32)(end - movie.mdatOffset));
    bool ok = fseek(movie.file, end, SEEK_SET) == 0 && writeMovieIndex(movie.file) && !ferror(movie.file);
    if (fclose(movie.file) != 0) ok = false;
    movie.file = NULL;
    if (!ok) remove(movie.path);
    free(movie.audioData);
    movie.audioData = NULL;
    return ok && !movie.failed;
}

static void appendMovieFrame(void)
{
    if (!movie.active || !movie.file) return;
    u64 now = svcGetSystemTick();
    LightLock_Lock(&movie.lock);
    bool done = movie.failed || movie.frameCount >= MOV_MAX_FRAMES ||
                movieElapsedMs(movie.startTick, now) >= MOV_MAX_DURATION_MS;
    u32 framesInFlight = movie.frameCount + movie.queueCount + (movie.busy ? 1 : 0);
    bool ready = !done && movie.queueCount + (movie.busy ? 1 : 0) < MOV_QUEUE_CAPACITY &&
                 framesInFlight < MOV_MAX_FRAMES &&
                 (!framesInFlight || movieElapsedMs(movie.lastQueuedTick, now) >= MOV_FRAME_INTERVAL_MS);
    if (ready) {
        unsigned buffer = movie.queueTail;
        memcpy(movie.frameBuffers[buffer], processedFrame, sizeof(movie.frameBuffers[0]));
        movie.frameTicks[buffer] = now;
        movie.queueTail = (movie.queueTail + 1) % MOV_QUEUE_CAPACITY;
        movie.queueCount++;
        movie.lastQueuedTick = now;
    }
    LightLock_Unlock(&movie.lock);
    if (done) finishMovie();
    else if (ready) LightEvent_Signal(&movie.event);
}

static bool encodeJpeg(const u16 *pixels, unsigned char **output, unsigned long *outputSize, int quality,
                       int width, int height)
{
    struct jpeg_compress_struct compressor;
    struct jpeg_error_mgr errorManager;
    u8 *row = malloc(width * 3);
    if (!row) return false;
    int sourceColumns[FRAME_WIDTH];
    for (int x = 0; x < width; x++) sourceColumns[x] = x * FRAME_WIDTH / width;

    memset(&compressor, 0, sizeof(compressor));
    compressor.err = jpeg_std_error(&errorManager);
    jpeg_create_compress(&compressor);
    jpeg_mem_dest(&compressor, output, outputSize);
    compressor.image_width = width;
    compressor.image_height = height;
    compressor.input_components = 3;
    compressor.in_color_space = JCS_RGB;
    jpeg_set_defaults(&compressor);
    jpeg_set_quality(&compressor, quality, TRUE);
    if (quality <= 88) compressor.dct_method = JDCT_IFAST;
    jpeg_start_compress(&compressor, TRUE);

    while (compressor.next_scanline < compressor.image_height) {
        int y = compressor.next_scanline * FRAME_HEIGHT / height;
        for (int x = 0; x < width; x++) {
            u16 pixel = pixels[y * FRAME_WIDTH + sourceColumns[x]];
            row[x * 3] = (u8)((pixel & 0x1f) * 255 / 31);
            row[x * 3 + 1] = (u8)(((pixel >> 5) & 0x3f) * 255 / 63);
            row[x * 3 + 2] = (u8)(((pixel >> 11) & 0x1f) * 255 / 31);
        }
        JSAMPROW rowPointer = row;
        jpeg_write_scanlines(&compressor, &rowPointer, 1);
    }
    jpeg_finish_compress(&compressor);
    jpeg_destroy_compress(&compressor);
    free(row);
    return *output != NULL && *outputSize > 0;
}

static bool writeJpeg(const u16 *pixels, const char *path)
{
    unsigned char *data = NULL;
    unsigned long size = 0;
    if (!encodeJpeg(pixels, &data, &size, 88, FRAME_WIDTH, FRAME_HEIGHT)) return false;

    FILE *file = fopen(path, "wb");
    if (!file) {
        free(data);
        return false;
    }
    bool ok = fwrite(data, 1, size, file) == size && !ferror(file);
    if (fclose(file) != 0) ok = false;
    free(data);
    if (!ok) remove(path);
    return ok;
}

static bool preparePhotoPath(char *path, size_t pathSize)
{
    if (!ensureGalleryDirectory()) return false;
    for (unsigned attempt = 0; attempt < 10000; attempt++) {
        char legacyPath[96];
        struct stat info;
        unsigned number = photoNumber++;
        int length = snprintf(path, pathSize, PHOTO_DIRECTORY "/PIC_%04u.jpg", number);
        if (length < 0 || (size_t)length >= pathSize) return false;
        if (stat(path, &info) == 0) continue;
        snprintf(legacyPath, sizeof(legacyPath), LEGACY_DIRECTORY "/PIC_%04u.jpg", number);
        if (stat(legacyPath, &info) == 0) continue;
        snprintf(legacyPath, sizeof(legacyPath), LEGACY_DIRECTORY "/PIC_%04u.bmp", number);
        if (stat(legacyPath, &info) == 0) continue;
        return true;
    }
    return false;
}

static void photoSaveWorker(void *argument)
{
    (void)argument;
    photoSave.saved = writeJpeg(photoSave.frame, photoSave.path);
}

static bool startPhotoSave(void)
{
    if (photoSave.active || !preparePhotoPath(photoSave.path, sizeof(photoSave.path))) return false;
    memcpy(photoSave.frame, processedFrame, sizeof(photoSave.frame));
    photoSave.saved = false;
    photoSave.done = false;
    photoSave.startTick = svcGetSystemTick();
    photoSave.worker = threadCreate(photoSaveWorker, NULL, 64 * 1024, 0x30, 0, false);
    if (!photoSave.worker) return false;
    photoSave.active = true;
    return true;
}

static int compareGalleryFiles(const void *left, const void *right)
{
    const char *leftPath = left;
    const char *rightPath = right;
    const char *leftName = strrchr(leftPath, '/');
    const char *rightName = strrchr(rightPath, '/');
    int result = strcmp(leftName ? leftName + 1 : leftPath,
                        rightName ? rightName + 1 : rightPath);
    return result ? result : strcmp(leftPath, rightPath);
}

static void scanGalleryDirectory(const char *path)
{
    DIR *directory = opendir(path);
    if (!directory) return;

    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL) {
        if (!isGalleryMedia(entry->d_name)) continue;
        size_t prefixSize = strlen(path) + 1;
        size_t nameSize = strlen(entry->d_name);
        char candidate[sizeof(galleryFiles[0])];
        if (prefixSize + nameSize >= sizeof(candidate)) continue;
        memcpy(candidate, path, prefixSize - 1);
        candidate[prefixSize - 1] = '/';
        memcpy(candidate + prefixSize, entry->d_name, nameSize + 1);
        size_t slot = galleryCount;
        if (galleryCount == MAX_GALLERY_ITEMS) {
            slot = 0;
            for (size_t index = 1; index < galleryCount; index++)
                if (compareGalleryFiles(galleryFiles[index], galleryFiles[slot]) < 0) slot = index;
            if (compareGalleryFiles(candidate, galleryFiles[slot]) <= 0) continue;
        } else {
            galleryCount++;
        }
        memcpy(galleryFiles[slot], candidate, prefixSize + nameSize + 1);
    }
    closedir(directory);
}

static void scanGallery(void)
{
    galleryCount = 0;
    galleryPageStart = (size_t)-1;
    scanGalleryDirectory(PHOTO_DIRECTORY);
    scanGalleryDirectory(LEGACY_DIRECTORY);
    qsort(galleryFiles, galleryCount, sizeof(galleryFiles[0]), compareGalleryFiles);
}

static u16 readLe16(const u8 *bytes)
{
    return (u16)(bytes[0] | ((u16)bytes[1] << 8));
}

static u32 readLe32(const u8 *bytes)
{
    return (u32)bytes[0] | ((u32)bytes[1] << 8) | ((u32)bytes[2] << 16) | ((u32)bytes[3] << 24);
}

typedef struct {
    struct jpeg_error_mgr manager;
    jmp_buf recovery;
} GalleryJpegError;

static void galleryJpegErrorExit(j_common_ptr info)
{
    GalleryJpegError *error = (GalleryJpegError *)info->err;
    longjmp(error->recovery, 1);
}

static bool decodeGalleryJpeg(FILE *file, const unsigned char *data, unsigned long size)
{
    struct jpeg_decompress_struct decompressor;
    GalleryJpegError errorManager;
    memset(&decompressor, 0, sizeof(decompressor));
    decompressor.err = jpeg_std_error(&errorManager.manager);
    errorManager.manager.error_exit = galleryJpegErrorExit;
    if (setjmp(errorManager.recovery)) {
        jpeg_destroy_decompress(&decompressor);
        return false;
    }
    jpeg_create_decompress(&decompressor);
    if (data) jpeg_mem_src(&decompressor, data, size);
    else jpeg_stdio_src(&decompressor, file);
    if (jpeg_read_header(&decompressor, TRUE) != JPEG_HEADER_OK) {
        jpeg_destroy_decompress(&decompressor);
        return false;
    }
    decompressor.out_color_space = JCS_RGB;
    jpeg_start_decompress(&decompressor);
    if (!decompressor.output_width || !decompressor.output_height ||
        decompressor.output_width > FRAME_WIDTH || decompressor.output_height > FRAME_HEIGHT ||
        decompressor.output_components != 3) {
        jpeg_destroy_decompress(&decompressor);
        return false;
    }
    u8 row[FRAME_WIDTH * 3];
    int targetY = 0;
    while (decompressor.output_scanline < decompressor.output_height) {
        JSAMPROW rowPointer = row;
        int sourceY = decompressor.output_scanline;
        jpeg_read_scanlines(&decompressor, &rowPointer, 1);
        while (targetY < FRAME_HEIGHT &&
               targetY * (int)decompressor.output_height / FRAME_HEIGHT == sourceY) {
            for (int x = 0; x < FRAME_WIDTH; x++) {
                int sourceX = x * (int)decompressor.output_width / FRAME_WIDTH;
                u8 red = row[sourceX * 3];
                u8 green = row[sourceX * 3 + 1];
                u8 blue = row[sourceX * 3 + 2];
                galleryFrame[targetY * FRAME_WIDTH + x] =
                    (u16)(((blue >> 3) << 11) | ((green >> 2) << 5) | (red >> 3));
            }
            targetY++;
        }
    }
    jpeg_finish_decompress(&decompressor);
    jpeg_destroy_decompress(&decompressor);
    return targetY == FRAME_HEIGHT;
}

static void greenScreenCrop(int width, int height, int *left, int *top, int *cropWidth, int *cropHeight)
{
    *cropWidth = width;
    *cropHeight = width * FRAME_HEIGHT / FRAME_WIDTH;
    if (*cropHeight > height) {
        *cropHeight = height;
        *cropWidth = height * FRAME_WIDTH / FRAME_HEIGHT;
    }
    if (*cropWidth < 1) *cropWidth = 1;
    if (*cropHeight < 1) *cropHeight = 1;
    *left = (width - *cropWidth) / 2;
    *top = (height - *cropHeight) / 2;
}

static bool decodeGreenScreenJpeg(FILE *file, const u8 *data, unsigned long size)
{
    struct jpeg_decompress_struct decompressor;
    GalleryJpegError errorManager;
    memset(&decompressor, 0, sizeof(decompressor));
    decompressor.err = jpeg_std_error(&errorManager.manager);
    errorManager.manager.error_exit = galleryJpegErrorExit;
    if (setjmp(errorManager.recovery)) {
        jpeg_destroy_decompress(&decompressor);
        return false;
    }
    jpeg_create_decompress(&decompressor);
    if (data) jpeg_mem_src(&decompressor, data, size);
    else jpeg_stdio_src(&decompressor, file);
    if (jpeg_read_header(&decompressor, TRUE) != JPEG_HEADER_OK) {
        jpeg_destroy_decompress(&decompressor);
        return false;
    }
    decompressor.out_color_space = JCS_RGB;
    decompressor.scale_num = 1;
    decompressor.scale_denom = 1;
    while (decompressor.scale_denom < 8 &&
           (decompressor.image_width / decompressor.scale_denom > 800 ||
            decompressor.image_height / decompressor.scale_denom > 480))
        decompressor.scale_denom *= 2;
    jpeg_start_decompress(&decompressor);
    int width = (int)decompressor.output_width;
    int height = (int)decompressor.output_height;
    if (width <= 0 || height <= 0 || width > 2048 || height > 2048 || decompressor.output_components != 3) {
        jpeg_destroy_decompress(&decompressor);
        return false;
    }
    int left, top, cropWidth, cropHeight;
    greenScreenCrop(width, height, &left, &top, &cropWidth, &cropHeight);
    JSAMPARRAY row = (*decompressor.mem->alloc_sarray)((j_common_ptr)&decompressor, JPOOL_IMAGE, width * 3, 1);
    int targetY = 0;
    while (decompressor.output_scanline < decompressor.output_height) {
        int sourceY = (int)decompressor.output_scanline;
        jpeg_read_scanlines(&decompressor, row, 1);
        while (targetY < FRAME_HEIGHT && top + targetY * cropHeight / FRAME_HEIGHT == sourceY) {
            for (int x = 0; x < FRAME_WIDTH; x++) {
                int sourceX = left + x * cropWidth / FRAME_WIDTH;
                u8 red = row[0][sourceX * 3];
                u8 green = row[0][sourceX * 3 + 1];
                u8 blue = row[0][sourceX * 3 + 2];
                greenScreenScratch[targetY * FRAME_WIDTH + x] =
                    (u16)(((blue >> 3) << 11) | ((green >> 2) << 5) | (red >> 3));
            }
            targetY++;
        }
    }
    jpeg_finish_decompress(&decompressor);
    jpeg_destroy_decompress(&decompressor);
    return targetY == FRAME_HEIGHT;
}

static bool decodeGreenScreenPng(const char *path)
{
    png_image image;
    memset(&image, 0, sizeof(image));
    image.version = PNG_IMAGE_VERSION;
    if (!png_image_begin_read_from_file(&image, path)) {
        png_image_free(&image);
        return false;
    }
    if (!image.width || !image.height || image.width > 4096 || image.height > 4096 ||
        (u64)image.width * image.height > 4000000) {
        png_image_free(&image);
        return false;
    }
    image.format = PNG_FORMAT_RGBA;
    u8 *pixels = malloc(PNG_IMAGE_SIZE(image));
    if (!pixels) {
        png_image_free(&image);
        return false;
    }
    bool ok = png_image_finish_read(&image, NULL, pixels, 0, NULL) != 0;
    if (ok) {
        int left, top, cropWidth, cropHeight;
        greenScreenCrop((int)image.width, (int)image.height, &left, &top, &cropWidth, &cropHeight);
        for (int y = 0; y < FRAME_HEIGHT; y++) {
            int sourceY = top + y * cropHeight / FRAME_HEIGHT;
            for (int x = 0; x < FRAME_WIDTH; x++) {
                int sourceX = left + x * cropWidth / FRAME_WIDTH;
                const u8 *pixel = pixels + ((size_t)sourceY * image.width + sourceX) * 4;
                int red = pixel[0] * pixel[3] / 255;
                int green = pixel[1] * pixel[3] / 255;
                int blue = pixel[2] * pixel[3] / 255;
                greenScreenScratch[y * FRAME_WIDTH + x] =
                    (u16)(((blue >> 3) << 11) | ((green >> 2) << 5) | (red >> 3));
            }
        }
    }
    free(pixels);
    png_image_free(&image);
    return ok;
}

static int compareGreenScreenPaths(const void *left, const void *right)
{
    return strcmp((const char *)left, (const char *)right);
}

static bool scanGreenScreenBackgrounds(void)
{
    greenScreenImportedCount = 0;
    greenScreenPageStart = 0;
    memset(greenScreenThumbnailValid, 0, sizeof(greenScreenThumbnailValid));
    memset(greenScreenThumbnailAttempted, 0, sizeof(greenScreenThumbnailAttempted));
    greenScreenBackgroundsScanned = true;
    if (!ensureDirectory("sdmc:/3ds") || !ensureDirectory(LEGACY_DIRECTORY) ||
        !ensureDirectory(GREEN_BACKGROUND_DIRECTORY)) {
        photoStatus = "BACKGROUND FOLDER ERROR";
        return false;
    }
    DIR *directory = opendir(GREEN_BACKGROUND_DIRECTORY);
    if (!directory) {
        photoStatus = "BACKGROUND FOLDER ERROR";
        return false;
    }
    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL && greenScreenImportedCount < GREEN_MAX_IMPORTED_BACKGROUNDS) {
        const char *extension = strrchr(entry->d_name, '.');
        if (!extension || (strcasecmp(extension, ".png") && strcasecmp(extension, ".jpg") &&
                           strcasecmp(extension, ".jpeg"))) continue;
        int length = snprintf(greenScreenPaths[greenScreenImportedCount], sizeof(greenScreenPaths[0]),
                              "%s/%s", GREEN_BACKGROUND_DIRECTORY, entry->d_name);
        if (length >= 0 && (size_t)length < sizeof(greenScreenPaths[0])) greenScreenImportedCount++;
    }
    closedir(directory);
    qsort(greenScreenPaths, greenScreenImportedCount, sizeof(greenScreenPaths[0]), compareGreenScreenPaths);
    return true;
}

static bool decodeGreenScreenBackground(int index)
{
    int count = GREEN_BUILTIN_BACKGROUNDS + (int)greenScreenImportedCount;
    if (index < 0 || index >= count) return false;
    bool loaded = false;
    if (index < GREEN_BUILTIN_BACKGROUNDS) {
        const u8 *starts[] = {bg_beach_bin, bg_road_bin, bg_earth_bin, bg_mountains_bin};
        const u8 *ends[] = {bg_beach_bin_end, bg_road_bin_end, bg_earth_bin_end, bg_mountains_bin_end};
        unsigned long size = (unsigned long)((uintptr_t)ends[index] - (uintptr_t)starts[index]);
        loaded = decodeGreenScreenJpeg(NULL, starts[index], size);
    } else {
        const char *path = greenScreenPaths[index - GREEN_BUILTIN_BACKGROUNDS];
        const char *extension = strrchr(path, '.');
        if (extension && !strcasecmp(extension, ".png")) {
            loaded = decodeGreenScreenPng(path);
        } else {
            FILE *file = fopen(path, "rb");
            if (file) {
                loaded = decodeGreenScreenJpeg(file, NULL, 0);
                fclose(file);
            }
        }
    }
    return loaded;
}

static void cacheGreenScreenThumbnail(int index)
{
    for (int y = 0; y < GREEN_THUMB_HEIGHT; y++) {
        for (int x = 0; x < GREEN_THUMB_WIDTH; x++) {
            int sourceX = x * FRAME_WIDTH / GREEN_THUMB_WIDTH;
            int sourceY = y * FRAME_HEIGHT / GREEN_THUMB_HEIGHT;
            greenScreenThumbnails[index][y * GREEN_THUMB_WIDTH + x] =
                greenScreenScratch[sourceY * FRAME_WIDTH + sourceX];
        }
    }
    greenScreenThumbnailValid[index] = true;
    greenScreenThumbnailAttempted[index] = true;
}

static void selectGreenScreenBackground(int index)
{
    if (!decodeGreenScreenBackground(index)) {
        photoStatus = "BACKGROUND READ FAILED";
        return;
    }
    memcpy(greenScreenBackground, greenScreenScratch, sizeof(greenScreenBackground));
    cacheGreenScreenThumbnail(index);
    greenScreenBackgroundIndex = index;
    greenScreenBackgroundReady = true;
    photoStatus = "";
}

static void updateGreenScreenThumbnails(void)
{
    if (movie.active || !greenScreenBackgroundReady) return;
    int total = GREEN_BUILTIN_BACKGROUNDS + (int)greenScreenImportedCount;
    for (int index = greenScreenPageStart;
         index < greenScreenPageStart + GREEN_BACKGROUND_PAGE_SIZE && index < total; index++) {
        if (greenScreenThumbnailAttempted[index]) continue;
        greenScreenThumbnailAttempted[index] = true;
        if (decodeGreenScreenBackground(index)) cacheGreenScreenThumbnail(index);
        break;
    }
}

typedef struct {
    long start;
    long end;
} MovieAtom;

static bool readMovieU32(FILE *file, u32 *value)
{
    u8 bytes[4];
    if (fread(bytes, 1, sizeof(bytes), file) != sizeof(bytes)) return false;
    *value = ((u32)bytes[0] << 24) | ((u32)bytes[1] << 16) | ((u32)bytes[2] << 8) | bytes[3];
    return true;
}

static bool findMovieAtom(FILE *file, long start, long end, const char *type, MovieAtom *atom)
{
    for (long position = start; position <= end - 8;) {
        u32 size;
        char name[4];
        if (fseek(file, position, SEEK_SET) != 0 || !readMovieU32(file, &size) ||
            fread(name, 1, sizeof(name), file) != sizeof(name) ||
            size < 8 || size > (u32)(end - position)) return false;
        if (memcmp(name, type, 4) == 0) {
            atom->start = position + 8;
            atom->end = position + (long)size;
            return true;
        }
        position += (long)size;
    }
    return false;
}

static void closeGalleryVideo(void)
{
    if (galleryVideo.audioReady) {
        ndspChnWaveBufClear(0);
        ndspExit();
        galleryVideo.audioReady = false;
    }
    if (galleryVideo.audioData) linearFree(galleryVideo.audioData);
    galleryVideo.audioData = NULL;
    galleryVideo.audioBytes = 0;
    galleryVideo.audioInvalid = false;
    if (galleryVideo.file) fclose(galleryVideo.file);
    galleryVideo.file = NULL;
    galleryVideo.frameCount = 0;
    galleryVideo.playing = false;
    galleryVideo.finished = false;
    galleryVideo.elapsedMs = 0;
}

static bool decodeGalleryVideoFrame(u32 index)
{
    if (!galleryVideo.file || index >= galleryVideo.frameCount) return false;
    u32 size = galleryVideo.sizes[index];
    unsigned char *data = malloc(size);
    if (!data) return false;
    bool ok = fseek(galleryVideo.file, (long)galleryVideo.offsets[index], SEEK_SET) == 0 &&
              fread(data, 1, size, galleryVideo.file) == size &&
              decodeGalleryJpeg(NULL, data, size);
    free(data);
    if (ok) {
        galleryVideo.frameIndex = index;
        galleryImageLoaded = true;
    }
    return ok;
}

static bool readGalleryAudioTrack(FILE *file, MovieAtom moov, MovieAtom videoTrak, MovieAtom mdat)
{
    MovieAtom trak, mdia, minf, stbl, stsd, sample, stsz, stco, stsc;
    if (!findMovieAtom(file, videoTrak.end, moov.end, "trak", &trak)) return true;
    if (!findMovieAtom(file, trak.start, trak.end, "mdia", &mdia) ||
        !findMovieAtom(file, mdia.start, mdia.end, "minf", &minf) ||
        !findMovieAtom(file, minf.start, minf.end, "stbl", &stbl) ||
        !findMovieAtom(file, stbl.start, stbl.end, "stsd", &stsd) || stsd.end - stsd.start < 8 ||
        !findMovieAtom(file, stsd.start + 8, stsd.end, "sowt", &sample) ||
        sample.end - sample.start < 28 || !findMovieAtom(file, stbl.start, stbl.end, "stsz", &stsz) ||
        !findMovieAtom(file, stbl.start, stbl.end, "stco", &stco) ||
        !findMovieAtom(file, stbl.start, stbl.end, "stsc", &stsc)) return false;

    u8 sampleEntry[28];
    if (fseek(file, sample.start, SEEK_SET) != 0 ||
        fread(sampleEntry, 1, sizeof(sampleEntry), file) != sizeof(sampleEntry) ||
        sampleEntry[16] != 0 || sampleEntry[17] != 1 ||
        sampleEntry[18] != 0 || sampleEntry[19] != 16) return false;
    u32 rate = ((u32)sampleEntry[24] << 8) | sampleEntry[25];
    if (!rate || rate > 48000) return false;

    u32 version, sampleSize, samples, count, value, offset;
    if (stsz.end - stsz.start < 12 || fseek(file, stsz.start, SEEK_SET) != 0 ||
        !readMovieU32(file, &version) || version != 0 || !readMovieU32(file, &sampleSize) ||
        sampleSize != 2 || !readMovieU32(file, &samples) || !samples ||
        samples > MOV_AUDIO_CAPACITY / 2) return false;
    if (stsc.end - stsc.start < 20 || fseek(file, stsc.start, SEEK_SET) != 0 ||
        !readMovieU32(file, &version) || version != 0 || !readMovieU32(file, &count) || count != 1 ||
        !readMovieU32(file, &value) || value != 1 || !readMovieU32(file, &value) || value != samples ||
        !readMovieU32(file, &value) || value != 1) return false;
    if (stco.end - stco.start < 12 || fseek(file, stco.start, SEEK_SET) != 0 ||
        !readMovieU32(file, &version) || version != 0 || !readMovieU32(file, &count) || count != 1 ||
        !readMovieU32(file, &offset) || offset < (u32)mdat.start || offset > (u32)mdat.end ||
        samples * 2 > (u32)mdat.end - offset) return false;
    galleryVideo.audioOffset = offset;
    galleryVideo.audioBytes = samples * 2;
    galleryVideo.audioRate = rate;
    return true;
}

static bool startGalleryAudio(void)
{
    if (!galleryVideo.audioBytes) {
        if (!galleryVideo.audioInvalid) photoStatus = "NO AUDIO TRACK";
        return !galleryVideo.audioInvalid;
    }
    if (!galleryVideo.audioReady) {
        if (R_FAILED(ndspInit())) return false;
        galleryVideo.audioReady = true;
        galleryVideo.audioData = linearAlloc(galleryVideo.audioBytes);
        if (!galleryVideo.audioData || fseek(galleryVideo.file, (long)galleryVideo.audioOffset, SEEK_SET) != 0 ||
            fread(galleryVideo.audioData, 1, galleryVideo.audioBytes, galleryVideo.file) != galleryVideo.audioBytes ||
            R_FAILED(DSP_FlushDataCache(galleryVideo.audioData, galleryVideo.audioBytes))) {
            ndspExit();
            galleryVideo.audioReady = false;
            if (galleryVideo.audioData) linearFree(galleryVideo.audioData);
            galleryVideo.audioData = NULL;
            return false;
        }
        ndspSetOutputMode(NDSP_OUTPUT_STEREO);
        ndspChnReset(0);
        ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
        ndspChnSetRate(0, (float)galleryVideo.audioRate);
        ndspChnSetFormat(0, NDSP_FORMAT_MONO_PCM16);
        float mix[12] = {1.0f, 1.0f};
        ndspChnSetMix(0, mix);
    }
    ndspChnWaveBufClear(0);
    memset(&galleryVideo.audioWave, 0, sizeof(galleryVideo.audioWave));
    galleryVideo.audioWave.data_pcm16 = (s16 *)galleryVideo.audioData;
    galleryVideo.audioWave.nsamples = galleryVideo.audioBytes / 2;
    ndspChnSetPaused(0, false);
    ndspChnWaveBufAdd(0, &galleryVideo.audioWave);
    return true;
}

static bool openGalleryVideo(FILE *file)
{
    MovieAtom moov, trak, mdia, minf, stbl, stsd, jpeg, stsc, stsz, stco, stts, mdhd, mdat;
    if (fseek(file, 0, SEEK_END) != 0) return false;
    long fileSize = ftell(file);
    if (fileSize < 8 || !findMovieAtom(file, 0, fileSize, "mdat", &mdat) ||
        !findMovieAtom(file, 0, fileSize, "moov", &moov) ||
        !findMovieAtom(file, moov.start, moov.end, "trak", &trak) ||
        !findMovieAtom(file, trak.start, trak.end, "mdia", &mdia) ||
        !findMovieAtom(file, mdia.start, mdia.end, "mdhd", &mdhd) ||
        !findMovieAtom(file, mdia.start, mdia.end, "minf", &minf) ||
        !findMovieAtom(file, minf.start, minf.end, "stbl", &stbl) ||
        !findMovieAtom(file, stbl.start, stbl.end, "stsd", &stsd) ||
        stsd.end - stsd.start < 8 ||
        !findMovieAtom(file, stsd.start + 8, stsd.end, "jpeg", &jpeg) ||
        !findMovieAtom(file, stbl.start, stbl.end, "stsc", &stsc) ||
        !findMovieAtom(file, stbl.start, stbl.end, "stsz", &stsz) ||
        !findMovieAtom(file, stbl.start, stbl.end, "stco", &stco) ||
        !findMovieAtom(file, stbl.start, stbl.end, "stts", &stts)) return false;

    u32 version, count, value, sampleSize, timeScale;
    if (mdhd.end - mdhd.start < 20 || fseek(file, mdhd.start, SEEK_SET) != 0 ||
        !readMovieU32(file, &version) || version != 0 || fseek(file, mdhd.start + 12, SEEK_SET) != 0 ||
        !readMovieU32(file, &timeScale) || !timeScale) return false;
    if (stsc.end - stsc.start < 20 || fseek(file, stsc.start, SEEK_SET) != 0 ||
        !readMovieU32(file, &version) || version != 0 || !readMovieU32(file, &count) || count != 1 ||
        !readMovieU32(file, &value) || value != 1 || !readMovieU32(file, &value) || value != 1 ||
        !readMovieU32(file, &value) || value != 1) return false;
    if (stsz.end - stsz.start < 12 || fseek(file, stsz.start, SEEK_SET) != 0 ||
        !readMovieU32(file, &version) || version != 0 || !readMovieU32(file, &sampleSize) ||
        !readMovieU32(file, &count) || !count || count > MOV_MAX_FRAMES ||
        (!sampleSize && stsz.end - stsz.start - 12 < (long)count * 4)) return false;
    galleryVideo.frameCount = count;
    for (u32 index = 0; index < count; index++) {
        u32 size = sampleSize;
        if ((!sampleSize && !readMovieU32(file, &size)) || !size || size > 4 * 1024 * 1024) return false;
        galleryVideo.sizes[index] = size;
    }
    if (stco.end - stco.start < 8 + (long)count * 4 || fseek(file, stco.start, SEEK_SET) != 0 ||
        !readMovieU32(file, &version) || version != 0 || !readMovieU32(file, &value) || value != count) return false;
    for (u32 index = 0; index < count; index++) {
        u32 offset;
        if (!readMovieU32(file, &offset) || offset < (u32)mdat.start ||
            offset > (u32)mdat.end || galleryVideo.sizes[index] > (u32)mdat.end - offset) return false;
        galleryVideo.offsets[index] = offset;
    }
    if (stts.end - stts.start < 8 || fseek(file, stts.start, SEEK_SET) != 0 ||
        !readMovieU32(file, &version) || version != 0 || !readMovieU32(file, &value) ||
        !value || value > count || stts.end - stts.start - 8 < (long)value * 8) return false;
    u32 frame = 0;
    for (u32 run = 0; run < value; run++) {
        u32 runLength, duration;
        if (!readMovieU32(file, &runLength) || !readMovieU32(file, &duration) ||
            !runLength || runLength > count - frame || !duration) return false;
        u64 milliseconds = (u64)duration * 1000 / timeScale;
        if (milliseconds > 0xffffffffULL) return false;
        for (u32 index = 0; index < runLength; index++)
            galleryVideo.durations[frame++] = milliseconds ? (u32)milliseconds : 1;
    }
    if (frame != count) return false;
    galleryVideo.audioInvalid = !readGalleryAudioTrack(file, moov, trak, mdat);
    galleryVideo.file = file;
    galleryVideo.frameIndex = 0;
    galleryVideo.playing = false;
    galleryVideo.finished = false;
    galleryVideo.elapsedMs = 0;
    return true;
}

static bool loadGalleryImage(void)
{
    closeGalleryVideo();
    galleryImageLoaded = false;
    if (galleryIndex >= galleryCount) return false;
    FILE *file = fopen(galleryFiles[galleryIndex], "rb");
    if (!file) return false;

    const char *extension = strrchr(galleryFiles[galleryIndex], '.');
    if (isVideoFile(galleryFiles[galleryIndex])) {
        if (!openGalleryVideo(file)) {
            fclose(file);
            closeGalleryVideo();
            return false;
        }
        if (!decodeGalleryVideoFrame(0)) {
            closeGalleryVideo();
            return false;
        }
        return true;
    }
    if (extension && strcmp(extension, ".jpg") == 0) {
        bool loaded = decodeGalleryJpeg(file, NULL, 0);
        fclose(file);
        galleryImageLoaded = loaded;
        return loaded;
    }

    u8 header[54];
    bool valid = fread(header, 1, sizeof(header), file) == sizeof(header)
        && header[0] == 'B' && header[1] == 'M'
        && readLe32(header + 18) == FRAME_WIDTH
        && readLe32(header + 22) == FRAME_HEIGHT
        && readLe16(header + 28) == 24
        && readLe32(header + 30) == 0;
    if (!valid || fseek(file, (long)readLe32(header + 10), SEEK_SET) != 0) {
        fclose(file);
        return false;
    }

    u8 row[FRAME_WIDTH * 3];
    for (int sourceY = 0; sourceY < FRAME_HEIGHT; sourceY++) {
        if (fread(row, 1, sizeof(row), file) != sizeof(row)) {
            fclose(file);
            return false;
        }
        int y = FRAME_HEIGHT - 1 - sourceY;
        for (int x = 0; x < FRAME_WIDTH; x++) {
            u8 blue = row[x * 3];
            u8 green = row[x * 3 + 1];
            u8 red = row[x * 3 + 2];
            galleryFrame[y * FRAME_WIDTH + x] =
                (u16)(((blue >> 3) << 11) | ((green >> 2) << 5) | (red >> 3));
        }
    }
    fclose(file);
    galleryImageLoaded = true;
    return true;
}

static void selectGalleryImage(size_t index)
{
    if (index >= galleryCount) return;
    gallerySelected = false;
    galleryIndex = index;
    size_t pageStart = index / GALLERY_PAGE_SIZE * GALLERY_PAGE_SIZE;
    if (pageStart != galleryPageStart) {
        galleryPageStart = pageStart;
        memset(galleryThumbnailValid, 0, sizeof(galleryThumbnailValid));
        for (size_t slot = 0; slot < GALLERY_PAGE_SIZE && pageStart + slot < galleryCount; slot++) {
            galleryIndex = pageStart + slot;
            if (!loadGalleryImage()) continue;
            for (int y = 0; y < GALLERY_THUMB_SIZE; y++) {
                int sourceY = y * FRAME_HEIGHT / GALLERY_THUMB_SIZE;
                for (int x = 0; x < GALLERY_THUMB_SIZE; x++) {
                    int sourceX = (FRAME_WIDTH - FRAME_HEIGHT) / 2 + x * FRAME_HEIGHT / GALLERY_THUMB_SIZE;
                    galleryThumbnails[slot][y * GALLERY_THUMB_SIZE + x] =
                        galleryFrame[sourceY * FRAME_WIDTH + sourceX];
                }
            }
            galleryThumbnailValid[slot] = true;
        }
        galleryIndex = index;
    }
    loadGalleryImage();
    if (!isVideoFile(galleryFiles[galleryIndex]))
        snprintf(lastPhotoPath, sizeof(lastPhotoPath), "%s", galleryFiles[galleryIndex]);
}

static void updateGalleryVideo(void)
{
    if (!galleryVideo.file || !galleryVideo.playing) return;
    u64 now = svcGetSystemTick();
    u64 elapsed = (u64)galleryVideo.elapsedMs + movieElapsedMs(galleryVideo.frameTick, now);
    u32 index = galleryVideo.frameIndex;
    while (elapsed >= galleryVideo.durations[index]) {
        elapsed -= galleryVideo.durations[index];
        if (index + 1 >= galleryVideo.frameCount) {
            galleryVideo.playing = false;
            galleryVideo.finished = true;
            if (galleryVideo.audioReady) ndspChnSetPaused(0, true);
            elapsed = 0;
            break;
        }
        index++;
    }
    if (index != galleryVideo.frameIndex && !decodeGalleryVideoFrame(index)) {
        galleryVideo.playing = false;
        if (galleryVideo.audioReady) ndspChnSetPaused(0, true);
        photoStatus = "VIDEO READ FAILED";
        return;
    }
    galleryVideo.elapsedMs = (u32)elapsed;
    galleryVideo.frameTick = now;
}

static void toggleGalleryVideo(void)
{
    if (!galleryVideo.file) {
        photoStatus = galleryIndex < galleryCount && isVideoFile(galleryFiles[galleryIndex]) ?
                      "VIDEO UNSUPPORTED" : "SELECT A VIDEO";
        return;
    }
    if (galleryVideo.playing) {
        updateGalleryVideo();
        galleryVideo.playing = false;
        if (galleryVideo.audioReady) ndspChnSetPaused(0, true);
    } else {
        if (galleryVideo.finished) {
            if (!decodeGalleryVideoFrame(0)) {
                photoStatus = "VIDEO READ FAILED";
                return;
            }
            galleryVideo.finished = false;
            galleryVideo.elapsedMs = 0;
            if (!startGalleryAudio()) photoStatus = "AUDIO UNAVAILABLE";
        } else if (!galleryVideo.audioReady && galleryVideo.frameIndex == 0 && !galleryVideo.elapsedMs) {
            if (!startGalleryAudio()) photoStatus = "AUDIO UNAVAILABLE";
        } else if (galleryVideo.audioReady) {
            ndspChnSetPaused(0, false);
        }
        gallerySelected = true;
        galleryVideo.frameTick = svcGetSystemTick();
        galleryVideo.playing = true;
    }
}

static void requestGalleryDelete(int mode)
{
    if (photoSave.active) {
        photoStatus = "PHOTO BUSY";
    } else if (!galleryCount) {
        photoStatus = "GALLERY EMPTY";
    } else {
        if (galleryVideo.playing) {
            updateGalleryVideo();
            galleryVideo.playing = false;
            if (galleryVideo.audioReady) ndspChnSetPaused(0, true);
        }
        galleryDeleteMode = mode;
    }
}

static void deleteGalleryDirectory(const char *path, unsigned *deleted, unsigned *failed)
{
    bool removed;
    do {
        removed = false;
        DIR *directory = opendir(path);
        if (!directory) {
            if (errno != ENOENT) (*failed)++;
            return;
        }
        struct dirent *entry;
        while ((entry = readdir(directory)) != NULL) {
            if (!isGalleryMedia(entry->d_name)) continue;
            char fullPath[96];
            int length = snprintf(fullPath, sizeof(fullPath), "%s/%s", path, entry->d_name);
            if (length < 0 || (size_t)length >= sizeof(fullPath)) {
                (*failed)++;
                continue;
            }
            struct stat info;
            if (stat(fullPath, &info) != 0) {
                (*failed)++;
                continue;
            }
            if (!S_ISREG(info.st_mode)) continue;
            if (remove(fullPath) == 0) {
                (*deleted)++;
                removed = true;
            } else {
                (*failed)++;
            }
        }
        closedir(directory);
    } while (removed);
}

static void confirmGalleryDelete(void)
{
    int mode = galleryDeleteMode;
    galleryDeleteMode = 0;
    if (photoSave.active || !galleryCount || !mode) return;
    size_t previousIndex = galleryIndex;
    unsigned deleted = 0, failed = 0;
    closeGalleryVideo();
    if (mode == 1) {
        char path[sizeof(lastPhotoPath)];
        snprintf(path, sizeof(path), "%s", galleryFiles[galleryIndex]);
        if (remove(path) == 0) {
            deleted = 1;
            if (strcmp(path, lastPhotoPath) == 0) lastPhotoPath[0] = '\0';
        } else failed = 1;
    } else {
        deleteGalleryDirectory(PHOTO_DIRECTORY, &deleted, &failed);
        deleteGalleryDirectory(LEGACY_DIRECTORY, &deleted, &failed);
        if (lastPhotoPath[0]) {
            struct stat info;
            if (stat(lastPhotoPath, &info) != 0 && errno == ENOENT) lastPhotoPath[0] = '\0';
        }
    }
    scanGallery();
    gallerySelected = false;
    galleryImageLoaded = false;
    if (galleryCount) {
        if (previousIndex >= galleryCount) previousIndex = galleryCount - 1;
        selectGalleryImage(previousIndex);
    } else {
        galleryIndex = 0;
        galleryPageStart = 0;
        memset(galleryThumbnailValid, 0, sizeof(galleryThumbnailValid));
    }
    photoStatus = failed ? "DELETE FAILED" : deleted ? "MEDIA DELETED" : "NOTHING DELETED";
}

static void renameLastPhoto(void)
{
    if (photoSave.active) {
        photoStatus = "PHOTO BUSY";
        return;
    }
    if (!lastPhotoPath[0]) {
        photoStatus = "NO PHOTO TO RENAME";
        return;
    }
    const char *slash = strrchr(lastPhotoPath, '/');
    const char *extension = strrchr(lastPhotoPath, '.');
    if (!slash || !extension || extension < slash) {
        photoStatus = "RENAME FAILED";
        return;
    }
    char initial[64];
    size_t initialLength = (size_t)(extension - slash - 1);
    if (initialLength >= sizeof(initial)) initialLength = sizeof(initial) - 1;
    memcpy(initial, slash + 1, initialLength);
    initial[initialLength] = '\0';

    SwkbdState keyboard;
    char name[64];
    swkbdInit(&keyboard, SWKBD_TYPE_WESTERN, 2, 32);
    swkbdSetHintText(&keyboard, "Photo name");
    swkbdSetInitialText(&keyboard, initial);
    swkbdSetValidation(&keyboard, SWKBD_NOTEMPTY_NOTBLANK, 0, 0);
    if (swkbdInputText(&keyboard, name, sizeof(name)) != SWKBD_BUTTON_CONFIRM) return;

    size_t length = strlen(name);
    if (!length || name[0] == ' ' || name[length - 1] == ' ') {
        photoStatus = "INVALID NAME";
        return;
    }
    for (size_t index = 0; index < length; index++) {
        char character = name[index];
        if (!((character >= 'A' && character <= 'Z') ||
              (character >= 'a' && character <= 'z') ||
              (character >= '0' && character <= '9') ||
              character == ' ' || character == '_' || character == '-')) {
            photoStatus = "INVALID NAME";
            return;
        }
    }
    char path[sizeof(lastPhotoPath)];
    int pathLength = snprintf(path, sizeof(path), "%.*s/%s%s", (int)(slash - lastPhotoPath),
                              lastPhotoPath, name, extension);
    if (pathLength < 0 || (size_t)pathLength >= sizeof(path)) {
        photoStatus = "NAME TOO LONG";
        return;
    }
    if (strcmp(path, lastPhotoPath) == 0) return;
    struct stat info;
    if (stat(path, &info) == 0) {
        photoStatus = "NAME EXISTS";
        return;
    }
    if (errno != ENOENT || strncmp(lastPhotoPath, "sdmc:/", 6) != 0 ||
        strncmp(path, "sdmc:/", 6) != 0) {
        photoStatus = "RENAME FAILED";
        return;
    }
    FS_Archive sdmcArchive;
    if (R_FAILED(FSUSER_OpenArchive(&sdmcArchive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, "")))) {
        photoStatus = "RENAME FAILED";
        return;
    }
    Result renameResult = FSUSER_RenameFile(sdmcArchive, fsMakePath(PATH_ASCII, lastPhotoPath + 5),
                                            sdmcArchive, fsMakePath(PATH_ASCII, path + 5));
    FSUSER_CloseArchive(sdmcArchive);
    if (R_FAILED(renameResult)) {
        photoStatus = "RENAME FAILED";
        return;
    }
    snprintf(lastPhotoPath, sizeof(lastPhotoPath), "%s", path);
    scanGallery();
    photoStatus = "PHOTO RENAMED";
}

static void renderGallery(u8 *framebuffer)
{
    if (!galleryCount || !galleryImageLoaded) {
        memset(framebuffer, 0, FRAME_PIXELS * 3);
        return;
    }

    for (int y = 0; y < FRAME_HEIGHT; y++) {
        for (int x = 0; x < FRAME_WIDTH; x++) {
            u16 pixel = galleryFrame[y * FRAME_WIDTH + x];
            size_t offset = ((size_t)x * FRAME_HEIGHT + (FRAME_HEIGHT - 1 - y)) * 3;
            framebuffer[offset] = (u8)(((pixel >> 11) & 0x1f) * 255 / 31);
            framebuffer[offset + 1] = (u8)(((pixel >> 5) & 0x3f) * 255 / 63);
            framebuffer[offset + 2] = (u8)((pixel & 0x1f) * 255 / 31);
        }
    }
}

static void drawLine(int x0, int y0, int x1, int y1)
{
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;

    for (;;) {
        for (int offsetY = -4; offsetY <= 4; offsetY++) {
            for (int offsetX = -4; offsetX <= 4; offsetX++) {
                int distanceSquared = offsetX * offsetX + offsetY * offsetY;
                if (distanceSquared > 16) continue;
                u8 coverage = distanceSquared <= 9 ? 255 :
                              distanceSquared <= 10 ? 192 :
                              distanceSquared <= 13 ? 112 : 32;
                int x = x0 + offsetX;
                int y = y0 + offsetY;
                if (x >= 0 && x < FRAME_WIDTH && y >= 0 && y < FRAME_HEIGHT) {
                    size_t index = (size_t)y * FRAME_WIDTH + x;
                    if (eraserEnabled) {
                        drawAlpha[index] = (u8)(drawAlpha[index] * (255 - coverage) / 255);
                    } else {
                        u16 previous = drawOverlay[index];
                        int previousWeight = drawAlpha[index] * (255 - coverage) / 255;
                        int totalWeight = coverage + previousWeight;
                        int red = ((drawColor565 & 0x1f) * coverage +
                                   (previous & 0x1f) * previousWeight + totalWeight / 2) / totalWeight;
                        int green = (((drawColor565 >> 5) & 0x3f) * coverage +
                                     ((previous >> 5) & 0x3f) * previousWeight + totalWeight / 2) / totalWeight;
                        int blue = (((drawColor565 >> 11) & 0x1f) * coverage +
                                    ((previous >> 11) & 0x1f) * previousWeight + totalWeight / 2) / totalWeight;
                        drawOverlay[index] = (u16)((blue << 11) | (green << 5) | red);
                        drawAlpha[index] = (u8)totalWeight;
                    }
                }
            }
        }
        if (x0 == x1 && y0 == y1) break;
        int twiceError = 2 * error;
        if (twiceError >= dy) { error += dy; x0 += sx; }
        if (twiceError <= dx) { error += dx; y0 += sy; }
    }
}

static bool startCamera(void)
{
    Result result = camInit();
    if (R_FAILED(result)) {
        printf("Camera service error: 0x%08lX\n", (unsigned long)result);
        return false;
    }
    cameraInitialized = true;

    result = CAMU_SetSize(cameraSelect, SIZE_CTR_TOP_LCD, CONTEXT_A);
    if (R_FAILED(result)) goto camera_error;
    result = CAMU_SetOutputFormat(cameraSelect, OUTPUT_RGB_565, CONTEXT_A);
    if (R_FAILED(result)) goto camera_error;
    result = CAMU_SetFrameRate(cameraSelect, FRAME_RATE_30);
    if (R_FAILED(result)) goto camera_error;
    result = CAMU_SetNoiseFilter(cameraSelect, true);
    if (R_FAILED(result)) goto camera_error;
    result = CAMU_SetAutoExposure(cameraSelect, true);
    if (R_FAILED(result)) goto camera_error;
    result = CAMU_SetAutoWhiteBalance(cameraSelect, true);
    if (R_FAILED(result)) goto camera_error;
    result = CAMU_SetTrimming(PORT_CAM1, false);
    if (R_FAILED(result)) goto camera_error;
    result = CAMU_GetMaxBytes(&cameraTransferBytes, FRAME_WIDTH, FRAME_HEIGHT);
    if (R_FAILED(result)) goto camera_error;
    result = CAMU_SetTransferBytes(PORT_CAM1, cameraTransferBytes, FRAME_WIDTH, FRAME_HEIGHT);
    if (R_FAILED(result)) goto camera_error;
    result = CAMU_Activate(cameraSelect);
    if (R_FAILED(result)) goto camera_error;
    result = CAMU_ClearBuffer(PORT_CAM1);
    if (R_FAILED(result)) goto camera_error;

    return true;

camera_error:
    printf("Camera setup error: 0x%08lX\n", (unsigned long)result);
    CAMU_Activate(SELECT_NONE);
    return false;
}

static bool receiveFrame(void)
{
    Handle event = 0;
    Result result = CAMU_ClearBuffer(PORT_CAM1);
    if (R_FAILED(result)) return false;
    result = CAMU_SetReceiving(&event, cameraFrame, PORT_CAM1, FRAME_BYTES, (s16)cameraTransferBytes);
    if (R_FAILED(result)) return false;

    result = CAMU_StartCapture(PORT_CAM1);
    if (R_SUCCEEDED(result)) {
        cameraCapturing = true;
        result = svcWaitSynchronization(event, 1000000000ULL);
        CAMU_StopCapture(PORT_CAM1);
        cameraCapturing = false;
    }
    svcCloseHandle(event);
    return R_SUCCEEDED(result);
}

static void stopCamera(void)
{
    if (cameraCapturing) CAMU_StopCapture(PORT_CAM1);
    if (cameraInitialized) {
        CAMU_Activate(SELECT_NONE);
        camExit();
    }
    cameraCapturing = false;
    cameraInitialized = false;
}

static void switchCamera(void)
{
    faceTrackerInvalidate();
    backdropMask.calibrated = false;
    if (movie.active) finishMovie();
    stopCamera();
    cameraSelect = cameraSelect == SELECT_OUT1 ? SELECT_IN1 : SELECT_OUT1;
    cameraReady = cameraFrame && startCamera();
}

static void setVideoMode(bool enabled)
{
    if (!enabled && movie.active) finishMovie();
    videoMode = enabled;
}

static void pushDrawingHistory(DrawingSnapshot *history, size_t *count)
{
    if (*count == DRAW_HISTORY_LIMIT) {
        memmove(&history[0], &history[1], (DRAW_HISTORY_LIMIT - 1) * sizeof(history[0]));
        (*count)--;
    }
    memcpy(history[*count].colors, drawOverlay, sizeof(drawOverlay));
    memcpy(history[*count].alpha, drawAlpha, sizeof(drawAlpha));
    (*count)++;
}

static void beginDrawingEdit(void)
{
    pushDrawingHistory(undoHistory, &undoCount);
    redoCount = 0;
}

static void undoDrawing(void)
{
    if (!undoCount) return;
    pushDrawingHistory(redoHistory, &redoCount);
    undoCount--;
    memcpy(drawOverlay, undoHistory[undoCount].colors, sizeof(drawOverlay));
    memcpy(drawAlpha, undoHistory[undoCount].alpha, sizeof(drawAlpha));
}

static void redoDrawing(void)
{
    if (!redoCount) return;
    pushDrawingHistory(undoHistory, &undoCount);
    redoCount--;
    memcpy(drawOverlay, redoHistory[redoCount].colors, sizeof(drawOverlay));
    memcpy(drawAlpha, redoHistory[redoCount].alpha, sizeof(drawAlpha));
}

static bool canvasGlyphPixel(char character, int row, int column)
{
    static const u8 lowercase[26][7] = {
        {0,0,14,1,15,17,15}, {16,16,30,17,17,17,30}, {0,0,15,16,16,16,15},
        {1,1,15,17,17,17,15}, {0,0,14,17,31,16,14}, {6,9,8,28,8,8,8},
        {0,0,15,17,15,1,14}, {16,16,30,17,17,17,17}, {4,0,12,4,4,4,14},
        {2,0,6,2,2,18,12}, {16,16,18,20,24,20,18}, {12,4,4,4,4,4,14},
        {0,0,26,21,21,21,21}, {0,0,30,17,17,17,17}, {0,0,14,17,17,17,14},
        {0,0,30,17,30,16,16}, {0,0,15,17,15,1,1}, {0,0,22,25,16,16,16},
        {0,0,15,16,14,1,30}, {8,8,28,8,8,9,6}, {0,0,17,17,17,19,13},
        {0,0,17,17,17,10,4}, {0,0,17,17,21,21,10}, {0,0,17,10,4,10,17},
        {0,0,17,17,15,1,14}, {0,0,31,2,4,8,31}
    };
    if (character >= 'a' && character <= 'z')
        return (lowercase[character - 'a'][row] & (1 << (4 - column))) != 0;
    if (character >= 'A' && character <= 'Z')
        return (smallFont[character - 'A'][row] & (1 << (4 - column))) != 0;
    if (character >= '0' && character <= '9')
        return (digitFont[character - '0'][row] & (1 << (4 - column))) != 0;
    if (character == '.') return row == 6 && column == 2;
    if (character == ',') return (row == 5 && column == 2) || (row == 6 && column == 1);
    if (character == ':') return column == 2 && (row == 2 || row == 5);
    if (character == '!') return column == 2 && (row < 5 || row == 6);
    if (character == '-') return row == 3 && column >= 1 && column <= 3;
    if (character == '_') return row == 6;
    if (character == '+') return (row == 3 && column >= 1 && column <= 3) ||
                                 (column == 2 && row >= 2 && row <= 4);
    if (character == '/') return column == 4 - row * 4 / 6;
    if (character == '?') {
        static const u8 question[7] = {14, 17, 1, 2, 4, 0, 4};
        return (question[row] & (1 << (4 - column))) != 0;
    }
    return false;
}

static void beginTextPlacement(void)
{
    if (movie.active) finishMovie();
    SwkbdState keyboard;
    char text[sizeof(canvasText)];
    swkbdInit(&keyboard, SWKBD_TYPE_WESTERN, 2, 32);
    swkbdSetHintText(&keyboard, "Canvas text");
    swkbdSetValidation(&keyboard, SWKBD_NOTEMPTY_NOTBLANK, 0, 0);
    if (swkbdInputText(&keyboard, text, sizeof(text)) != SWKBD_BUTTON_CONFIRM) return;
    for (size_t index = 0; text[index]; index++) {
        char character = text[index];
        if (!((character >= 'A' && character <= 'Z') ||
              (character >= 'a' && character <= 'z') ||
              (character >= '0' && character <= '9') ||
              strchr(" .,!?:-_+/", character))) {
            photoStatus = "UNSUPPORTED TEXT";
            return;
        }
    }
    snprintf(canvasText, sizeof(canvasText), "%s", text);
    drawEnabled = true;
    eraserEnabled = false;
    textPlacementPending = true;
    photoStatus = "PLACE TEXT";
}

static void placeCanvasText(int x, int y)
{
    int width = (int)strlen(canvasText) * 12;
    if (x + width > FRAME_WIDTH) x = FRAME_WIDTH - width;
    if (x < 0) x = 0;
    if (y + 14 > FRAME_HEIGHT) y = FRAME_HEIGHT - 14;
    if (y < 0) y = 0;
    beginDrawingEdit();
    for (size_t index = 0; canvasText[index]; index++) {
        for (int row = 0; row < 7; row++) {
            for (int column = 0; column < 5; column++) {
                if (!canvasGlyphPixel(canvasText[index], row, column)) continue;
                for (int offsetY = 0; offsetY < 2; offsetY++) {
                    for (int offsetX = 0; offsetX < 2; offsetX++) {
                        int targetX = x + (int)index * 12 + column * 2 + offsetX;
                        int targetY = y + row * 2 + offsetY;
                        size_t position = (size_t)targetY * FRAME_WIDTH + targetX;
                        drawOverlay[position] = drawColor565;
                        drawAlpha[position] = 255;
                    }
                }
            }
        }
    }
    textPlacementPending = false;
    photoStatus = "TEXT ADDED";
}

static void resetEdits(void)
{
    currentFilter = FILTER_NORMAL;
    currentFrame = FRAME_NONE;
    brightness = 1.0f;
    contrast = 1.0f;
    saturation = 1.0f;
    gameboyDither = 0.5f;
    gameboyPaletteIndex = 0;
    zoom = 1.0f;
    mirrorPreview = false;
    beginDrawingEdit();
    memset(drawOverlay, 0, sizeof(drawOverlay));
    memset(drawAlpha, 0, sizeof(drawAlpha));
    previousTouchValid = false;
    textPlacementPending = false;
    eyedropperMode = false;
    brushPaletteOpen = false;
}

static void previewToFrame(int x, int y, int *frameX, int *frameY)
{
    if (x < PREVIEW_LEFT) x = PREVIEW_LEFT;
    if (x >= PREVIEW_LEFT + PREVIEW_WIDTH) x = PREVIEW_LEFT + PREVIEW_WIDTH - 1;
    if (y < PREVIEW_TOP) y = PREVIEW_TOP;
    if (y >= PREVIEW_TOP + PREVIEW_HEIGHT) y = PREVIEW_TOP + PREVIEW_HEIGHT - 1;
    int cropHeight = FRAME_WIDTH * PREVIEW_HEIGHT / PREVIEW_WIDTH;
    *frameX = (x - PREVIEW_LEFT) * FRAME_WIDTH / PREVIEW_WIDTH;
    *frameY = (FRAME_HEIGHT - cropHeight) / 2 +
              (y - PREVIEW_TOP) * cropHeight / PREVIEW_HEIGHT;
}

static void setSliderFromTouch(int slider, int x)
{
    if (slider == 3 && currentFilter == FILTER_GREEN_SCREEN) {
        if (x < GREEN_SLIDER_LEFT) x = GREEN_SLIDER_LEFT;
        if (x > GREEN_SLIDER_RIGHT) x = GREEN_SLIDER_RIGHT;
        greenScreenThreshold = 40 - (x - GREEN_SLIDER_LEFT) * 40 /
                               (GREEN_SLIDER_RIGHT - GREEN_SLIDER_LEFT);
        return;
    }
    if ((slider == 4 || slider == 5) && currentFilter == FILTER_GREEN_SCREEN) {
        if (x < GREEN_SLIDER_LEFT) x = GREEN_SLIDER_LEFT;
        if (x > GREEN_SLIDER_RIGHT) x = GREEN_SLIDER_RIGHT;
        float position = (float)(x - GREEN_SLIDER_LEFT) /
                         (GREEN_SLIDER_RIGHT - GREEN_SLIDER_LEFT);
        float value = 0.5f + position * 1.5f;
        if (slider == 4) brightness = value;
        else contrast = value;
        return;
    }
    int left = currentFilter == FILTER_GAMEBOY ? GAMEBOY_SLIDER_LEFT : SLIDER_TRACK_LEFT;
    int right = currentFilter == FILTER_GAMEBOY ? GAMEBOY_SLIDER_RIGHT : SLIDER_TRACK_RIGHT;
    if (x < left) x = left;
    if (x > right) x = right;
    float position = (float)(x - left) / (right - left);
    float value = 0.5f + position * 1.5f;
    if (slider == 0) brightness = value;
    else if (slider == 1) contrast = value;
    else if (currentFilter == FILTER_GAMEBOY) gameboyDither = position;
    else saturation = value;
}

static void openGallery(void)
{
    faceTrackerInvalidate();
    if (movie.active) finishMovie();
    menuOpen = false;
    editingMenuOpen = false;
    timerMenuOpen = false;
    textPlacementPending = false;
    if (captureTimerDeadline) photoStatus = "";
    captureTimerDeadline = 0;
    galleryOpen = true;
    gallerySelected = false;
    galleryDeleteMode = 0;
    scanGallery();
    galleryIndex = galleryCount ? galleryCount - 1 : 0;
    if (galleryCount) selectGalleryImage(galleryIndex);
    stopCamera();
    cameraReady = false;
}

static void updatePhotoSave(void)
{
    if (!photoSave.active) return;
    if (!photoSave.done && R_SUCCEEDED(threadJoin(photoSave.worker, 0))) {
        threadFree(photoSave.worker);
        photoSave.worker = NULL;
        photoSave.done = true;
    }
    if (!photoSave.done || movieElapsedMs(photoSave.startTick, svcGetSystemTick()) < 500) return;
    photoSave.active = false;
    photoStatus = photoSave.saved ? "PHOTO SAVED" : "SD SAVE FAILED";
    if (photoSave.saved) {
        char selected[sizeof(lastPhotoPath)] = "";
        if (galleryOpen && galleryIndex < galleryCount)
            snprintf(selected, sizeof(selected), "%s", galleryFiles[galleryIndex]);
        scanGallery();
        if (galleryOpen && galleryCount) {
            size_t index = galleryCount - 1;
            for (size_t candidate = 0; candidate < galleryCount; candidate++) {
                if (strcmp(galleryFiles[candidate], selected) == 0) {
                    index = candidate;
                    break;
                }
            }
            selectGalleryImage(index);
        } else {
            snprintf(lastPhotoPath, sizeof(lastPhotoPath), "%s", photoSave.path);
        }
    }
}

static void applyEditingMenuItem(void)
{
    if (editingMenuItem < FRAME_STYLE_COUNT) {
        currentFrame = (FrameStyle)editingMenuItem;
        editingMenuOpen = false;
    } else if (editingMenuItem == FRAME_STYLE_COUNT) {
        mirrorPreview = !mirrorPreview;
    } else {
        timestampEnabled = !timestampEnabled;
    }
}

static void applyTimerMenuItem(void)
{
    static const unsigned choices[TIMER_MENU_ITEM_COUNT] = {0, 4, 6, 8};
    captureTimerSeconds = choices[timerMenuItem];
    captureTimerDeadline = 0;
    timerMenuOpen = false;
    photoStatus = "";
}

int main(void)
{
    gfxInitDefault();
    gfxSetDoubleBuffering(GFX_TOP, true);
    gfxSetDoubleBuffering(GFX_BOTTOM, true);

    cameraFrame = (u16 *)linearAlloc(FRAME_BYTES);
    cameraReady = cameraFrame && startCamera();
    if (!ensureGalleryDirectory()) photoStatus = "SD FOLDER ERROR";
    scanGallery();
    if (galleryCount) snprintf(lastPhotoPath, sizeof(lastPhotoPath), "%s", galleryFiles[galleryCount - 1]);
    drawBottomUi();
    int activeSlider = -1;
    bool pendingPhoto = false;
    bool pendingVideoStart = false;
    bool pendingBackdropCapture = false;
    bool strokeActive = false;
    u64 previousInputTick = svcGetSystemTick();

    while (aptMainLoop()) {
        hidScanInput();
        if (movie.active) collectMovieAudio();
        updatePhotoSave();
        u32 keys = hidKeysDown();
        u64 inputTick = svcGetSystemTick();
        float elapsedSeconds = (float)(inputTick - previousInputTick) / (float)SYSCLOCK_ARM11;
        previousInputTick = inputTick;
        if (elapsedSeconds > 0.25f) elapsedSeconds = 0.25f;
        if (keys & KEY_START) {
            if (movie.active) finishMovie();
            break;
        }

        bool updateUi = false;
        bool captureTouch = false;
        bool capturePhotoRequested = (keys & (KEY_R | KEY_ZR)) != 0;
        bool recordRequested = (keys & (KEY_L | KEY_ZL)) != 0;
        bool galleryHandledTouch = false;
        int menuAction = -1;
        if (galleryOpen) {
            if (galleryDeleteMode) {
                if (keys & KEY_B) galleryDeleteMode = 0;
                else if (keys & KEY_A) confirmGalleryDelete();
                else if (keys & KEY_TOUCH) {
                    touchPosition touch;
                    hidTouchRead(&touch);
                    galleryHandledTouch = true;
                    if (touch.py >= 135 && touch.py < 154) {
                        if (touch.px >= 63 && touch.px < 143) confirmGalleryDelete();
                        else if (touch.px >= 177 && touch.px < 257) galleryDeleteMode = 0;
                    }
                }
                updateUi = true;
            } else if (keys & KEY_X) {
                closeGalleryVideo();
                galleryOpen = false;
                cameraReady = startCamera();
                updateUi = true;
            } else if (keys & KEY_B) {
                if (gallerySelected) {
                    gallerySelected = false;
                    if (galleryVideo.playing) updateGalleryVideo();
                    galleryVideo.playing = false;
                    if (galleryVideo.audioReady) ndspChnSetPaused(0, true);
                } else {
                    closeGalleryVideo();
                    galleryOpen = false;
                    cameraReady = startCamera();
                }
                updateUi = true;
            } else if ((keys & KEY_A) && galleryCount) {
                gallerySelected = true;
                updateUi = true;
            } else if (keys & KEY_Y) {
                toggleGalleryVideo();
                updateUi = true;
            } else if (galleryCount && (keys & KEY_DLEFT)) {
                selectGalleryImage((galleryIndex + galleryCount - 1) % galleryCount);
                updateUi = true;
            } else if (galleryCount && (keys & KEY_DRIGHT)) {
                selectGalleryImage((galleryIndex + 1) % galleryCount);
                updateUi = true;
            } else if (galleryCount && (keys & KEY_DUP)) {
                selectGalleryImage(galleryIndex >= GALLERY_COLUMNS ? galleryIndex - GALLERY_COLUMNS : 0);
                updateUi = true;
            } else if (galleryCount && (keys & KEY_DDOWN)) {
                size_t next = galleryIndex + GALLERY_COLUMNS;
                selectGalleryImage(next < galleryCount ? next : galleryCount - 1);
                updateUi = true;
            }
            if (galleryOpen && !galleryDeleteMode && !galleryHandledTouch && (keys & KEY_TOUCH)) {
                touchPosition touch;
                hidTouchRead(&touch);
                int x = touch.px - GALLERY_GRID_LEFT;
                int y = touch.py - GALLERY_GRID_TOP;
                if (touch.py >= 216 && touch.py < 236 && touch.px >= 7 && touch.px < 146) {
                    requestGalleryDelete(1);
                } else if (touch.py >= 216 && touch.py < 236 && touch.px >= 165 && touch.px < 314) {
                    requestGalleryDelete(2);
                } else if (galleryCount && touch.py >= 192 && touch.py < 210 &&
                    touch.px >= 10 && touch.px < 62 && galleryPageStart > 0) {
                    selectGalleryImage(galleryPageStart - GALLERY_PAGE_SIZE);
                } else if (galleryCount && touch.py >= 192 && touch.py < 210 &&
                           touch.px >= 258 && touch.px < 310 &&
                           galleryPageStart + GALLERY_PAGE_SIZE < galleryCount) {
                    selectGalleryImage(galleryPageStart + GALLERY_PAGE_SIZE);
                } else if (x >= 0 && y >= 0 && x / GALLERY_CELL_WIDTH < GALLERY_COLUMNS &&
                    y / GALLERY_CELL_HEIGHT < GALLERY_ROWS &&
                    x % GALLERY_CELL_WIDTH < GALLERY_THUMB_SIZE &&
                    y % GALLERY_CELL_HEIGHT < GALLERY_THUMB_SIZE) {
                    size_t index = galleryPageStart + (y / GALLERY_CELL_HEIGHT) * GALLERY_COLUMNS +
                                   x / GALLERY_CELL_WIDTH;
                    selectGalleryImage(index);
                    if (index < galleryCount) gallerySelected = true;
                }
                updateUi = true;
            }
        } else if (timerMenuOpen) {
            if (keys & (KEY_B | KEY_SELECT)) timerMenuOpen = false;
            else if (keys & KEY_DUP) timerMenuItem = (timerMenuItem + TIMER_MENU_ITEM_COUNT - 1) % TIMER_MENU_ITEM_COUNT;
            else if (keys & KEY_DDOWN) timerMenuItem = (timerMenuItem + 1) % TIMER_MENU_ITEM_COUNT;
            else if (keys & KEY_A) applyTimerMenuItem();
            else if (keys & KEY_Y) { setVideoMode(!videoMode); timerMenuOpen = false; }
            updateUi = true;
        } else if (editingMenuOpen) {
            if (keys & (KEY_B | KEY_SELECT)) {
                editingMenuOpen = false;
            } else if (keys & KEY_DUP) {
                editingMenuItem = (editingMenuItem + EDIT_MENU_ITEM_COUNT - 1) % EDIT_MENU_ITEM_COUNT;
            } else if (keys & KEY_DDOWN) {
                editingMenuItem = (editingMenuItem + 1) % EDIT_MENU_ITEM_COUNT;
            } else if (keys & KEY_A) {
                applyEditingMenuItem();
            } else if (keys & KEY_Y) {
                setVideoMode(!videoMode);
                editingMenuOpen = false;
            }
            updateUi = true;
        } else if (menuOpen) {
            if (keys & KEY_B) {
                menuOpen = false;
                updateUi = true;
            }
            if (keys & KEY_Y) {
                setVideoMode(!videoMode);
                menuOpen = false;
                updateUi = true;
            }
            if (keys & KEY_DLEFT) {
                menuIndex = (menuIndex + 2) % 3;
                menuItem = 0;
                updateUi = true;
            } else if (keys & KEY_DRIGHT) {
                menuIndex = (menuIndex + 1) % 3;
                menuItem = 0;
                updateUi = true;
            }
            if (keys & (KEY_DUP | KEY_DDOWN)) {
                int itemCount = menuIndex == 0 ? 2 : menuIndex == 1 ? 3 : 5;
                menuItem = (menuItem + ((keys & KEY_DUP) ? itemCount - 1 : 1)) % itemCount;
                updateUi = true;
            }
            if (menuOpen && (keys & KEY_A)) menuAction = menuItem;
        } else {
            if (keys & KEY_A) {
                if (videoMode) recordRequested = true;
                else capturePhotoRequested = true;
            }
            if (keys & KEY_Y) {
                setVideoMode(!videoMode);
                updateUi = true;
            }
            if (keys & KEY_X) {
                openGallery();
                updateUi = true;
            }
            if (keys & KEY_DLEFT) {
                currentFilter = (Filter)((currentFilter + FILTER_COUNT - 1) % FILTER_COUNT);
                updateUi = true;
            } else if (keys & KEY_DRIGHT) {
                currentFilter = (Filter)((currentFilter + 1) % FILTER_COUNT);
                updateUi = true;
            }
            if (keys & KEY_DUP) {
                zoom += 0.25f;
                if (zoom > 4.0f) zoom = 4.0f;
                updateUi = true;
            }
            if (keys & KEY_DDOWN) {
                zoom -= 0.25f;
                if (zoom < 1.0f) zoom = 1.0f;
                updateUi = true;
            }
            if (!galleryOpen && !menuOpen) {
                circlePosition circle;
                hidCircleRead(&circle);
                int magnitude = abs(circle.dy);
                if (magnitude > CIRCLE_ZOOM_DEADZONE) {
                    if (magnitude > CIRCLE_ZOOM_MAX) magnitude = CIRCLE_ZOOM_MAX;
                    float strength = (float)(magnitude - CIRCLE_ZOOM_DEADZONE) /
                                     (CIRCLE_ZOOM_MAX - CIRCLE_ZOOM_DEADZONE);
                    zoom += (circle.dy > 0 ? 1.0f : -1.0f) *
                            strength * CIRCLE_ZOOM_PER_SECOND * elapsedSeconds;
                    if (zoom > 4.0f) zoom = 4.0f;
                    if (zoom < 1.0f) zoom = 1.0f;
                    updateUi = true;
                }
            }
            if (keys & KEY_SELECT) {
                editingMenuOpen = true;
                editingMenuItem = currentFrame;
                brushPaletteOpen = false;
                eyedropperMode = false;
                updateUi = true;
            }
            if (keys & KEY_B) {
                if (currentFilter == FILTER_GREEN_SCREEN || currentFilter == FILTER_GAMEBOY)
                    currentFilter = FILTER_NORMAL;
                else switchCamera();
                updateUi = true;
            }
        }

        if (!galleryOpen && (keys & KEY_TOUCH)) {
            touchPosition touch;
            hidTouchRead(&touch);
            int x = touch.px;
            int y = touch.py;
            if (timerMenuOpen) {
                if (x >= TIMER_MENU_LEFT && x < TIMER_MENU_LEFT + TIMER_MENU_WIDTH &&
                    y >= TIMER_MENU_TOP + 1 &&
                    y < TIMER_MENU_TOP + 1 + TIMER_MENU_ITEM_COUNT * TIMER_MENU_ROW_HEIGHT) {
                    timerMenuItem = (y - (TIMER_MENU_TOP + 1)) / TIMER_MENU_ROW_HEIGHT;
                    applyTimerMenuItem();
                } else timerMenuOpen = false;
                updateUi = true;
            } else if (editingMenuOpen) {
                if (x >= EDIT_MENU_LEFT && x < EDIT_MENU_LEFT + EDIT_MENU_WIDTH &&
                    y >= EDIT_MENU_TOP + 1 &&
                    y < EDIT_MENU_TOP + 1 + EDIT_MENU_ITEM_COUNT * EDIT_MENU_ROW_HEIGHT) {
                    editingMenuItem = (y - (EDIT_MENU_TOP + 1)) / EDIT_MENU_ROW_HEIGHT;
                    applyEditingMenuItem();
                } else {
                    editingMenuOpen = false;
                }
                updateUi = true;
            } else if (brushPaletteOpen && textColorPicking) {
                if (x >= 49 && x < 49 + BRUSH_COLOR_COUNT * 17 && y >= 59 && y < 76) {
                    drawColor565 = brushColors[(x - 49) / 17];
                    brushPaletteOpen = false;
                    textColorPicking = false;
                    eyedropperMode = false;
                    beginTextPlacement();
                } else if (x >= 190 && x < 244 && y >= 59 && y < 76) {
                    brushPaletteOpen = false;
                    textColorPicking = false;
                }
                updateUi = true;
            } else if (y >= HEADER_MENU_TOP && y < HEADER_MENU_TOP + HEADER_MENU_HEIGHT && x < 180) {
                int selectedMenu = x < 52 ? 0 : x < 108 ? 1 : 2;
                bool alreadyOpen = menuOpen && menuIndex == selectedMenu;
                menuOpen = !alreadyOpen;
                menuIndex = selectedMenu;
                menuItem = 0;
                brushPaletteOpen = false;
                eyedropperMode = false;
                updateUi = true;
            } else if (y >= HEADER_MENU_TOP && y < HEADER_MENU_TOP + HEADER_MENU_HEIGHT && x >= 255) {
                openGallery();
                updateUi = true;
            } else if (menuOpen) {
                int itemCount = menuIndex == 0 ? 2 : menuIndex == 1 ? 3 : 5;
                if (x >= 4 && x < 154 && y >= MENU_POPUP_TOP && y < MENU_POPUP_TOP + itemCount * 13) {
                    menuItem = (y - MENU_POPUP_TOP) / 13;
                    menuAction = menuItem;
                } else {
                    menuOpen = false;
                }
                updateUi = true;
            } else if (currentFilter == FILTER_GREEN_SCREEN) {
                if (x >= GREEN_LIST_LEFT + 2 && x < GREEN_LIST_LEFT + 113 &&
                    y >= GREEN_LIST_TOP && y < GREEN_LIST_TOP + GREEN_BACKGROUND_PAGE_SIZE * GREEN_LIST_ROW_HEIGHT) {
                    int index = greenScreenPageStart + (y - GREEN_LIST_TOP) / GREEN_LIST_ROW_HEIGHT;
                    if (index < GREEN_BUILTIN_BACKGROUNDS + (int)greenScreenImportedCount)
                        selectGreenScreenBackground(index);
                    updateUi = true;
                } else if (y >= 134 && y < 154 && x >= GREEN_LIST_LEFT + 2 && x < GREEN_LIST_LEFT + 34) {
                    if (greenScreenPageStart >= GREEN_BACKGROUND_PAGE_SIZE)
                        greenScreenPageStart -= GREEN_BACKGROUND_PAGE_SIZE;
                    updateUi = true;
                } else if (y >= 134 && y < 154 && x >= GREEN_LIST_LEFT + 80 && x < GREEN_LIST_LEFT + 112) {
                    if (greenScreenPageStart + GREEN_BACKGROUND_PAGE_SIZE <
                        GREEN_BUILTIN_BACKGROUNDS + (int)greenScreenImportedCount)
                        greenScreenPageStart += GREEN_BACKGROUND_PAGE_SIZE;
                    updateUi = true;
                } else if (x >= GREEN_CAPTURE_LEFT && x < GREEN_CAPTURE_LEFT + GREEN_CAPTURE_WIDTH &&
                           y >= 41 && y < 54) {
                    if (!cameraReady) photoStatus = "NO CAMERA";
                    else if (movie.active || pendingVideoStart) photoStatus = "STOP RECORDING";
                    else if (captureTimerDeadline || pendingPhoto) photoStatus = "PHOTO BUSY";
                    else {
                        pendingBackdropCapture = true;
                        photoStatus = "WAITING BG FRAME";
                    }
                    updateUi = true;
                } else if (x >= GREEN_SLIDER_LEFT - 2 && x <= GREEN_SLIDER_RIGHT + 3 &&
                          y >= GREEN_SLIDER_TOP &&
                          y < GREEN_SLIDER_TOP + 3 * GREEN_SLIDER_ROW_HEIGHT) {
                      activeSlider = 3 + (y - GREEN_SLIDER_TOP) / GREEN_SLIDER_ROW_HEIGHT;
                    setSliderFromTouch(activeSlider, x);
                    updateUi = true;
                } else if (x >= GREEN_CONTROL_LEFT && x < GREEN_CONTROL_LEFT + 5 * GREEN_CONTROL_PITCH &&
                           y >= GREEN_CONTROL_TOP && y < GREEN_CONTROL_TOP + GREEN_CONTROL_HEIGHT &&
                           (x - GREEN_CONTROL_LEFT) % GREEN_CONTROL_PITCH < GREEN_CONTROL_WIDTH) {
                    int control = (x - GREEN_CONTROL_LEFT) / GREEN_CONTROL_PITCH;
                    if (control == 0) {
                        timerMenuOpen = true;
                        timerMenuItem = captureTimerSeconds == 4 ? 1 : captureTimerSeconds == 6 ? 2 :
                                        captureTimerSeconds == 8 ? 3 : 0;
                    } else if (control == 1) switchCamera();
                    else if (control == 2) setVideoMode(!videoMode);
                    else if (control == 3) captureTouch = true;
                    else currentFilter = FILTER_NORMAL;
                    updateUi = true;
                }
            } else if (currentFilter == FILTER_GAMEBOY) {
                if (x >= GAMEBOY_CONTROL_LEFT && x < GAMEBOY_CONTROL_LEFT + GAMEBOY_CONTROL_WIDTH &&
                    y >= GAMEBOY_CONTROL_TOP && y < GAMEBOY_CONTROL_TOP + 5 * GAMEBOY_CONTROL_PITCH &&
                    (y - GAMEBOY_CONTROL_TOP) % GAMEBOY_CONTROL_PITCH < GAMEBOY_CONTROL_HEIGHT) {
                    int control = (y - GAMEBOY_CONTROL_TOP) / GAMEBOY_CONTROL_PITCH;
                    if (control == 0) {
                        timerMenuOpen = true;
                        timerMenuItem = captureTimerSeconds == 4 ? 1 : captureTimerSeconds == 6 ? 2 :
                                        captureTimerSeconds == 8 ? 3 : 0;
                    } else if (control == 1) {
                        switchCamera();
                    } else if (control == 2) {
                        setVideoMode(!videoMode);
                    } else if (control == 3) {
                        captureTouch = true;
                    } else {
                        currentFilter = FILTER_NORMAL;
                    }
                    updateUi = true;
                } else if (y >= GAMEBOY_FRAME_TOP && y < GAMEBOY_FRAME_TOP + 18) {
                    if (x >= 4 && x < 154) gameboyPaletteIndex = (x - 4) / 38;
                    else if (x >= 160 && x < 188) selectGameboyFrame(-1);
                    else if (x >= 290 && x < 316) selectGameboyFrame(1);
                    updateUi = true;
                } else if (x >= GAMEBOY_SLIDER_LEFT - 2 && x <= GAMEBOY_SLIDER_RIGHT + 3 &&
                           y >= GAMEBOY_SLIDER_TOP && y < GAMEBOY_SLIDER_TOP + 3 * GAMEBOY_SLIDER_HEIGHT) {
                    activeSlider = (y - GAMEBOY_SLIDER_TOP) / GAMEBOY_SLIDER_HEIGHT;
                    setSliderFromTouch(activeSlider, x);
                    updateUi = true;
                }
            } else if (brushPaletteOpen && x >= 49 && x < 49 + BRUSH_COLOR_COUNT * 17 &&
                y >= 59 && y < 76) {
                drawColor565 = brushColors[(x - 49) / 17];
                brushPaletteOpen = false;
                eyedropperMode = false;
                updateUi = true;
            } else if (brushPaletteOpen && x >= 190 && x < 244 && y >= 59 && y < 76) {
                brushPaletteOpen = false;
                eyedropperMode = true;
                updateUi = true;
            } else if (x >= 260 && x < 316 && y >= 226 && y < 239) {
                resetEdits();
                updateUi = true;
            } else if (x >= 202 && x < 248 && y >= 184 && y < 202) {
                switchCamera();
                updateUi = true;
            } else if (x >= 202 && x < 248 && y >= 204 && y < 223) {
                setVideoMode(!videoMode);
                updateUi = true;
            } else if (x >= 251 && x < 315 && y >= 184 && y < 223) {
                captureTouch = true;
            } else if (x >= 20 && x < 200 && y >= SLIDER_TOUCH_TOP &&
                       y < SLIDER_TOUCH_TOP + SLIDER_COUNT * SLIDER_ROW_HEIGHT) {
                activeSlider = (y - SLIDER_TOUCH_TOP) / SLIDER_ROW_HEIGHT;
                setSliderFromTouch(activeSlider, x);
                updateUi = true;
            } else if (y >= FILTER_SWATCH_TOP && y < FILTER_SWATCH_TOP + FILTER_SWATCH_ROW_HEIGHT &&
                       x >= FILTER_SWATCH_LEFT - 1 &&
                       x < FILTER_SWATCH_LEFT - 1 + FILTER_COUNT * FILTER_SWATCH_PITCH) {
                currentFilter = (Filter)((x - (FILTER_SWATCH_LEFT - 1)) / FILTER_SWATCH_PITCH);
                updateUi = true;
            } else if (x >= 6 && x < 35 && y >= TOOLBAR_TOP &&
                       y < TOOLBAR_TOP + (TOOLBAR_TOOL_COUNT - 1) * TOOLBAR_SLOT_HEIGHT + TOOLBAR_BUTTON_HEIGHT) {
                int tool = (y - TOOLBAR_TOP) / TOOLBAR_SLOT_HEIGHT;
                brushPaletteOpen = false;
                eyedropperMode = false;
                textColorPicking = false;
                if (textPlacementPending) photoStatus = "";
                textPlacementPending = false;
                if (tool == 0) setVideoMode(false);
                else if (tool == 1) setVideoMode(true);
                else if (tool == 2) {
                    timerMenuOpen = true;
                    timerMenuItem = captureTimerSeconds == 4 ? 1 : captureTimerSeconds == 6 ? 2 :
                                    captureTimerSeconds == 8 ? 3 : 0;
                }
                else if (tool == 3) {
                    drawEnabled = true;
                    eraserEnabled = false;
                    brushPaletteOpen = true;
                }
                else if (tool == 4) { drawEnabled = true; eraserEnabled = true; }
                else if (tool == 5) {
                    textColorPicking = true;
                    brushPaletteOpen = true;
                }
                else if (tool == 6) zoom = zoom < 4.0f ? zoom + 0.25f : 4.0f;
                else if (tool == 7) zoom = zoom > 1.0f ? zoom - 0.25f : 1.0f;
                updateUi = true;
            } else if ((drawEnabled || textPlacementPending) && x >= PREVIEW_LEFT - PAINT_TOUCH_MARGIN &&
                       x < PREVIEW_LEFT + PREVIEW_WIDTH + PAINT_TOUCH_MARGIN &&
                       y >= PREVIEW_TOP - PAINT_TOUCH_MARGIN &&
                       y < PREVIEW_TOP + PREVIEW_HEIGHT + PAINT_TOUCH_MARGIN) {
                int drawX, drawY;
                previewToFrame(x, y, &drawX, &drawY);
                brushPaletteOpen = false;
                if (textPlacementPending) {
                    placeCanvasText(drawX, drawY);
                } else if (eyedropperMode) {
                    if (cameraReady && frameNumber) drawColor565 = processedFrame[drawY * FRAME_WIDTH + drawX];
                    eyedropperMode = false;
                } else {
                    strokeActive = true;
                    beginDrawingEdit();
                    drawLine(drawX, drawY, drawX, drawY);
                    previousTouchX = drawX;
                    previousTouchY = drawY;
                    previousTouchValid = true;
                }
                updateUi = true;
            }
        }
        if (menuOpen && menuAction >= 0 && !galleryOpen) {
            if (menuIndex == 0) {
                menuOpen = false;
                if (menuAction == 0) capturePhotoRequested = true;
                else renameLastPhoto();
            } else if (menuIndex == 1) {
                if (menuAction == 0) undoDrawing();
                else if (menuAction == 1) redoDrawing();
                else currentFilter = (Filter)((currentFilter + 1) % FILTER_COUNT);
            } else {
                if (menuAction == 0) mirrorPreview = !mirrorPreview;
                else if (menuAction == 1) showHud = !showHud;
                else if (menuAction == 2) switchCamera();
                else if (menuAction == 3) hudThemeIndex = (hudThemeIndex + 1) % HUD_THEME_COUNT;
                else timestampEnabled = !timestampEnabled;
            }
            updateUi = true;
        }
        if (activeSlider >= 0) {
            if (!galleryOpen && !menuOpen && !editingMenuOpen && !timerMenuOpen && (hidKeysHeld() & KEY_TOUCH)) {
                touchPosition touch;
                hidTouchRead(&touch);
                setSliderFromTouch(activeSlider, touch.px);
                updateUi = true;
            } else {
                activeSlider = -1;
            }
        }
        if (strokeActive && drawEnabled && !galleryOpen && !menuOpen && !editingMenuOpen && !timerMenuOpen && activeSlider < 0 &&
            (hidKeysHeld() & KEY_TOUCH) && !captureTouch) {
            touchPosition touch;
            hidTouchRead(&touch);
            if (touch.px >= PREVIEW_LEFT - PAINT_TOUCH_MARGIN &&
                touch.px < PREVIEW_LEFT + PREVIEW_WIDTH + PAINT_TOUCH_MARGIN &&
                touch.py >= PREVIEW_TOP - PAINT_TOUCH_MARGIN &&
                touch.py < PREVIEW_TOP + PREVIEW_HEIGHT + PAINT_TOUCH_MARGIN) {
                int x, y;
                previewToFrame(touch.px, touch.py, &x, &y);
                if (!previousTouchValid) drawLine(x, y, x, y);
                else if (x != previousTouchX || y != previousTouchY)
                    drawLine(previousTouchX, previousTouchY, x, y);
                previousTouchX = x;
                previousTouchY = y;
                previousTouchValid = true;
            } else previousTouchValid = false;
        } else {
            strokeActive = false;
            previousTouchValid = false;
        }
        if (updateUi && (menuOpen || editingMenuOpen || timerMenuOpen || galleryOpen)) {
            strokeActive = false;
            previousTouchValid = false;
        }

        if (currentFilter == FILTER_GREEN_SCREEN && !galleryOpen) {
            if (!greenScreenBackgroundsScanned) {
                bool folderReady = scanGreenScreenBackgrounds();
                greenScreenBackgroundReady = false;
                selectGreenScreenBackground(0);
                if (!folderReady) photoStatus = "BACKGROUND FOLDER ERROR";
            }
            updateGreenScreenThumbnails();
        } else {
            greenScreenBackgroundsScanned = false;
            backdropMask.calibrated = false;
            pendingBackdropCapture = false;
        }
        if (currentFilter == FILTER_GAMEBOY) {
            if (!gameboyFramesScanned) scanGameboyFrames();
        } else {
            gameboyFramesScanned = false;
        }
        if (galleryOpen || !videoMode) pendingVideoStart = false;
        if (!galleryOpen && (recordRequested || (captureTouch && videoMode))) {
            if (movie.active) {
                finishMovie();
            } else if (pendingVideoStart) {
                pendingVideoStart = false;
            } else if (cameraReady) {
                setVideoMode(true);
                pendingVideoStart = true;
            } else {
                photoStatus = "NO CAMERA";
            }
        }
        if (!galleryOpen && (capturePhotoRequested || (captureTouch && !videoMode))) {
            if (photoSave.active) {
                photoStatus = "PHOTO BUSY";
            } else if (cameraReady) {
                if (captureTimerSeconds) {
                    captureTimerDeadline = inputTick + (u64)captureTimerSeconds * SYSCLOCK_ARM11;
                } else {
                    pendingPhoto = true;
                    photoStatus = "WAITING FRAME";
                }
            } else {
                photoStatus = "NO CAMERA";
            }
        }
        if (captureTimerDeadline) {
            if (galleryOpen || !cameraReady) {
                captureTimerDeadline = 0;
                photoStatus = galleryOpen ? "" : "NO CAMERA";
            } else if (inputTick >= captureTimerDeadline) {
                captureTimerDeadline = 0;
                pendingPhoto = true;
                photoStatus = "WAITING FRAME";
            } else {
                unsigned seconds = (unsigned)((captureTimerDeadline - inputTick + SYSCLOCK_ARM11 - 1) /
                                              SYSCLOCK_ARM11);
                snprintf(captureTimerStatus, sizeof(captureTimerStatus), "TIMER %u", seconds);
                photoStatus = captureTimerStatus;
            }
        }
        u8 *topFramebuffer = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
        if (galleryOpen) {
            updateGalleryVideo();
            renderGallery(topFramebuffer);
        } else if (cameraReady && receiveFrame()) {
            frameNumber++;
            if (pendingBackdropCapture) {
                backdropCropWidth = (int)(FRAME_WIDTH / zoom);
                backdropCropHeight = (int)(FRAME_HEIGHT / zoom);
                backdropMirrored = mirrorPreview;
                backdropCapture(&backdropMask, cameraFrame, (FRAME_WIDTH - backdropCropWidth) / 2,
                                (FRAME_HEIGHT - backdropCropHeight) / 2, backdropCropWidth,
                                backdropCropHeight, backdropMirrored);
                pendingBackdropCapture = false;
                photoStatus = "BG CAPTURED";
            }
            renderFrame(topFramebuffer);
            if (pendingPhoto) {
                pendingPhoto = false;
                if (startPhotoSave()) {
                    photoStatus = "";
                    CAMU_PlayShutterSound(SHUTTER_SOUND_TYPE_NORMAL);
                } else photoStatus = "SD SAVE FAILED";
            }
            if (pendingVideoStart) {
                pendingVideoStart = false;
                startMovie();
            }
            appendMovieFrame();
        }

        drawBottomUi();
        if (!galleryOpen && !cameraReady) memset(topFramebuffer, 0, FRAME_PIXELS * 3);
        if (!galleryOpen) drawTopUi(topFramebuffer, cameraReady);
        gfxFlushBuffers();
        gspWaitForVBlank();
        gfxSwapBuffers();
    }

    if (movie.active) finishMovie();
    closeGalleryVideo();
    faceTrackerShutdown();
    if (photoSave.worker) {
        threadJoin(photoSave.worker, U64_MAX);
        threadFree(photoSave.worker);
    }
    stopCamera();
    if (cameraFrame) linearFree(cameraFrame);
    gfxExit();
    return 0;
}