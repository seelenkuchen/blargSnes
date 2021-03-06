/*
    Copyright 2014 StapleButter

    This file is part of blargSnes.

    blargSnes is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    blargSnes is distributed in the hope that it will be useful, but WITHOUT ANY 
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS 
    FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along 
    with blargSnes. If not, see http://www.gnu.org/licenses/.
*/

//#define REPORT_STATS

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <3ds.h>

#include "main.h"
#include "config.h"

#include "blargGL.h"
#include "ui.h"
#include "audio.h"
#include "mixrate.h"

#include "mem.h"
#include "cpu.h"
#include "spc700.h"
#include "ppu.h"
#include "snes.h"
#include "dsp.h"

#include "defaultborder.h"
#include "screenfill.h"

#include "final_shbin.h"
#include "render_soft_shbin.h"
#include "render_hard_shbin.h"
#include "render_hard7_shbin.h"
#include "plain_quad_shbin.h"
#include "window_mask_shbin.h"

#include "saves.h"


#include "version.h"

u64 emuTick;
u64 emuTime;
u64 ppuTick;
u64 ppuTime;

u64 gpuTick;
u64 gpuTime;
u32 gpuFPS;

u32 reportFrame;


u32* gpuOut;
u32* SNESFrame;

DVLB_s* finalShader;
DVLB_s* softRenderShader;
DVLB_s* hardRenderShader;
DVLB_s* hard7RenderShader;
DVLB_s* plainQuadShader;
DVLB_s* windowMaskShader;

shaderProgram_s finalShaderP;
shaderProgram_s softRenderShaderP;
shaderProgram_s hardRenderShaderP;
shaderProgram_s hard7RenderShaderP;
shaderProgram_s plainQuadShaderP;
shaderProgram_s windowMaskShaderP;

u8 finalUniforms[1];
u8 softRenderUniforms[1];
u8 hardRenderUniforms[2];
u8 hard7RenderUniforms[3];

u8 plainQuadUniforms[1];
u8 windowMaskUniforms[1];

void* vertexBuf;
void* vertexPtr;

int GPUState = 0;
DVLB_s* CurShader = NULL;

u32* BorderTex;
u16* MainScreenTex;
u16* SubScreenTex;


int forceexit = 0;
int running = 0;
int pause = 0;
int reset = 0;
u32 framecount = 0;

u8 RenderState = 0;
int FramesSkipped = 0;
bool SkipThisFrame = false;
u64 LastVBlank = 0;

// debug
u32 ntriangles = 0;

// hax
extern Handle gspEventThread;
extern Handle gspEvents[GSPGPU_EVENT_MAX];


#define SPC_THREAD_STACK_SIZE 0x4000
Thread spcthread = NULL;
Handle SPCSync;

int exitspc = 0;

bool AudioEnabled = false;


void reportStats()
{
	if(running && !pause)
	{
		reportFrame++;
		if(reportFrame == 60)
		{
		//	// bprintf("%d\n", vramSpaceFree());
		//	// bprintf("E: %.2f, P: %.2f, G: %.2f, FPS: %02d\n", ((double)(emuTime / 60) / 4468724.0) * 100, ((double)(ppuTime / 60) / 4468724.0) * 100, ((double)(gpuTime / gpuFPS) / 4468724.0) * 100, gpuFPS);
		//	// bprintf(" \n");
			reportFrame = 0;
			emuTime = 0;
			ppuTime = 0;
			gpuTime = 0;
			gpuFPS = 0;
		}
	}
}




// TODO: correction
// mixes 127995 samples every 4 seconds, instead of 128000 (128038.1356)
// +43 samples every 4 seconds
// +1 sample every 32 second

void SPCThread(void *arg)
{

	int audCnt = 512;
	u32 lastpos = 0;


	int i;
	while (!exitspc)
	{
		svcWaitSynchronization(SPCSync, U64_MAX);
		svcClearEvent(SPCSync);
		
		if (!pause)
		{
			bool started = Audio_Begin();
			if(started)
			{
				audCnt = 512;
				lastpos = 0;
			}
			u32 curpos = ndspChnGetSamplePos(0);

			Audio_Mix(audCnt, started);

			s32 diff = curpos - lastpos;
			if(diff < 0) diff += MIXBUFSIZE;
			lastpos = curpos;

			audCnt = diff;
		}
		else
			Audio_Pause();
	}

	threadExit(0);
}



void dbg_save(char* path, void* buf, int size)
{
	FILE * pFile = fopen(path, "wb");
	if(pFile != NULL)
	{
		fwrite(buf, sizeof(char), size, pFile);
		fclose(pFile);
	}
}

void debugcrapo(u32 op, u32 op2)
{
//	// bprintf("DBG: %08X %08X\n", op, op2);
	DrawConsole();
	//SwapBottomBuffers(0);
	//ClearBottomBuffer();
}

void SPC_ReportUnk(u8 op, u32 pc)
{
	static bool unkreported = false;
	if (unkreported) return;
	unkreported = true;
 //	// bprintf("SPC UNK %02X @ %04X\n", op, pc);
 }	

void ReportCrash()
{
	pause = 1;
	running = 0;
	
	ClearConsole();
//	// bprintf("Game has crashed (STOP)\n");
	
	extern u32 debugpc;
	// // bprintf("PC: %02X:%04X (%06X)\n", CPU_Regs.PBR, CPU_Regs.PC, debugpc);
	// // bprintf("P: %02X | M=%d X=%d E=%d\n", CPU_Regs.P.val&0xFF, CPU_Regs.P.M, CPU_Regs.P.X, CPU_Regs.P.E);
	// // bprintf("A: %04X X: %04X Y: %04X\n", CPU_Regs.A, CPU_Regs.X, CPU_Regs.Y);
	// // bprintf("S: %04X D: %02X DBR: %02X\n", CPU_Regs.S, CPU_Regs.D, CPU_Regs.DBR);
	
	// // // bprintf("Stack\n");
	// // // bprintf("%02X %02X %02X %02X %02X %02X %02X %02X\n",
		// SNES_SysRAM[CPU_Regs.S+0], SNES_SysRAM[CPU_Regs.S+1],
		// SNES_SysRAM[CPU_Regs.S+2], SNES_SysRAM[CPU_Regs.S+3],
		// SNES_SysRAM[CPU_Regs.S+4], SNES_SysRAM[CPU_Regs.S+5],
		// SNES_SysRAM[CPU_Regs.S+6], SNES_SysRAM[CPU_Regs.S+7]);
	CPU_Regs.S += 8;
	// // // bprintf("%02X %02X %02X %02X %02X %02X %02X %02X\n",
		// SNES_SysRAM[CPU_Regs.S+0], SNES_SysRAM[CPU_Regs.S+1],
		// SNES_SysRAM[CPU_Regs.S+2], SNES_SysRAM[CPU_Regs.S+3],
		// SNES_SysRAM[CPU_Regs.S+4], SNES_SysRAM[CPU_Regs.S+5],
		// SNES_SysRAM[CPU_Regs.S+6], SNES_SysRAM[CPU_Regs.S+7]);
	CPU_Regs.S += 8;
	// // // bprintf("%02X %02X %02X %02X %02X %02X %02X %02X\n",
		// SNES_SysRAM[CPU_Regs.S+0], SNES_SysRAM[CPU_Regs.S+1],
		// SNES_SysRAM[CPU_Regs.S+2], SNES_SysRAM[CPU_Regs.S+3],
		// SNES_SysRAM[CPU_Regs.S+4], SNES_SysRAM[CPU_Regs.S+5],
		// SNES_SysRAM[CPU_Regs.S+6], SNES_SysRAM[CPU_Regs.S+7]);
	CPU_Regs.S += 8;
	// // // bprintf("%02X %02X %02X %02X %02X %02X %02X %02X\n",
		// SNES_SysRAM[CPU_Regs.S+0], SNES_SysRAM[CPU_Regs.S+1],
		// SNES_SysRAM[CPU_Regs.S+2], SNES_SysRAM[CPU_Regs.S+3],
		// SNES_SysRAM[CPU_Regs.S+4], SNES_SysRAM[CPU_Regs.S+5],
		// SNES_SysRAM[CPU_Regs.S+6], SNES_SysRAM[CPU_Regs.S+7]);
		
	// // bprintf("Full RAM dump can be found on SD\n");
	
	u32 pc = (CPU_Regs.PBR<<16)|CPU_Regs.PC;
	u32 ptr = Mem_PtrTable[pc >> 13];
	// // bprintf("Ptr table entry: %08X\n", ptr);
	
	// // bprintf("Tell StapleButter\n");
	
	dbg_save("/SNESRAM.bin", SNES_SysRAM, 0x20000);
	dbg_save("/SNESPtrChunk.bin", (void*)(ptr&~0xF), 0x2000);
}

void dbgcolor(u32 col)
{
	u32 regData=0x01000000|col;
	GSPGPU_WriteHWRegs(0x202204, &regData, 4);
}



float screenProjMatrix[16] = 
{
	2.0f/240.0f, 0, 0, -1,
	0, 2.0f/400.0f, 0, -1,
	0, 0, 1, -1,
	0, 0, 0, 1
};

float snesProjMatrix[16] = 
{
	2.0f/256.0f, 0, 0, -1,
	0, 2.0f/256.0f, 0, -1,
	0, 0, 1.0f/128.0f, -1,
	0, 0, 0, 1
};

float mode7ProjMatrix[16] = 
{
	2.0f/512.0f, 0, 0, -1,
	0, 2.0f/512.0f, 0, -1,
	0, 0, 1, -1,
	0, 0, 0, 1
};

float vertexList[] = 
{
	// border
	0.0, 0.0, 0.9,      0.78125, 0.0625,
	240.0, 400.0, 0.9,  0.0, 1.0,
	
	// screen
	8.0, 72.0, 0.9,     1.0, 0.87890625,
	232.0, 328.0, 0.9,  0.0, 0.00390625,
};
float* borderVertices;
float* screenVertices;


void ApplyScaling()
{
	float texy = (float)(SNES_Status->ScreenHeight+1) / 256.0f;
	
	float x1, x2, y1, y2;
	
	int scalemode = Config.ScaleMode;
	if (!running && scalemode == 2) scalemode = 1;
	else if (!running && scalemode == 4) scalemode = 3;

	
	switch (scalemode)
	{
		case 1: // fullscreen
			x1 = 0.0f; x2 = 240.0f;
			y1 = 0.0f; y2 = 400.0f;
			break;
			
		case 2: // cropped
			{
				float bigy = ((float)SNES_Status->ScreenHeight * 240.0f) / (float)(SNES_Status->ScreenHeight-16);
				float margin = (bigy - 240.0f) / 2.0f;
				x1 = -margin; x2 = 240.0f+margin;
				y1 = 0.0f; y2 = 400.0f;
			}
			break;
			
		case 3: // 4:3
			x1 = 0.0f; x2 = 240.0f;
			y1 = 40.0f; y2 = 360.0f;
			break;
			
		case 4: // cropped 4:3
			{
				float bigy = ((float)SNES_Status->ScreenHeight * 240.0f) / (float)(SNES_Status->ScreenHeight-16);
				float margin = (bigy - 240.0f) / 2.0f;
				x1 = -margin; x2 = 240.0f+margin;
				y1 = 29.0f; y2 = 371.0f;
			}
			break;
			
		default: // 1:1
			if (SNES_Status->ScreenHeight == 239)
			{
				x1 = 1.0f; x2 = 240.0f;
			}
			else
			{
				x1 = 8.0f; x2 = 232.0f;
			}
			y1 = 72.0f; y2 = 328.0f;
			break;
	}
	
	screenVertices[5*0 + 0] = x1; screenVertices[5*0 + 1] = y1; screenVertices[5*0 + 4] = texy; 
	screenVertices[5*1 + 0] = x2; screenVertices[5*1 + 1] = y2; 
	
	GSPGPU_FlushDataCache((u32*)screenVertices, 5*2*sizeof(float));

}


void VSyncAndFrameskip();

bool PeekEvent(Handle evt)
{
	// do a wait that returns immediately.
	// if we get a timeout error code, the event didn't occur
	Result res = svcWaitSynchronization(evt, 0);
	if (!res)
	{
		svcClearEvent(evt);
		return true;
	}
	
	return false;
}

void SafeWait(Handle evt)
{
	// sometimes, we end up waiting for a given event, but for whatever reason the associated action failed to start
	// and we end up 'freezing'
	// this method of waiting avoids that
	// it's dirty and doesn't solve the actual issue but atleast it avoids a freeze
	
	Result res = svcWaitSynchronization(evt, 40*1000*1000);
	if (!res)
		svcClearEvent(evt);
}
//u64 baderp;
void RenderTopScreen()
{
	bglUseShader(&finalShaderP);

	bglOutputBuffers(0x2, 0x3, gpuOut, NULL, 240, 400);
	bglViewport(0, 0, 240, 400);

	bglOutputBufferAccess(0, 1, 0, 0);
	
	bglEnableDepthTest(false);
	bglColorDepthMask(GPU_WRITE_COLOR);
	
	bglEnableTextures(GPU_TEXUNIT0);
	
	bglTexEnv(0, 
		GPU_TEVSOURCES(GPU_TEXTURE0, 0, 0), 
		GPU_TEVSOURCES(GPU_TEXTURE0, 0, 0),
		GPU_TEVOPERANDS(0,0,0), 
		GPU_TEVOPERANDS(0,0,0), 
		GPU_REPLACE, GPU_REPLACE, 
		0xFFFFFFFF);
	bglDummyTexEnv(1);
	bglDummyTexEnv(2);
	bglDummyTexEnv(3);
	bglDummyTexEnv(4);
	bglDummyTexEnv(5);
	
	bglTexImage(GPU_TEXUNIT0, BorderTex,512,256,0,GPU_RGBA8);
	
	bglUniformMatrix(GPU_VERTEX_SHADER, finalUniforms[0], screenProjMatrix);
	
	bglNumAttribs(2);
	bglAttribType(0, GPU_FLOAT, 3);	// vertex
	bglAttribType(1, GPU_FLOAT, 2);	// texcoord
	bglAttribBuffer(borderVertices);
	
	bglDrawArrays(GPU_GEOMETRY_PRIM, 2); // border

	// filtering enabled only when scaling
	// filtering at 1:1 causes output to not be pixel-perfect, but not filtering at higher res looks like total shit
	bglTexImage(GPU_TEXUNIT0, SNESFrame, 256,256, Config.ScaleMode?0x6:0 ,GPU_RGBA8);
	
	bglAttribBuffer(screenVertices);
	
	bglDrawArrays(GPU_GEOMETRY_PRIM, 2); // screen

	if (!RenderState)
	{
		//baderp = svcGetSystemTick();

		bglFlush();
		gpuTick = svcGetSystemTick();
		RenderState = 1;
				
		/*SafeWait(gspEvents[GSPEVENT_P3D]);
		{u64 darp = svcGetSystemTick() - baderp;// bprintf("GPU: %f\n", (float)darp/268123.480);}
		GX_DisplayTransfer(gpuOut, 0x019000F0, (u32*)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL), 0x019000F0, 0x00001000);
		RenderState = 2;*/
	}
}

void ContinueRendering()
{
	switch (RenderState)
	{
		case 0: return;
		
		case 3:
			if (PeekEvent(gspEvents[GSPGPU_EVENT_PPF]))
			{
				bglFlush();
				RenderState = 1;

				gpuTick = svcGetSystemTick();
			}
			break;
			
		case 1:
			if (PeekEvent(gspEvents[GSPGPU_EVENT_P3D]))
			{
				gpuTime += svcGetSystemTick() - gpuTick;
				gpuFPS++;

				//{u64 darp = svcGetSystemTick() - baderp;// bprintf("GPU: %f\n", (float)darp/268123.480);}
				GX_DisplayTransfer(gpuOut, 0x019000F0, (u32*)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL), 0x019000F0, 0x00001000);
				RenderState = 2;
			}
			//baderp = svcGetSystemTick();
			break;
			
		case 2:
			if (PeekEvent(gspEvents[GSPGPU_EVENT_PPF]))
			{
				RenderState = 0;
				VSyncAndFrameskip();
			}
			break;
	}
}

void FinishRendering()
{
	if (RenderState == 3)
	{
		//gspWaitForPPF();
		SafeWait(gspEvents[GSPGPU_EVENT_PPF]);
		bglFlush();
		RenderState = 1;

		gpuTick = svcGetSystemTick();
	}
	if (RenderState == 1)
	{
		//gspWaitForP3D();
		SafeWait(gspEvents[GSPGPU_EVENT_P3D]);

		gpuTime += svcGetSystemTick() - gpuTick;
		gpuFPS++;

		//{u64 darp = svcGetSystemTick() - baderp;// bprintf("GPU: %f\n", (float)darp/268123.480);}
		GX_DisplayTransfer(gpuOut, 0x019000F0, (u32*)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL), 0x019000F0, 0x00001000);
		RenderState = 2;
	}
	//baderp = svcGetSystemTick();
	if (RenderState == 2)
	{
		//gspWaitForPPF();
		SafeWait(gspEvents[GSPGPU_EVENT_PPF]);
		VSyncAndFrameskip();
	}
	if (RenderState == 4)
	{
		VSyncAndFrameskip();
	}
	
	RenderState = 0;
}

u32 PALCount = 0;

void VSyncAndFrameskip()
{
	if (running && !pause && PeekEvent(gspEvents[GSPGPU_EVENT_VBlank0]) && FramesSkipped<5)
	{
		// we missed the VBlank
		// skip the next frames to compensate
		
		// TODO: doesn't work
		/*s64 time = (s64)(svcGetSystemTick() - LastVBlank);
		while (time > 4468724)
		{
			FramesSkipped++;
			time -= 4468724;
		}*/
		
		SkipThisFrame = true;
		FramesSkipped++;
	}
	else
	{
		SkipThisFrame = false;
		FramesSkipped = 0;
		
		{
			u8* bottomfb = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);
			
			UI_SetFramebuffer(bottomfb);
			UI_Render();
			GSPGPU_FlushDataCache(bottomfb, 0x38400);
		}
		
		gfxSwapBuffersGpu();
		gspWaitForEvent(GSPGPU_EVENT_VBlank0, false);
		//LastVBlank = svcGetSystemTick();
	}
	
	// in PAL mode, wait one extra frame every 5 frames to slow down to 50FPS
	if (running && !pause && ROM_Region)
	{
		PALCount++;
		if (PALCount >= 5)
		{
			PALCount = 0;
			gspWaitForVBlank();
		}
	}
}


bool TakeScreenshot(char* path)
{
	int x, y;
	
	FILE *pFile = fopen(path, "wb");
	if(pFile == NULL)
		return false;
	
	u32 bitmapsize = 400*480*3;
	u8* tempbuf = (u8*)linearAlloc(0x36 + bitmapsize);
	memset(tempbuf, 0, 0x36 + bitmapsize);

	
	*(u16*)&tempbuf[0x0] = 0x4D42;
	*(u32*)&tempbuf[0x2] = 0x36 + bitmapsize;
	*(u32*)&tempbuf[0xA] = 0x36;
	*(u32*)&tempbuf[0xE] = 0x28;
	*(u32*)&tempbuf[0x12] = 400; // width
	*(u32*)&tempbuf[0x16] = 480; // height
	*(u32*)&tempbuf[0x1A] = 0x00180001;
	*(u32*)&tempbuf[0x22] = bitmapsize;
	
	u8* framebuf = (u8*)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
	for (y = 0; y < 240; y++)
	{
		for (x = 0; x < 400; x++)
		{
			int si = ((239 - y) + (x * 240)) * 3;
			int di = 0x36 + (x + ((479 - y) * 400)) * 3;
			
			tempbuf[di++] = framebuf[si++];
			tempbuf[di++] = framebuf[si++];
			tempbuf[di++] = framebuf[si++];
		}
	}
	
	framebuf = (u8*)gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);
	for (y = 0; y < 240; y++)
	{
		for (x = 0; x < 320; x++)
		{
			int si = ((239 - y) + (x * 240)) * 3;
			int di = 0x36 + ((x+40) + ((239 - y) * 400)) * 3;
			
			tempbuf[di++] = framebuf[si++];
			tempbuf[di++] = framebuf[si++];
			tempbuf[di++] = framebuf[si++];
		}
	}

	fwrite(tempbuf, sizeof(char), 0x36 + bitmapsize, pFile);
	fclose(pFile);

	linearFree(tempbuf);
	return true;
}


// flags: bit0=tiled, bit1=15bit color
void CopyBitmapToTexture(u8* src, void* dst, u32 width, u32 height, u32 alpha, u32 startx, u32 stride, u32 flags)
{
	int x, y;
	for (y = height-1; y >= 0; y--)
	{
		for (x = startx; x < startx+width; x++)
		{
			u8 b = *src++;
			u8 g = *src++;
			u8 r = *src++;
			
			int di;
			if (flags & 0x1)
			{
				di  = x & 0x1;
				di += (y & 0x1) << 1;
				di += (x & 0x2) << 1;
				di += (y & 0x2) << 2;
				di += (x & 0x4) << 2;
				di += (y & 0x4) << 3;
				di += (x & 0x1F8) << 3;
				di += ((y & 0xF8) << 3) * stride;
			}
			else
				di = x + (y * stride * 8);
			
			if (flags & 0x2)
				((u16*)dst)[di] = (alpha ? 1:0) | ((b & 0xF8) >> 2) | ((g & 0xF8) << 3) | ((r & 0xF8) << 8);
			else
				((u32*)dst)[di] = alpha | (b << 8) | (g << 16) | (r << 24);
		}
	}
}

bool LoadBitmap(char* path, u32 width, u32 height, void* dst, u32 alpha, u32 startx, u32 stride, u32 flags)
{
	u8 header[0x1E];
	FILE *pFile = fopen(path, "rb");
	if(pFile == NULL)
		return false;

	fread(header, sizeof(char), 0x1E, pFile);
	if((*(u16*)&header[0] != 0x4D42) || (*(u32*)&header[0x12] != width) || (*(u32*)&header[0x16] != height) || (*(u16*)&header[0x1A] != 1) || (*(u16*)&header[0x1C] != 24))
	{
		fclose(pFile);
		return false;
	}

	
	u32 bufsize = width*height*3;
	u8* buf = (u8*)linearMemAlign(bufsize, 0x10);
	
    fseek(pFile, 0x36, SEEK_SET);
	fread(buf, sizeof(char), bufsize, pFile);
	fclose(pFile);

	CopyBitmapToTexture(buf, dst, width, height, alpha, startx, stride, flags);
	
	linearFree(buf);
	return true;
}

bool LoadBorder(char* path)
{
	return LoadBitmap(path, 400, 240, BorderTex, 0xFF, 0, 64, 0x1);
}


bool StartROM()
{
	if (spcthread)
	{
		exitspc = 1; pause = 1;
		svcSignalEvent(SPCSync);
		svcWaitSynchronization(spcthread, U64_MAX);
		threadJoin(spcthread, U64_MAX);
		exitspc = 0;
	}
	
	running = 1;
	pause = 0;
    reset = 0;
	framecount = 0;
	
	ClearConsole();
//	// bprintf("blargSNES %s\n", BLARGSNES_VERSION);
//	// bprintf("http://blargsnes.kuribo64.net/\n");

	if(!AudioEnabled)
        bprintf("NDSP cannot initialize.\n");
	
//	// bprintf("Loading %s...\n", path);
	

	if (!SNES_LoadROM())
		return false;

	SaveConfig(1);
	
	CPU_Reset();
	SPC_Reset();

	RenderState = 0;
	FramesSkipped = 0;
	SkipThisFrame = false;
	PALCount = 0;

	
	// SPC700 thread (running on syscore)

	spcthread = threadCreate(SPCThread, 0x0, SPC_THREAD_STACK_SIZE, 0x18, 1, true);
	if (!spcthread) 
	{
        bprintf("Failed to create SPC700 thread:\n");
	}
	
//	// bprintf("ROM loaded, running...\n");
	
	return true;
}



int reported=0;extern u32 debugpc;
u32 oldshiz=0;
void reportshit(u32 pc, u32 a, u32 y)
{
	/*if (*(u32*)&SNES_SysRAM[0x300] != 0xEFEFEFEF && oldshiz==0xEFEFEFEF)
	{
		if (reported) return; reported=1;
		// bprintf("%06X A=%04X %04X\n", pc, a, *(u32*)&SNES_SysRAM[0x300]);
	}
	oldshiz = *(u32*)&SNES_SysRAM[0x300];*/
	//// bprintf("!! IRQ %04X %02X\n", SNES_Status->IRQ_CurHMatch, SNES_Status->IRQCond);
	//pause=1;
	//// bprintf("TSX S=%04X X=%04X P=%04X  %04X\n", pc>>16, a, y&0xFFFF, y>>16);
	if (reported) return;
	//// bprintf("!! %08X -> %08X | %08X\n", pc, a, y);
	// bprintf("!! A=%02X X=%02X Y=%02X\n", pc, a, y);
	reported=1;
	//running=0; pause=1;
}

int reported2=0;
void reportshit2(u32 pc, u32 a, u32 y)
{
	//// bprintf("TSC S=%04X A=%04X P=%04X  %04X\n", pc>>16, a, y&0xFFFF, y>>16);
	if (SNES_SysRAM[0x3C8] == 0 && reported2 != 0)
		bprintf("[%06X] 3C8=0\n", debugpc);
	reported2 = SNES_SysRAM[0x3C8];
}

void derpreport()
{
	// bprintf("passed NMI check\n");
}

static aptHookCookie apt_hook_sleepsuspend;

static void apt_sleepsuspend_hook(APT_HookType hook, void *param)
{
	if(hook == APTHOOK_ONSLEEP || hook == APTHOOK_ONSUSPEND)
	{
		int * running = (int*)param;
		if (*running) SNES_SaveSRAM();

		// Should we do this? Ctrulib has matured quite a bit, so I dunno
		svcSignalEvent(SPCSync);
		FinishRendering();
		bglDeInit();
	}
	else if(hook == APTHOOK_ONWAKEUP || hook == APTHOOK_ONRESTORE)
	{
		bglInit();
		FinishRendering();
	}
}

int main() 
{
	int i;
	int shot = 0;
	
	touchPosition lastTouch;
	u32 repeatkeys = 0;
	int repeatstate = 0;
	int repeatcount = 0;
	
	forceexit = 0;
	running = 0;
	pause = 0;
    reset = 0;
	exitspc = 0;
	
	ClearConsole();
	
	// Enable 804Mhz mode on New 3DS
	osSetSpeedupEnable(true);

	aptOpenSession();
	APT_SetAppCpuTimeLimit(30); // enables syscore usage
	aptCloseSession();

	gfxInitDefault();
    romfsInit();
    
    char folderPath[] = "/snes";
    mkdir(folderPath, 0777);

	Config.HardwareMode7Filter = -1;
	LoadConfig(1);
	
	VRAM_Init();
	SNES_Init();
	PPU_Init();
	
	//GPU_Init(NULL);
	gfxSet3D(false);
	bglInit();
	RenderState = 0;
	
	vertexBuf = linearAlloc(0x80000 * 4);
	vertexPtr = vertexBuf;
	
	svcSetThreadPriority(gspEventThread, 0x30);
	
	gpuOut = (u32*)vramAlloc(400*240*2*4);
	SNESFrame = (u32*)vramAlloc(256*256*4);

	finalShader = DVLB_ParseFile((u32*)final_shbin, final_shbin_size);
	softRenderShader = DVLB_ParseFile((u32*)render_soft_shbin, render_soft_shbin_size);
	hardRenderShader = DVLB_ParseFile((u32*)render_hard_shbin, render_hard_shbin_size);
	hard7RenderShader = DVLB_ParseFile((u32*)render_hard7_shbin, render_hard7_shbin_size);
	plainQuadShader = DVLB_ParseFile((u32*)plain_quad_shbin, plain_quad_shbin_size);
	windowMaskShader = DVLB_ParseFile((u32*)window_mask_shbin, window_mask_shbin_size);

	shaderProgramInit(&finalShaderP);		shaderProgramSetVsh(&finalShaderP, &finalShader->DVLE[0]);				shaderProgramSetGsh(&finalShaderP, &finalShader->DVLE[1], 4);
	shaderProgramInit(&softRenderShaderP);	shaderProgramSetVsh(&softRenderShaderP, &softRenderShader->DVLE[0]);	shaderProgramSetGsh(&softRenderShaderP, &softRenderShader->DVLE[1], 4);
	shaderProgramInit(&hardRenderShaderP);	shaderProgramSetVsh(&hardRenderShaderP, &hardRenderShader->DVLE[0]);	shaderProgramSetGsh(&hardRenderShaderP, &hardRenderShader->DVLE[1], 4);
	shaderProgramInit(&hard7RenderShaderP);	shaderProgramSetVsh(&hard7RenderShaderP, &hard7RenderShader->DVLE[0]);	shaderProgramSetGsh(&hard7RenderShaderP, &hard7RenderShader->DVLE[1], 4);
	shaderProgramInit(&plainQuadShaderP);	shaderProgramSetVsh(&plainQuadShaderP, &plainQuadShader->DVLE[0]);		shaderProgramSetGsh(&plainQuadShaderP, &plainQuadShader->DVLE[1], 4);
	shaderProgramInit(&windowMaskShaderP);	shaderProgramSetVsh(&windowMaskShaderP, &windowMaskShader->DVLE[0]);	shaderProgramSetGsh(&windowMaskShaderP, &windowMaskShader->DVLE[1], 4);

	finalUniforms[0] = shaderInstanceGetUniformLocation(finalShaderP.vertexShader, "projMtx");

	softRenderUniforms[0] = shaderInstanceGetUniformLocation(softRenderShaderP.vertexShader, "projMtx");

	hardRenderUniforms[0] = shaderInstanceGetUniformLocation(hardRenderShaderP.vertexShader, "projMtx");
	hardRenderUniforms[1] = shaderInstanceGetUniformLocation(hardRenderShaderP.vertexShader, "scaler");

	hard7RenderUniforms[0] = shaderInstanceGetUniformLocation(hard7RenderShaderP.vertexShader, "scaler");
	hard7RenderUniforms[1] = shaderInstanceGetUniformLocation(hard7RenderShaderP.geometryShader, "projMtx");
	hard7RenderUniforms[2] = shaderInstanceGetUniformLocation(hard7RenderShaderP.geometryShader, "mode7Type");

	plainQuadUniforms[0] = shaderInstanceGetUniformLocation(plainQuadShaderP.vertexShader, "projMtx");

	windowMaskUniforms[0] = shaderInstanceGetUniformLocation(windowMaskShaderP.vertexShader, "projMtx");

	GX_MemoryFill(gpuOut, 0x404040FF, &gpuOut[0x2EE00], 0x201, NULL, 0, NULL, 0);
	gspWaitForPSC0();
	gfxSwapBuffersGpu();
	
	UI_SetFramebuffer(gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL));
	//ClearConsole();
	
	BorderTex = (u32*)linearAlloc(512*256*4);
	
	// copy some fixed vertices to linear memory
	borderVertices = (float*)linearAlloc(5*2 * sizeof(float));
	screenVertices = (float*)linearAlloc(5*2 * sizeof(float));
	
	float* fptr = &vertexList[0];
	for (i = 0; i < 5*2; i++) borderVertices[i] = *fptr++;
	for (i = 0; i < 5*2; i++) screenVertices[i] = *fptr++;
	ApplyScaling();
	
	
	// load border
	if ((!LoadBorder("romfs:/blargSnesBorder.bmp")) && (!LoadBorder("/blargSnesBorder.bmp")))
		CopyBitmapToTexture(defaultborder, BorderTex, 400, 240, 0xFF, 0, 64, 0x1);

	// copy splashscreen
	u32* tempbuf = (u32*)linearAlloc(256*256*4);
	CopyBitmapToTexture(screenfill, tempbuf, 256, 224, 0xFF, 0, 32, 0x0);
	GSPGPU_FlushDataCache(tempbuf, 256*256*4);

	GX_DisplayTransfer(tempbuf, 0x01000100, (u32*)SNESFrame, 0x01000100, 0x3);
	//gspWaitForPPF();

	SafeWait(gspEvents[GSPGPU_EVENT_PPF]);
	linearFree(tempbuf);
	
	AudioEnabled = Audio_Init();
	svcCreateEvent(&SPCSync, 0); 

    UI_Switch(&UI_Console);

    if (!StartROM()) forceexit = 1;

//	UI_Switch(&UI_ROMMenu);
	
	// Create a hook for when the system either goes into sleep mode or is suspended, so it'll do things.
	aptHook(&apt_hook_sleepsuspend, apt_sleepsuspend_hook, &running);

	while(!forceexit && aptMainLoop())
	{
		hidScanInput();
		u32 press = hidKeysDown();
		u32 held = hidKeysHeld();
		u32 release = hidKeysUp();
        
        if (reset)
        {
            if (!StartROM()) forceexit = 1;
        }
			
		if (running && !pause)
		{
			// emulate
			emuTick = svcGetSystemTick();
			CPU_MainLoop(); // runs the SNES for one frame. Handles PPU rendering.
#ifdef REPORT_STATS
			FinishRendering();
			reportStats();
#else
			ContinueRendering();
#endif
		
				
			/*{
				extern u32 dbgcycles, nruns;
				// bprintf("SPC: %d / 17066  %08X\n", dbgcycles, SNES_Status->SPC_CycleRatio);
				dbgcycles = 0; nruns=0;
			}*/
			/*if (press & KEY_X) SNES_Status->SPC_CycleRatio+=0x1000;
			if (press & KEY_Y) SNES_Status->SPC_CycleRatio-=0x1000;
			SNES_Status->SPC_CyclesPerLine = SNES_Status->SPC_CycleRatio*1364;*/
				
			// SRAM autosave check
			// TODO: also save SRAM under certain circumstances (pausing, returning to home menu, etc)
			//framecount++;
			//if (!(framecount & 7))
			//	SNES_SaveSRAM();
					
			if (release & KEY_TOUCH) 
			{
				SNES_SaveSRAM();
				ClearConsole();
				bprintf("Paused.\n");
				bprintf("Tap screen or press A to resume.\n");
				bprintf("Press Select to reset.\n");
				bprintf("Press Start to enter the config.\n");
				bprintf("Press X to save game.\n");
				bprintf("Press Y to restore saved game.\n");
				pause = 1;
				svcSignalEvent(SPCSync);
			}
		}
		else
		{
			// update UI
				
			// TODO move this chunk of code somewhere else?
			if (running && !UI_Level()) // only run this if not inside a child UI
			{
				if (release & (KEY_TOUCH|KEY_A))
				{
				//	// bprintf("Resume.\n");
                    ClearConsole();
					pause = 0;
				}
				else if (release & KEY_SELECT)
				{
					
                    running = 0;
                    pause = 0;
                    reset = 1;
					/*	
					// copy splashscreen
					FinishRendering();
					SNES_Status->ScreenHeight = 224;
					ApplyScaling();
					u32* tempbuf = (u32*)linearAlloc(256*256*4);
					CopyBitmapToTexture(screenfill, tempbuf, 256, 224, 0xFF, 0, 32, 0x0);
					GSPGPU_FlushDataCache(tempbuf, 256*256*4);

					GX_DisplayTransfer(tempbuf, 0x01000100, (u32*)SNESFrame, 0x01000100, 0x3);
					//gspWaitForPPF();

					SafeWait(gspEvents[GSPGPU_EVENT_PPF]);
					linearFree(tempbuf);
                    */
				}
				else if (release & KEY_START)
				{
					UI_SaveAndSwitch(&UI_Config);
				}
				//added some saving here
				else if (release & KEY_X)
				{
					/*
					bprintf("PC: CPU %02X:%04X  SPC %04X\n", CPU_Regs.PBR, CPU_Regs.PC, SPC_Regs.PC);
					dbg_save("/snesram.bin", SNES_SysRAM, 128*1024);
					dbg_save("/spcram.bin", SPC_RAM, 64*1024);
					dbg_save("/vram.bin", PPU.VRAM, 64*1024);
					dbg_save("/oam.bin", PPU.OAM, 0x220);
					dbg_save("/cgram.bin", PPU.CGRAM, 512);
					*/
					ClearConsole();

					if(saveGame())
						bprintf("Game saved.\n");
					else
						bprintf("Something went wrong");

					bprintf("Tap Screen or press A to continue.\n");
				}
				//added some loading here
				else if(release & KEY_Y){
					
					ClearConsole();

					if(retrieveSavegame())
						bprintf("Game restored.\n");
					else
						bprintf("Something went wrong");
						
					bprintf("Tap Screen or press A to continue.\n");
					
				}
					
				if ((held & (KEY_L|KEY_R)) == (KEY_L|KEY_R))
				{
					if (!shot)
					{
						u32 timestamp = (u32)(svcGetSystemTick() / 446872);
						char file[256];
						snprintf(file, 256, "/blargSnes%08d.bmp", timestamp);
						if (TakeScreenshot(file))
						{
						//	// bprintf("Screenshot saved as:\n");
						//	// bprintf("SD:%s\n", file);
						}
						else
                            bprintf("Error saving screenshot\n");
								
						shot = 1;
					}
				}
				else
					shot = 0;
			}
				
			RenderTopScreen();
			FinishRendering();
				
			if (held & KEY_TOUCH)
			{
				hidTouchRead(&lastTouch);
				UI_Touch((press & KEY_TOUCH) ? 1:2, lastTouch.px, lastTouch.py);
				held &= ~KEY_TOUCH;
			}
			else if (release & KEY_TOUCH)
			{
				UI_Touch(0, lastTouch.px, lastTouch.py);
				release &= ~KEY_TOUCH;
			}
				
			if (press)
			{
				UI_ButtonPress(press);
					
				// key repeat
				repeatkeys = press & (KEY_UP|KEY_DOWN|KEY_LEFT|KEY_RIGHT);
				repeatstate = 1;
				repeatcount = 15;
			}
			else if (held && held == repeatkeys)
			{
				repeatcount--;
				if (!repeatcount)
				{
					repeatcount = 7;
					if (repeatstate == 2)
						UI_ButtonPress(repeatkeys);
					else
						repeatstate = 2;
				}
			}
		}
	}
	
	if (running) SNES_SaveSRAM();
	
	exitspc = 1; pause = 1;
	svcSignalEvent(SPCSync);
	if (spcthread) 
	{
		svcWaitSynchronization(spcthread, U64_MAX);
		threadJoin(spcthread, U64_MAX);
	}
	svcCloseHandle(SPCSync);

	vramFree(SNESFrame);
	vramFree(gpuOut);

	Audio_DeInit();


	PPU_DeInit();

	shaderProgramFree(&finalShaderP);
	shaderProgramFree(&softRenderShaderP);
	shaderProgramFree(&hardRenderShaderP);
	shaderProgramFree(&hard7RenderShaderP);
	shaderProgramFree(&plainQuadShaderP);
	shaderProgramFree(&windowMaskShaderP);

	DVLB_Free(finalShader);
	DVLB_Free(softRenderShader);
	DVLB_Free(hardRenderShader);
	DVLB_Free(hard7RenderShader);
	DVLB_Free(plainQuadShader);
	DVLB_Free(windowMaskShader);

	linearFree(borderVertices);
	linearFree(screenVertices);
	
	linearFree(BorderTex);

	bglDeInit();

    romfsExit();
	gfxExit();

    return 0;
}
