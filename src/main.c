#include <Uefi.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/SimpleTextInEx.h>
#include <Protocol/SimpleFileSystem.h>

#define CHECK_STATUS()  if (EFI_ERROR (Status)) { \
                            Print(L"%r\n", Status); \
                            return Status; \
                        }

#define PI 3.14
#define PIXEL_SIZE 20

/* Graphics */
EFI_GRAPHICS_OUTPUT_PROTOCOL *gfx;
EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *input;
EFI_STATUS Status;

typedef struct  {
    UINTN Signature;
    EFI_EVENT PeriodicTimer;
} MAIN_DEVICE;

typedef struct screen_info {
    UINTN width;
    UINTN height;
    UINTN size;
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL *fb;
} screen_info;

typedef struct position {
    UINT32 x;
    UINT32 y;
} position;

enum block_type {
    CLEAR = 0,
    SNAKE = 1,
    FOOD = 2,
};

typedef struct grid_data {
    UINT32 id; /* used to find the order in which to remove things in. Slow, but we only have 
    to check 1200 things each state update */
    enum block_type type;
} grid_data;

typedef struct grid {
    UINTN g_width;
    UINTN g_height;
    grid_data *d;
} grid;

typedef struct state {
    screen_info *si;
    INT64 t;
    INT64 frame;
    position *pos;
    grid *g;
    UINT32 speed;
    UINT32 length;
    UINT16 key;

} state;

state gs;

float
sin(float in) {
    float out;
    __asm__ (
            "fldl %1;"
            "fsin;"
            "fstl %0;"
            : "=m" (out) : "m" (in)
    );

    return out;
}

float
cos(float in) {
    float out;
    __asm__ (
            "fldl %1;"
            "fcos;"
            "fstl %0;"
            : "=m" (out) : "m" (in)
    );

    return out;
}

float
sqrt(float in) {
    float out;
    __asm__ (
            "fldl %1;"
            "fsqrt;"
            "fstl %0;"
            : "=m" (out) : "m" (in)
    );

    return out;
}

//UINT32 color = 0;

UINT32 seed = 132342232;

UINT32
lcg(UINT32 modulus, UINT32 a, UINT32 c) {
    seed = (a*seed + c) % modulus;
    return seed;
}

void
draw_square(UINT32 x, UINT32 y, UINT32 length, UINT32 color_sq) {
    int i;
    screen_info *si = gs.si;
    for (i = 0; i < length; i++) {
        SetMem32(&si->fb[(y+i)*si->width + x], 4*length, color_sq);
    }
}

void
place_food() {
    int x = lcg(0xffffffff, 2, 10) % gs.g->g_width;
    int y = lcg(0xffffffff, 2, 10) % gs.g->g_height;
    grid_data *t2 = &gs.g->d[y*gs.g->g_width + x];

    while (t2->type == SNAKE) {
        /* generate more x and ys */
        x = lcg(0xffffffff, 2, 10) % gs.g->g_width;
        y = lcg(0xffffffff, 2, 10) % gs.g->g_height;
        t2 = &gs.g->d[y*gs.g->g_width + x];
    }
    t2->type = FOOD;
}

void
reset() {
    gs.length = 1;
    gs.speed = 50;
    gs.pos = AllocatePool(sizeof(position));
    gs.pos->x = 20;
    gs.pos->y = 20;
    gs.frame += 1200;
}


UINT32
progress_state() {
    /* Only run state updates */
    if ((gs.t % gs.speed) != 0) {
        return 0;
    }
    int x, y;
    x = y = 0;
    gs.frame += 1;
    /* Check position most recently seen */
    /* Add our new position */
    switch (gs.key) {
        case SCAN_UP:
            y = -1;
            break;
        case SCAN_DOWN:
            y = 1;
            break;
        case SCAN_LEFT:
            x = -1;
            break;
        case SCAN_RIGHT:
            x = 1;
            break;
        default:
            break;
    }

    gs.pos->x += x;
    gs.pos->y += y;

    /* Check boundaries */
    if (gs.pos->x < 0 || gs.pos->x >= gs.g->g_width) {
        reset();
    }

    if (gs.pos->y < 0 || gs.pos->y >= gs.g->g_height) {
        reset();
    }


    grid_data *t = &gs.g->d[gs.pos->y*gs.g->g_width + gs.pos->x];
    
    /* Check if we hit a snake or food */
    switch (t->type) {
        case SNAKE:
            reset();
            break;
        case FOOD:
            gs.length += 1;
            gs.speed -= 5;
            if (gs.speed < 5) {
                gs.speed = 5;
            }
            place_food();
            break;
        default:
            break;
    }
    

    t->type = SNAKE;
    t->id = gs.frame;
    

    /* Make the drawing function clear it. */
    return (gs.frame-gs.length);
}

/* Function called for every frame */
void
main_loop(
    IN EFI_EVENT  Event,
    IN VOID       *Context
) {
    int x,y;
    INT32 min_age = 0;
    screen_info *si = gs.si;
    //INT64 t = gs.t;

    /* Update the state */
    min_age = progress_state();

    /* Clear */
    /* */
    /*
    for (y = 0; y < si->height; y+=1 ) {
        for (x = 0; x < si->width; x+=1 ) {
            UINT32 color = 0x00;
            SetMem32(&si->fb[y*si->width + x], 4, color);
        }
    }*/

    /* Now draw the game state by cycling though the grid_data*/
    for (y = 0; y < gs.g->g_height; y++) {
        for (x = 0; x < gs.g->g_width; x++) {
            UINT32 color = 0x00;
            grid_data *t = &gs.g->d[y*gs.g->g_width + x];
            /* Check the age to see if we need to clear it */
            if ((t->id < min_age) && (t->type == SNAKE)) {
                t->type = CLEAR;
            }
            switch (t->type) {
                case CLEAR:
                    color = 0xffffff;
                    break;
                case SNAKE:
                    color = 0xff;
                    break;
                case FOOD:
                    color = 0xff0000;
                    break;
                default:
                    break;
            }
            draw_square(x*PIXEL_SIZE, y*PIXEL_SIZE, PIXEL_SIZE, color);
        }
    }

    /* Display the frame buffer */
    
    gfx->Blt(
        gfx,
        si->fb,
        EfiBltBufferToVideo,
        0,
        0,
        0,
        0,
        si->width,
        si->height,
        0
    );

    gs.t += 1;

}

EFI_STATUS
EFIAPI
KeyPressed(
  IN EFI_KEY_DATA *KeyData
  )
{
    //color += 0xff;
    /* Make sure we can't invert the direction. */
    
    switch (KeyData->Key.ScanCode) {
        case SCAN_UP:
            if (gs.key != SCAN_DOWN) {
                gs.key = KeyData->Key.ScanCode;
            }
            break;
        case SCAN_DOWN:
            if (gs.key != SCAN_UP) {
                gs.key = KeyData->Key.ScanCode;
            }
            break;
        case SCAN_LEFT:
            if (gs.key != SCAN_RIGHT) {
                gs.key = KeyData->Key.ScanCode;
            }
            break;
        case SCAN_RIGHT:
            if (gs.key != SCAN_LEFT) {
                gs.key = KeyData->Key.ScanCode;
            }
            break;


    }
    
    return EFI_SUCCESS;
}


EFI_STATUS EFIAPI
UefiMain (
        IN EFI_HANDLE ImageHandle,
        IN EFI_SYSTEM_TABLE  *SystemTable
)
{
    MAIN_DEVICE device;
    EFI_HANDLE *gfx_controller, *io_controller;
    UINTN gfx_handlecnt, io_handlecnt;
    gs.si = AllocatePool(sizeof(screen_info));
    gs.t = 0;
    gs.pos = AllocatePool(sizeof(position));
    reset();
    gs.g = AllocatePool(sizeof(grid));
    gs.g->g_width = 800/PIXEL_SIZE;
    gs.g->g_height = 600/PIXEL_SIZE;
    int t = sizeof(grid_data)* gs.g->g_width * gs.g->g_height;
    gs.g->d = (void *)AllocatePool(t);
    gs.key = SCAN_DOWN;
    gs.frame = 1;
    SetMem(gs.g->d, t, 0x00);
    screen_info *screen = gs.si;

    int i, j;
    void *io_notify_handle;
    EFI_KEY_DATA key_info;


    /* Get our graphics handler */
    Status = gBS->LocateHandleBuffer(
        ByProtocol,
        &gEfiGraphicsOutputProtocolGuid,
        NULL,
        &gfx_handlecnt,
        &gfx_controller
    );
    CHECK_STATUS();

    Status = gBS->HandleProtocol(
        gfx_controller[0],
        &gEfiGraphicsOutputProtocolGuid,
        (VOID **)&(gfx)
    );
    CHECK_STATUS();

    /* Get our Input handler */
    Status = gBS->LocateHandleBuffer(
        ByProtocol,
        &gEfiSimpleTextInputExProtocolGuid,
        NULL,
        &io_handlecnt,
        &io_controller
    );
    CHECK_STATUS();

    for (i = 0; i < io_handlecnt; i++) {
        Status = gBS->HandleProtocol(
            io_controller[i],
            &gEfiSimpleTextInputExProtocolGuid,
            (VOID **)&(input)
        );
        CHECK_STATUS();

        key_info.KeyState.KeyToggleState = EFI_TOGGLE_STATE_VALID|EFI_KEY_STATE_EXPOSED;
        key_info.Key.ScanCode            = SCAN_UP;
        key_info.KeyState.KeyShiftState  = 0;
        /* Setup the keys we care about */
        UINTN key_buffer[] = {SCAN_UP, SCAN_DOWN, SCAN_LEFT, SCAN_RIGHT};
        for (j = 0; j < 4; j++) {
            key_info.Key.ScanCode = key_buffer[j];
            /* Now go register our input handlers */
            Status = input->RegisterKeyNotify(
                input,
                &key_info,
                KeyPressed,
                &io_notify_handle
            );
            CHECK_STATUS();
        }
    }

    /* Setup the screen */
    screen->width = gfx->Mode->Info->HorizontalResolution;
    screen->height = gfx->Mode->Info->VerticalResolution;
    screen->size = screen->width
        * screen->height * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL);

    screen->fb = AllocatePool(screen->size);
    place_food();
    /* Setup a timer to call the loop function periodically */
    Status = gBS->CreateEvent (
        EVT_TIMER | EVT_NOTIFY_SIGNAL,
        TPL_NOTIFY,
        (EFI_EVENT_NOTIFY) main_loop,
        &device,
        &device.PeriodicTimer
    );
    Status = gBS->SetTimer(
        device.PeriodicTimer,
        TimerPeriodic,
        EFI_TIMER_PERIOD_MILLISECONDS (10)
    );
    CHECK_STATUS();

    Print(L"done\n");
    while (1){}
    return EFI_SUCCESS;
}
