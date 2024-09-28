#include <3ds.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "sintable.h"
#include "font.h"

#define CURVECOUNT 256
#define CURVESTEP 4
#define ITERATIONS 256
#define SCREENWIDTH 240
#define SCREENHEIGHT 400
#define SIZE 120
#define PI 3.1415926535897932384626433832795
#define SINTABLEPOWER 14
#define SINTABLEENTRIES (1<<SINTABLEPOWER)
#define ANG1INC (s32)((CURVESTEP * SINTABLEENTRIES) / 235)
#define ANG2INC (s32)((CURVESTEP * SINTABLEENTRIES) / (2*PI))
#define SCALEMUL (s32)(SIZE*PI)
#define MAXDEPTH 32
#define INITIALDEPTHOFFSET 0

#define keysDownRepeat hidKeysDownRepeat
#define keysSetRepeat hidSetRepeatParameters

bool trails = false;
bool quit = false;
s32 speed = 8;
s32 oldSpeed = 0;
bool justReset = false;
s32 scaleMul = SCALEMUL;
s32 xPan = 0;
s32 yPan = 0;
s32 depthOffset = INITIALDEPTHOFFSET;

s32 SinTable[SINTABLEENTRIES];
u16 ColourTable[ITERATIONS*CURVECOUNT/CURVESTEP];
u8 fontData[fontTilesLen];

void ExpandSinTable()
{
    for (int i = 0; i < SINTABLEENTRIES/4; ++i)
    {
        SinTable[i] = SinTable[SINTABLEENTRIES/2 - i - 1] = compactsintable[i];
        SinTable[SINTABLEENTRIES/2 + i] = SinTable[SINTABLEENTRIES - i - 1] = -compactsintable[i];
    }
    for (int i = 0; i < SINTABLEENTRIES; ++i)
    {
        *((s16*)(SinTable+i)+1) = *(s16*)(SinTable+((i+SINTABLEENTRIES/4)%SINTABLEENTRIES));
    }
}

void InitColourTable()
{
    int colourIndex = 0;
    for (u32 i = 0; i < CURVECOUNT; i += CURVESTEP)
    {
        const u32 red = (256 * i) / CURVECOUNT;
        for (int j = 0; j < ITERATIONS; ++j)
        {
            const u32 green = (256 * j) / ITERATIONS;
            const u32 blue = (512-(red+green))>>1;
            ColourTable[colourIndex++] = ((red>>3)<<0) | ((green>>2)<<5) | ((blue>>3)<<11);
        }
    }
}

void InitConsole()
{
    consoleInit(GFX_BOTTOM, NULL);

    for (int i = 0; i < fontTilesLen; ++i)
    {
        u8 row = ((u8*)fontTiles)[i];
        fontData[i] = ((row &  1) << 7) | ((row &  2) << 5) | ((row &  4) << 3) | ((row &   8) << 1) | 
                      ((row & 16) >> 1) | ((row & 32) >> 3) | ((row & 64) >> 5) | ((row & 128) >> 7);
    }

    ConsoleFont font;
    font.gfx = fontData;
    font.asciiOffset = 32;
    font.numChars = 256-font.asciiOffset;
    consoleSetFont(NULL, &font);
}

void ColourText(int x1, int y1, int x2, int y2, int xo, int yo)
{
    u16* buffer = (u16*)gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);
    for (int y = y1 + yo; y < y2; ++y)
    {
        for (int x = x1 + xo; x < x2; ++x)
        {
            if (buffer[x*240+239-y] > 0)
            {
                const u32 red = (256 * (x-x1)) / (x2-x1);
                const u32 green = (256 * (y2-y-1)) / (y2-y1);
                const u32 blue = (512-(red+green))>>1;
                buffer[x*240+239-y] = ((red>>3)<<0) | ((green>>2)<<5) | ((blue>>3)<<11);
            }
        }
    }
}

void Instructions()
{
    for (int i = 128; i < 224; ++i)
    {
        if (!(i%32))
        {
            iprintf("\n    ");
        }
    	iprintf("%c",i);
    }
	iprintf(
        "\n\n\n"
        "       Circle-Pad : Move around\n"
        "    D-pad Up/Down : 3D offset in/out\n"
        "              A/B : Zoom in/out\n"
        "              A+B : Reset view\n"
        "              X/Y : Speed inc/dec\n"
        "        L/R + A/B : Faster zoom\n"
        "            Start : Pause/Unpause\n"
        "           Select : Toggle trails\n"
        "       Hold Start : Exit\n"
        "\n\n"
        "        Particles : %d\n"
        "            Speed : \x1b[s\n"
        "        3D Offset :\n"
        "\n"
        "            Frame :\n"
        "            Vsync :\n"
        "\n\n\n"
        "            By Movie Vertigo\n"
        "        youtube.com/movievertigo\n"
        "        twitter.com/movievertigo",
        ITERATIONS * CURVECOUNT / CURVESTEP
    );

    ColourText(32, 8, 288, 32, 0, 0);
    ColourText(32, 48, 288, 120, 0, 0);
    ColourText(32, 136, 288, 184, 0, 0);
    ColourText(32, 208, 288, 232, 0, 0);
}

void Controls()
{
    circlePosition circlePos;
    scanKeys();

    int pressed = keysHeld();
    int modSpeed = (pressed & KEY_L ? 4 : 1) + (pressed & KEY_R ? 8 : 0);
    hidCircleRead(&circlePos);
    xPan -= circlePos.dx/16;
    yPan += circlePos.dy/16;
    if ((pressed & KEY_A) && !justReset) scaleMul += modSpeed;
    if ((pressed & KEY_B) && !justReset) scaleMul -= modSpeed;
    if (!(pressed & (KEY_A|KEY_B))) justReset = false;

    pressed = keysDownRepeat();
    if ((pressed & KEY_A) && (pressed & KEY_B)) { scaleMul = SCALEMUL; xPan = yPan = 0; justReset = true; }
    if(pressed & KEY_X) speed += modSpeed;
    if (pressed & KEY_Y) speed -= modSpeed;
    if (pressed & KEY_DUP) ++depthOffset;
    if (pressed & KEY_DDOWN) --depthOffset;
    bool startHeld = pressed & KEY_START;

    pressed = keysDown();
    if (pressed & KEY_START)
    {
        if (speed)
        {
            oldSpeed = speed;
            speed = 0;
        }
        else
        {
            speed = oldSpeed;
        }
    }
    else if (startHeld)
    {
        quit = true;
    }

    if (pressed & KEY_SELECT)
    {
        trails = !trails;
    }
}

u64 getusec()
{
    return svcGetSystemTick() / CPU_TICKS_PER_USEC;
}

int main(void)
{
    osSetSpeedupEnable(true);

    gfxInit(GSP_RGB565_OES,GSP_BGR8_OES,false);
    gfxSetDoubleBuffering(GFX_TOP, true);
    gfxSetDoubleBuffering(GFX_BOTTOM, false);
    gfxSet3D(true);

    ExpandSinTable();
    InitColourTable();
    InitConsole();
    Instructions();

    keysSetRepeat(32, 4);

    s32 animationTime = 0;
    u32 startTime = getusec();
	while (aptMainLoop() && !quit)
	{
        u16* leftBuffer = (u16*)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
        u16* rightBuffer = (u16*)gfxGetFramebuffer(GFX_TOP, GFX_RIGHT, NULL, NULL);
        float slider = osGet3DSliderState();

        if (!trails)
        {
            memset(leftBuffer, 0, SCREENWIDTH*SCREENHEIGHT*2);
            memset(rightBuffer, 0, SCREENWIDTH*SCREENHEIGHT*2);
        }

        s32 ang1Start = animationTime;
        s32 ang2Start = animationTime;

        u16* leftScreenCentre = leftBuffer + ((SCREENWIDTH + SCREENWIDTH*SCREENHEIGHT)>>1);
        u16* rightScreenCentre = rightBuffer + ((SCREENWIDTH + SCREENWIDTH*SCREENHEIGHT)>>1);

        u16* colourPtr = ColourTable;
        for (u32 i = 0; i < CURVECOUNT; i += CURVESTEP)
        {
            const s32 depth = (((s32)i*MAXDEPTH)/CURVECOUNT-MAXDEPTH/2-depthOffset)*slider;
            const s32 leftoffset = depth/2;
            const s32 rightoffset = leftoffset-depth;
            s32 x = 0, y = 0;
            for (u32 j = 0; j < ITERATIONS; ++j)
            {
                s32 values1, values2, pX, pY;

                values1 = SinTable[(ang1Start + x)&(SINTABLEENTRIES-1)];
                values2 = SinTable[(ang2Start + y)&(SINTABLEENTRIES-1)];
                x = (s32)(s16)values1 + (s32)(s16)values2;
                y = (values1>>16) + (values2>>16);
                pX = ((-y * scaleMul) >> SINTABLEPOWER) - yPan;
                pY = ((x * scaleMul) >> SINTABLEPOWER) + xPan;
                if (pX >= -(SCREENWIDTH>>1) && pX < (SCREENWIDTH>>1))
                {
                    if (pY+leftoffset >= -(SCREENHEIGHT>>1) && pY+leftoffset < (SCREENHEIGHT>>1))
                    {
                        leftScreenCentre[(pY+leftoffset)*SCREENWIDTH + pX] = *colourPtr;
                    }
                    if (pY+rightoffset >= -(SCREENHEIGHT>>1) && pY+rightoffset < (SCREENHEIGHT>>1))
                    {
                        rightScreenCentre[(pY+rightoffset)*SCREENWIDTH + pX] = *colourPtr;
                    }
                }

                colourPtr++;
            }

            ang1Start += ANG1INC;
            ang2Start += ANG2INC;
        }

        u32 usec = getusec()-startTime;
		gfxFlushBuffers();
		gfxSwapBuffers();
		gspWaitForVBlank();
        u32 usecvsync = getusec()-startTime;
        startTime = getusec();
        iprintf("\x1b[u%ld  \x1b[u\x1b[1B%ld  \x1b[u\x1b[3B%ldms %ldfps  \x1b[u\x1b[4B%ldms %ldfps  \n", speed, depthOffset, usec/1000, (2000000/usec+1)/2, usecvsync/1000, (2000000/usecvsync+1)/2);
        ColourText(32, 136, 288, 184, 128, 8);

        Controls();

        animationTime += speed;
	}

	gfxExit();
	return 0;
}
