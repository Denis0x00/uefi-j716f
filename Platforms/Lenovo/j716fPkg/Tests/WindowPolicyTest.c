/* Host checks compile the actual firmware policy, not a reimplementation. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#define STATIC static
#define BOOLEAN int
#define TRUE 1
#define FALSE 0
#define UINT64 uint64_t
#define UINTN uintptr_t
#define MAX_UINT64 UINT64_MAX
#include "WindowPolicy.h"

static unsigned cases;
static void check(uint64_t start, uint64_t pages, int ok, uint64_t expected)
{
    uint64_t address = 0xfeedface;
    assert(J716fChooseWindow(start, pages, &address) == ok);
    assert(address == (ok ? expected : 0xfeedface));
    ++cases;
}
int main(void)
{
    check(0xd0000000, 0x18000, 1, 0xd007c000);
    check(0xd0000000, 0x1007c, 1, 0xd007c000); /* exact fit */
    check(0xd0000000, 0x1007b, 0, 0); /* one page too short */
    check(0xc0000000, 0x0e000, 0, 0); /* old DXE heap */
    check(0xc0000000, 0x28000, 1, 0xd007c000); /* clipped lower bound */
    check(0xd0000000, 0x30000, 1, 0xd007c000); /* clipped upper bound */
    check(0xd0001000, 0x17fff, 1, 0xd027c000);
    /* Match fixed DarwinBoot ALIGN_VALUE(start, 2MiB) + kernel residue. */
    check(0xd007c000, 0x10000, 0, 0);
    check(0xe8000000, 0x10000, 0, 0);
    check(0, 0, 0, 0);
    check(0, UINT64_MAX, 0, 0);
    check(UINT64_MAX - 0xfff, 1, 0, 0);
    check(0xd0000000, UINT64_MAX >> 12, 0, 0);
    for (uint64_t a = 0xd007c000; a < 0xe007c000; a += 0x1000) {
        assert((J716fPattern(a, 0) ^ J716fPattern(a, 1)) == UINT64_MAX);
        assert(J716fPattern(a, 0) != J716fPattern(a + 8, 0));
    }
    printf("WindowPolicy: %u boundary cases and 65536 pattern pairs PASS\n", cases);
    return 0;
}
