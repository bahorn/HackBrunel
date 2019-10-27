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

typedef struct state {
    screen_info *si;
    UINT64 t;
} state;

state gs;

float sin(float in) {
    float out;
    __asm__ (
            "fldl %1;"
            "fsin;"
            "fstl %0;"
            : "=m" (out) : "m" (in)
    );

    return out;
}

float cos(float in) {
    float out;
    __asm__ (
            "fldl %1;"
            "fcos;"
            "fstl %0;"
            : "=m" (out) : "m" (in)
    );

    return out;
}

float sqrt(float in) {
    float out;
    __asm__ (
            "fldl %1;"
            "fsqrt;"
            "fstl %0;"
            : "=m" (out) : "m" (in)
    );

    return out;
}

UINT32 color = 0;


/* Function called for every frame */
void
main_loop(
    IN EFI_EVENT  Event,
    IN VOID       *Context
) {
    int x,y;
    screen_info *si = gs.si;
    //UINT64 t = gs.t;
    /* Clear */
    /* */
    for (y = 0; y < si->height; y+=1 ) {
        for (x = 0; x < si->width; x+=1 ) {
            
            //UINT32 color = sin(t/100)*x + y % 0xffff;
            SetMem32(&si->fb[y*si->width + x], 4, color);
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
    color += 0xff;
    
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
    gs.si = AllocatePool(sizeof(sizeof(screen_info)));
    gs.t = 0;
    screen_info *screen = gs.si;;

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
        EFI_TIMER_PERIOD_MILLISECONDS (30)
    );
    CHECK_STATUS();

    Print(L"done\n");
    while (1){}
    return EFI_SUCCESS;
}
