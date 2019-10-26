#include <Uefi.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/SimpleTextInEx.h>

#define CHECK_STATUS()  if (EFI_ERROR (Status)) { \
                            Print(L"%r\n", Status); \
                            return Status; \
                        }

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
} state;

state gs;

UINT32 test = 0x00000001;

/* Function called for every frame */
void
main_loop(
    IN EFI_EVENT  Event,
    IN VOID       *Context
) {

    screen_info *si = gs.si;
    //SetMem32(si->fb, si->size, test);
    //test = 10*(test+1);

    /* Display the frame buffer */
    gfx->Blt(
        gfx,
        si->fb,
        EfiBltVideoFill,
        0,
        0,
        0,
        0,
        si->width,
        si->height,
        0
    );
}


EFI_STATUS EFIAPI
UefiMain (
        IN EFI_HANDLE ImageHandle,
        IN EFI_SYSTEM_TABLE  *SystemTable
)
{
    MAIN_DEVICE device;
    EFI_HANDLE *gfx_controller;//, *io_controller;
    UINTN gfx_handlecnt;//, io_handlecnt;
    gs.si = AllocatePool(sizeof(sizeof(screen_info)));
    screen_info *screen = gs.si;;

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
        EFI_TIMER_PERIOD_MILLISECONDS (10)
    );
    CHECK_STATUS();

    Print(L"done\n");
    while(1){}
    return EFI_SUCCESS;
}
