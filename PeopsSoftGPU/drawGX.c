/***************************************************************************
    drawGX.m
    PeopsSoftGPU for cubeSX/wiiSX

    Created by sepp256 on Thur Jun 26 2008.
    Copyright (c) 2008 Gil Pedersen.
    Adapted from draw.c by Pete Bernet and drawgl.m by Gil Pedersen.
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version. See also the license.txt file for *
 *   additional informations.                                              *
 *                                                                         *
 ***************************************************************************/

#include <gccore.h>
#include <malloc.h>
#include <time.h>
#include <ogc/lwp_watchdog.h>
#include "../coredebug.h"
#include "stdafx.h"
#define _IN_DRAW
#include "externals.h"
#include "gpu.h"
#include "draw.h"
#include "prim.h"
#include "menu.h"
#include "swap.h"
#include "../Gamecube/libgui/IPLFontC.h"
#include "../Gamecube/DEBUG.h"
#include "../Gamecube/wiiSXconfig.h"

////////////////////////////////////////////////////////////////////////////////////
// misc globals
////////////////////////////////////////////////////////////////////////////////////
int            iResX;
int            iResY;
long           lLowerpart;
BOOL           bIsFirstFrame = TRUE;
BOOL           bCheckMask=FALSE;
unsigned short sSetMask=0;
unsigned long  lSetMask=0;
int            iDesktopCol=16;
int            iShowFPS=1;
int            iWinSize;
int            iUseScanLines=0;
int            iUseNoStretchBlt=0;
int            iFastFwd=0;
int            iDebugMode=0;
int            iFVDisplay=0;
PSXPoint_t     ptCursorPoint[8];
unsigned short usCursorActive=0;
bool 		   backFromMenu=0;

//Some GX specific variables
#define RESX_MAX 1024	//Vmem width
#define RESY_MAX 512	//Vmem height
#define GXRESX_MAX 1366	//1024 * 1.33 for ARGB
//int	iResX_Max=640;	//Max FB Width
int		iResX_Max=RESX_MAX;
int		iResY_Max=RESY_MAX;
static unsigned char	GXtexture[GXRESX_MAX*RESY_MAX*2] __attribute__((aligned(32)));
char *	pCaptionText;

extern u32* xfb[2];	/*** Framebuffers ***/
extern int whichfb;        /*** Frame buffer toggle ***/
extern time_t tStart;
extern char text[DEBUG_TEXT_HEIGHT][DEBUG_TEXT_WIDTH]; /*** DEBUG textbuffer ***/
extern char menuActive;
extern char screenMode;


// prototypes
void BlitScreenNS_GX(unsigned char * surf,long x,long y, short dx, short dy);
void GX_Flip(short width, short height, u8 * buffer, int pitch, u8 fmt);

void switchToTVMode(short dWidth, short dHeight, bool retMenu);

void DoBufferSwap(void)                                // SWAP BUFFERS
{                                                      // (we don't swap... we blit only)
	static int iOldDX=0;
	static int iOldDY=0;
	long x = PSXDisplay.DisplayPosition.x;
	long y = PSXDisplay.DisplayPosition.y;
	short iDX = PreviousPSXDisplay.DisplayMode.x;
	short iDY = PreviousPSXDisplay.DisplayMode.y;

	if (menuActive) return;


 // TODO: visual rumble

/*
  if(iRumbleTime)
   {
    ScreenRect.left+=((rand()*iRumbleVal)/RAND_MAX)-(iRumbleVal/2);
    ScreenRect.right+=((rand()*iRumbleVal)/RAND_MAX)-(iRumbleVal/2);
    ScreenRect.top+=((rand()*iRumbleVal)/RAND_MAX)-(iRumbleVal/2);
    ScreenRect.bottom+=((rand()*iRumbleVal)/RAND_MAX)-(iRumbleVal/2);
    iRumbleTime--;
   }
*/


	if(iOldDX!=iDX || iOldDY!=iDY)
	{
		memset(GXtexture, 0, GXRESX_MAX*RESY_MAX*2);
		iOldDX=iDX;iOldDY=iDY;
		backFromMenu = 1;
	}

// TODO: Show menu text
	if(ulKeybits&KEY_SHOWFPS) //DisplayText();               // paint menu text
	{
		if(szDebugText[0] && ((time(NULL) - tStart) < 2))
		{
			strcpy(szDispBuf,szDebugText);
		}
		else
		{
			szDebugText[0]=0;
			strcat(szDispBuf,szMenuBuf);
		}
	}


	u8 *imgPtr = (u8*)(psxVuw + ((y<<10) + x));
	if(PSXDisplay.RGB24) {
		long lPitch=iResX_Max<<1;

		if(PreviousPSXDisplay.Range.y0)                       // centering needed?
		{
			imgPtr+=PreviousPSXDisplay.Range.y0*lPitch;
			iDY-=PreviousPSXDisplay.Range.y0;
		}
		imgPtr+=PreviousPSXDisplay.Range.x0<<1;
	}
	

	GX_Flip(iDX, iDY, imgPtr, iResX_Max*2, PSXDisplay.RGB24 ? GX_TF_RGBA8 : GX_TF_RGB5A3);
	
	// Check if TVMode needs to be changed (240 or 480 lines)
	if (originalMode == ORIGINALMODE_ENABLE)
	{
		if(backFromMenu)
		{
			backFromMenu = 0;
			switchToTVMode(iDX, iDY, 0);
		}
	}
}

////////////////////////////////////////////////////////////////////////

void DoClearScreenBuffer(void)                         // CLEAR DX BUFFER
{
	// clear the screen, and DON'T flush it
	DEBUG_print("DoClearScreenBuffer",DBG_GPU1);
}

////////////////////////////////////////////////////////////////////////

void DoClearFrontBuffer(void)                          // CLEAR DX BUFFER
{
	if (menuActive) return;

	// clear the screen, and flush it
	DEBUG_print("DoClearFrontBuffer",DBG_GPU1);
//	printf("DoClearFrontBuffer\n");

	//Write menu/debug text on screen
	if (showFPSonScreen && (ulKeybits&KEY_SHOWFPS))
	{
	    GXColor fontColor = {150,255,150,255};
        IplFont_drawInit(fontColor);
		IplFont_drawString(10,35,szDispBuf, 1.0, false);
		int i = 0;
        DEBUG_update();
        for (i=0;i<DEBUG_TEXT_HEIGHT;i++)
		{
            IplFont_drawString(10,(24*i+60),text[i], 0.5, false);
		}
	}


   //reset swap table from GUI/DEBUG
	GX_SetTevSwapModeTable(GX_TEV_SWAP0, GX_CH_RED, GX_CH_GREEN, GX_CH_BLUE, GX_CH_ALPHA);
	GX_SetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP0);

	GX_DrawDone();

	whichfb ^= 1;
	GX_CopyDisp(xfb[whichfb], GX_TRUE);
	GX_DrawDone();
	VIDEO_SetNextFramebuffer(xfb[whichfb]);
	VIDEO_Flush();
//	VIDEO_WaitVSync();
}


////////////////////////////////////////////////////////////////////////

unsigned long ulInitDisplay(void)
{
	bUsingTWin=FALSE;

	InitMenu();

	bIsFirstFrame = FALSE;                                // done

	if(iShowFPS)
	{
		iShowFPS=0;
		ulKeybits|=KEY_SHOWFPS;
		szDispBuf[0]=0;
		BuildDispMenu(0);
	}

	memset(GXtexture,0,GXRESX_MAX*iResY_Max*2);

	return (unsigned long)GXtexture;		//This isn't right, but didn't want to return 0..
}

////////////////////////////////////////////////////////////////////////

void CloseDisplay(void)
{
}

////////////////////////////////////////////////////////////////////////

void CreatePic(unsigned char * pMem)
{
}

///////////////////////////////////////////////////////////////////////////////////////

void DestroyPic(void)
{
}

///////////////////////////////////////////////////////////////////////////////////////

void DisplayPic(void)
{
}

///////////////////////////////////////////////////////////////////////////////////////

void ShowGpuPic(void)
{
}

///////////////////////////////////////////////////////////////////////////////////////

void ShowTextGpuPic(void)
{
}

////////////////////////////////////////////////////////////////////////
void GX_Flip(short width, short height, u8 * buffer, int pitch, u8 fmt)
{	
	int h, w;
	static int oldwidth=0;
	static int oldheight=0;
	static int oldformat=-1;
	static GXTexObj GXtexobj;

	if((width == 0) || (height == 0))
		return;


	if ((oldwidth != width) || (oldheight != height) || (oldformat != fmt))
	{ //adjust texture conversion
		oldwidth = width;
		oldheight = height;
		oldformat = fmt;
		memset(GXtexture,0,GXRESX_MAX*iResY_Max*2);
		GX_InitTexObj(&GXtexobj, GXtexture, width, height, fmt, GX_CLAMP, GX_CLAMP, GX_FALSE);
	}
	
	if (originalMode == ORIGINALMODE_ENABLE || bilinearFilter == BILINEARFILTER_DISABLE)
		GX_InitTexObjFilterMode(&GXtexobj, GX_NEAR, GX_NEAR);
	else
		GX_InitTexObjFilterMode(&GXtexobj, GX_LINEAR, GX_LINEAR);

	if(PSXDisplay.RGB24) {
		vu8 *wgPipePtr = GX_RedirectWriteGatherPipe(GXtexture);
		u8 *image = (u8 *) buffer;
		for (h = 0; h < (height >> 2); h++)
		{
			for (w = 0; w < (width >> 2); w++)
			{
				// Convert from RGB888 to GX_TF_RGBA8 texel format
				for (int texelY = 0; texelY < 4; texelY++)
				{
					for (int texelX = 0; texelX < 4; texelX++)
					{
						// AR
						int pixelAddr = (((w*4)+(texelX))*3) + (texelY*pitch);
						*wgPipePtr = 0xFF;	// A
						*wgPipePtr = image[pixelAddr+2]; // R (which is B in little endian)
					}
				}
				
				for (int texelY = 0; texelY < 4; texelY++)
				{
					for (int texelX = 0; texelX < 4; texelX++)
					{
						// GB
						int pixelAddr = (((w*4)+(texelX))*3) + (texelY*pitch);
						*wgPipePtr = image[pixelAddr+1]; // G
						*wgPipePtr = image[pixelAddr]; // B (which is R in little endian)
					}
				}
			}
			image+=pitch*4;
		}
	}
	else {
		vu32 *wgPipePtr = GX_RedirectWriteGatherPipe(GXtexture);
		u32 *src1 = (u32 *) buffer;
		u32 *src2 = (u32 *) (buffer + pitch);
		u32 *src3 = (u32 *) (buffer + (pitch * 2));
		u32 *src4 = (u32 *) (buffer + (pitch * 3));
		int rowpitch = (pitch) - (width >> 1);
		u32 alphaMask = 0x00800080;
		for (h = 0; h < height; h += 4)
		{
			for (w = 0; w < (width >> 2); w++)
			{
				// Convert from BGR555 LE data to BGR BE, we OR the first bit with "1" to send RGB5A3 mode into RGB555 on the GP (GC/Wii)
				__asm__ (
					"lwz 3, 0(%1)\n"
					"lwz 4, 4(%1)\n"
					"lwz 5, 0(%2)\n"
					"lwz 6, 4(%2)\n"
					
					"rlwinm 3, 3, 16, 0, 31\n"
					"rlwinm 4, 4, 16, 0, 31\n"
					"rlwinm 5, 5, 16, 0, 31\n"
					"rlwinm 6, 6, 16, 0, 31\n"
					
					"or 3, 3, %5\n"
					"or 4, 4, %5\n"
					"or 5, 5, %5\n"
					"or 6, 6, %5\n"				
					
					"stwbrx 3, 0, %0\n" 
					"stwbrx 4, 0, %0\n" 
					"stwbrx 5, 0, %0\n" 	
					"stwbrx 6, 0, %0\n" 
					
					"lwz 3, 0(%3)\n"
					"lwz 4, 4(%3)\n"
					"lwz 5, 0(%4)\n"
					"lwz 6, 4(%4)\n"
					
					"rlwinm 3, 3, 16, 0, 31\n"
					"rlwinm 4, 4, 16, 0, 31\n"
					"rlwinm 5, 5, 16, 0, 31\n"
					"rlwinm 6, 6, 16, 0, 31\n"
					
					"or 3, 3, %5\n"
					"or 4, 4, %5\n"
					"or 5, 5, %5\n"
					"or 6, 6, %5\n"
					
					"stwbrx 3, 0, %0\n" 
					"stwbrx 4, 0, %0\n" 					
					"stwbrx 5, 0, %0\n" 
					"stwbrx 6, 0, %0" 					
					: : "r" (wgPipePtr), "r" (src1), "r" (src2), "r" (src3), "r" (src4), "r" (alphaMask) : "memory", "r3", "r4", "r5", "r6");
					//         %0             %1            %2          %3          %4           %5     
				src1+=2;
				src2+=2;
				src3+=2;
				src4+=2;
			}

		  src1 += rowpitch;
		  src2 += rowpitch;
		  src3 += rowpitch;
		  src4 += rowpitch;
		}
	}
	GX_RestoreWriteGatherPipe();

	GX_LoadTexObj(&GXtexobj, GX_TEXMAP0);

	Mtx44	GXprojIdent;
	Mtx		GXmodelIdent;
	guMtxIdentity(GXprojIdent);
	guMtxIdentity(GXmodelIdent);
	GXprojIdent[2][2] = 0.5;
	GXprojIdent[2][3] = -0.5;
	GX_LoadProjectionMtx(GXprojIdent, GX_ORTHOGRAPHIC);
	GX_LoadPosMtxImm(GXmodelIdent,GX_PNMTX0);

	GX_SetCullMode(GX_CULL_NONE);
	GX_SetZMode(GX_ENABLE,GX_ALWAYS,GX_TRUE);

	GX_InvalidateTexAll();
	GX_SetTevOp(GX_TEVSTAGE0, GX_REPLACE);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);


	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_PTNMTXIDX, GX_PNMTX0);
	GX_SetVtxDesc(GX_VA_TEX0MTXIDX, GX_IDENTITY);
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);

	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_F32, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

	GX_SetNumTexGens(1);
	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);

	float xcoord = 1.0;
	float ycoord = 1.0;
	if(screenMode == SCREENMODE_16x9_PILLARBOX) xcoord = 640.0/848.0;

	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
	  GX_Position2f32(-xcoord, ycoord);
	  GX_TexCoord2f32( 0.0, 0.0);
	  GX_Position2f32( xcoord, ycoord);
	  GX_TexCoord2f32( 1.0, 0.0);
	  GX_Position2f32( xcoord,-ycoord);
	  GX_TexCoord2f32( 1.0, 1.0);
	  GX_Position2f32(-xcoord,-ycoord);
	  GX_TexCoord2f32( 0.0, 1.0);
	GX_End();

	//Write menu/debug text on screen
	if (showFPSonScreen && (ulKeybits&KEY_SHOWFPS))
    {
        GXColor fontColor = {150,255,150,255};
	    IplFont_drawInit(fontColor);
		IplFont_drawString(10,35,szDispBuf, 1.0, false);
		int i = 0;
        DEBUG_update();
        for (i=0;i<DEBUG_TEXT_HEIGHT;i++)
		{
            IplFont_drawString(10,(24*i+60),text[i], 0.5, false);
		}
    }

   //reset swap table from GUI/DEBUG
	GX_SetTevSwapModeTable(GX_TEV_SWAP0, GX_CH_BLUE, GX_CH_GREEN, GX_CH_RED ,GX_CH_ALPHA);
	GX_SetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP0);

	GX_DrawDone();

	whichfb ^= 1;
	GX_CopyDisp(xfb[whichfb], GX_TRUE);
	GX_DrawDone();
//	printf("Prv.Rng.x0,x1,y0 = %d, %d, %d, Prv.Mode.y = %d,DispPos.x,y = %d, %d, RGB24 = %x\n",PreviousPSXDisplay.Range.x0,PreviousPSXDisplay.Range.x1,PreviousPSXDisplay.Range.y0,PreviousPSXDisplay.DisplayMode.y,PSXDisplay.DisplayPosition.x,PSXDisplay.DisplayPosition.y,PSXDisplay.RGB24);
	VIDEO_SetNextFramebuffer(xfb[whichfb]);
	VIDEO_Flush();
// VIDEO_WaitVSync();
}

////////////////////////////////////////////////////////////////////////

/* TODO - add this function
void ShowGunCursor(unsigned char * surf,int iPitch)
{
 unsigned short dx=(unsigned short)PreviousPSXDisplay.Range.x1;
 unsigned short dy=(unsigned short)PreviousPSXDisplay.DisplayMode.y;
 int x,y,iPlayer,sx,ex,sy,ey;

 if(iColDepth==32) iPitch=iPitch<<2;
 else              iPitch=iPitch<<1;

 if(PreviousPSXDisplay.Range.y0)                       // centering needed?
  {
   surf+=PreviousPSXDisplay.Range.y0*iPitch;
   dy-=PreviousPSXDisplay.Range.y0;
  }

 if(iColDepth==32)                                     // 32 bit color depth
  {
   const unsigned long crCursorColor32[8]={0xffff0000,0xff00ff00,0xff0000ff,0xffff00ff,0xffffff00,0xff00ffff,0xffffffff,0xff7f7f7f};

   surf+=PreviousPSXDisplay.Range.x0<<2;               // -> add x left border

   for(iPlayer=0;iPlayer<8;iPlayer++)                  // -> loop all possible players
    {
     if(usCursorActive&(1<<iPlayer))                   // -> player active?
      {
       const int ty=(ptCursorPoint[iPlayer].y*dy)/256;  // -> calculate the cursor pos in the current display
       const int tx=(ptCursorPoint[iPlayer].x*dx)/512;
       sx=tx-5;if(sx<0) {if(sx&1) sx=1; else sx=0;}
       sy=ty-5;if(sy<0) {if(sy&1) sy=1; else sy=0;}
       ex=tx+6;if(ex>dx) ex=dx;
       ey=ty+6;if(ey>dy) ey=dy;

       for(x=tx,y=sy;y<ey;y+=2)                        // -> do dotted y line
        *((unsigned long *)((surf)+(y*iPitch)+x*4))=crCursorColor32[iPlayer];
       for(y=ty,x=sx;x<ex;x+=2)                        // -> do dotted x line
        *((unsigned long *)((surf)+(y*iPitch)+x*4))=crCursorColor32[iPlayer];
      }
    }
  }
 else                                                  // 16 bit color depth
  {
   const unsigned short crCursorColor16[8]={0xf800,0x07c0,0x001f,0xf81f,0xffc0,0x07ff,0xffff,0x7bdf};

   surf+=PreviousPSXDisplay.Range.x0<<1;               // -> same stuff as above

   for(iPlayer=0;iPlayer<8;iPlayer++)
    {
     if(usCursorActive&(1<<iPlayer))
      {
       const int ty=(ptCursorPoint[iPlayer].y*dy)/256;
       const int tx=(ptCursorPoint[iPlayer].x*dx)/512;
       sx=tx-5;if(sx<0) {if(sx&1) sx=1; else sx=0;}
       sy=ty-5;if(sy<0) {if(sy&1) sy=1; else sy=0;}
       ex=tx+6;if(ex>dx) ex=dx;
       ey=ty+6;if(ey>dy) ey=dy;

       for(x=tx,y=sy;y<ey;y+=2)
        *((unsigned short *)((surf)+(y*iPitch)+x*2))=crCursorColor16[iPlayer];
       for(y=ty,x=sx;x<ex;x+=2)
        *((unsigned short *)((surf)+(y*iPitch)+x*2))=crCursorColor16[iPlayer];
      }
    }
  }
}
*/
