/* Native Linux runtime smoke test. No retail files, no display and no audio
 * hardware are needed: SDL runs on its dummy drivers.
 *
 * Exercises the pieces the native port adds to the runtime:
 *   1. the sigaction VEH in win32_compat (fault -> CONTEXT -> fix -> resume,
 *      the exact contract the APU/NV2A MMIO decoders rely on);
 *   2. the SDL framebuffer presenter (publish -> window -> BMP dump);
 *   3. the SDL audio backend behind the xa2_* contract;
 *   4. keyboard events merged into the guest pad report by pc_input.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>

#include "platform/win32_compat.h"
#include "fb_present.h"
#include "apu_xaudio2.h"
#include "pc_input.h"

static int failures;
#define CHECK(cond, ...) do { \
    if (!(cond)) { fprintf(stderr, "FAIL %d: ", __LINE__); \
                   fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); failures++; } \
} while (0)

/* ---- 1. VEH: the handler repairs the fault and execution resumes -------- */
static volatile void *s_trap_page;
static volatile LONG s_trap_hits;

static LONG CALLBACK trap_handler(PEXCEPTION_POINTERS ep)
{
    if (ep->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION)
        return EXCEPTION_CONTINUE_SEARCH;
    if ((volatile void *)ep->ExceptionRecord->ExceptionInformation[1] != s_trap_page &&
        ((uintptr_t)ep->ExceptionRecord->ExceptionInformation[1] & ~(uintptr_t)0xFFF) !=
            ((uintptr_t)s_trap_page & ~(uintptr_t)0xFFF))
        return EXCEPTION_CONTINUE_SEARCH;
    InterlockedIncrement(&s_trap_hits);
    /* The MMIO decoders advance Rip instead; here making the page writable
     * is the repair, and CONTINUE_EXECUTION retries the same instruction. */
    if (mprotect((void *)((uintptr_t)s_trap_page & ~(uintptr_t)0xFFF), 4096,
                 PROT_READ | PROT_WRITE) != 0)
        return EXCEPTION_CONTINUE_SEARCH;
    return EXCEPTION_CONTINUE_EXECUTION;
}

static void test_veh(void)
{
    void *page = mmap(NULL, 4096, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    volatile uint32_t *word;
    CHECK(page != MAP_FAILED, "mmap trap page");
    if (page == MAP_FAILED) return;
    s_trap_page = page;
    CHECK(AddVectoredExceptionHandler(1, trap_handler) != NULL, "VEH register");
    word = (volatile uint32_t *)page;
    *word = 0xB10C;              /* faults once, handler repairs, write lands */
    CHECK(*word == 0xB10C, "write after VEH repair landed");
    CHECK(s_trap_hits == 1, "VEH handler ran once (got %ld)", (long)s_trap_hits);
    CHECK(*(volatile uint32_t *)page == 0xB10C, "CONTEXT fault address matched");
    munmap(page, 4096);
    puts("PASS: VEH delivers access violations and resumes after repair.");
}

/* ---- 4. pc_input: a held key reaches the pad report ---------------------- */
/* In play the game installs its own resolver (guest action slot -> the
 * control the live action map binds it to); identity is enough here. */
static int test_resolver(int slot) { return slot >= 0 && slot < 28 ? slot : -1; }

static void test_input(void)
{
    pc_input_set_control_resolver(test_resolver);
    uint8_t neutral[20], merged[20];
    memset(neutral, 0, sizeof(neutral));
    memset(merged, 0, sizeof(merged));
    pc_input_init();
    pc_input_apply_report(neutral, 0);
    pc_input_gameplay_tick();                 /* the guest camera update is running */
    pc_input_key_event(0x11, 0, 1, 0);        /* W, set-1 scancode */
    pc_input_frame_complete();
    pc_input_apply_report(merged, 0);
    CHECK(memcmp(neutral, merged, sizeof(neutral)) != 0,
          "held W changes the pad report");
    pc_input_key_event(0x11, 0, 0, 0);
    puts("PASS: keyboard events merge into the pad report.");
}

/* ---- 3. SDL audio behind xa2_* ------------------------------------------- */
static void test_audio(void)
{
    static int16_t samples[1024 * 2];
    Xa2OutputStats stats;
    int i, accepted = 0, backpressure = 0;

    CHECK(xa2_init() == 1, "SDL audio init (dummy driver)");
    if (!xa2_is_active()) return;
    for (i = 0; i < 8; i++) {
        int r = xa2_submit_samples(samples, 1024);
        if (r == XA2_SUBMIT_ACCEPTED) accepted++;
        else if (r == XA2_SUBMIT_BACKPRESSURE) backpressure++;
    }
    CHECK(accepted >= 2, "audio packets accepted (%d)", accepted);
    CHECK(backpressure >= 1, "full queue is backpressure, not a drop");
    xa2_get_output_stats(&stats);
    CHECK(stats.submitted_samples >= 2048, "stats count submitted samples");
    CHECK(xa2_get_buffer_size() == 1024, "buffer size contract");
    xa2_shutdown();
    CHECK(!xa2_is_active(), "audio shutdown");
    puts("PASS: SDL audio backend honours the xa2 submit contract.");
}

/* ---- 2. SDL presenter: publish, present, dump ---------------------------- */
static void test_presenter(void)
{
    static uint32_t frame[64 * 64];
    const char *dump = "linux_native_frame.bmp";
    int i;
    FILE *f;

    for (i = 0; i < 64 * 64; i++)
        frame[i] = 0xFF204060u;
    remove(dump);
    /* Both are read once, when the presenter initializes. */
    setenv("RECOMP_FB_WINDOW", "1", 0);
    setenv("RECOMP_FB_DUMP", dump, 0);
    xbox_FramebufferWindowStart();
    for (i = 0; i < 90; i++) {
        xbox_FramebufferPublish(frame, sizeof(frame), 0xF00, 64, 64,
                                64 * 4, 8 /* A8R8G8B8-family 32bpp */);
        usleep(4000);
    }
    for (i = 0; i < 250 && !(f = fopen(dump, "rb")); i++)
        usleep(10000);
    CHECK(f != NULL, "presenter produced RECOMP_FB_DUMP bmp");
    if (f) {
        uint8_t hdr[2] = {0};
        CHECK(fread(hdr, 1, 2, f) == 2 && hdr[0] == 'B' && hdr[1] == 'M',
              "dump is a BMP");
        fclose(f);
        remove(dump);
    }
    puts("PASS: SDL presenter publishes and presents frames.");
}

int main(void)
{
    test_veh();
    test_input();
    test_audio();
    test_presenter();
    if (failures) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        fflush(stdout);
        _exit(1);
    }
    puts("PASS: native Linux runtime (VEH, input, audio, presenter).");
    fflush(stdout);
    /* Last: RequestClose ends the process from the presenter thread with
     * status 0, exactly as closing the window does in play. */
    if (!xbox_FramebufferRequestClose())
        _exit(1);
    for (;;) pause();
    return 0;
}
