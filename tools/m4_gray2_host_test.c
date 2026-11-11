#include "m4_gray2.h"

#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

int main(void) {
    unsigned char portrait[480u * 800u / 4u];
    unsigned char panel[800u * 480u / 4u];
    memset(portrait, 0xff, sizeof(portrait));
    portrait[0] = 0x00; /* first four logical pixels are black */
    assert(m4Gray2PortraitToPanel(portrait, sizeof(portrait), panel, sizeof(panel)));
    /* logical (0,0) -> physical (0,479), logical (3,0) -> (0,476) */
    assert((panel[(479u * 800u) / 4u] >> 6) == 0);
    assert((panel[(476u * 800u) / 4u] >> 6) == 0);
    /* Quantization preserves the four ordered Gray4 source bands. */
    unsigned char gray4[480u * 800u / 2u];
    const unsigned char gray4Levels[] = {0x00, 0x55, 0xaa, 0xff};
    for (unsigned band = 0; band < 4; ++band) {
        memset(gray4, gray4Levels[band], sizeof(gray4));
        assert(m4ProjectGray4ToGray2(gray4, sizeof(gray4), portrait, sizeof(portrait), 480, 800));
        assert(m4Gray2PortraitToPanel(portrait, sizeof(portrait), panel, sizeof(panel)));
        const unsigned level = (panel[0] >> 6) & 3u;
        assert(level == band);
    }
    const uint8_t source[] = {0xfau, 0x50u, 0xa5u, 0x0fu};
    uint8_t destination[2] = {0xffu, 0xffu};
    assert(m4ProjectGray4ToGray2(source, sizeof(source), destination, sizeof(destination), 8, 1));
    assert(destination[0] == 0xb1u);
    assert(destination[1] == 0x6cu);
    assert(!m4ProjectGray4ToGray2(source, sizeof(source) - 1, destination, sizeof(destination), 8, 1));
    assert(!m4ProjectGray4ToGray2(source, sizeof(source), destination, sizeof(destination) - 1, 8, 1));
    assert(!m4ProjectGray4ToGray2(source, sizeof(source), destination, sizeof(destination), 6, 1));

    const uint8_t panelGray[] = {0x1bu, 0x6cu}; // black,dark,light,white,dark,light,white,black
    uint8_t bw = 0, lsb = 0, msb = 0;
    assert(m4Gray2ToSsd1677Planes(panelGray, sizeof(panelGray), 8, 1,
                                  &bw, 1, 1, &lsb, &msb, 1));
    assert(bw == 0x7eu);
    assert(lsb == 0x6cu);
    assert(msb == 0x48u);
    assert(!m4Gray2ToSsd1677Planes(panelGray, sizeof(panelGray), 8, 1,
                                   &bw, 1, 0, &lsb, &msb, 1));
    puts("M4 Gray4-to-Gray2 tests passed");
    return 0;
}
