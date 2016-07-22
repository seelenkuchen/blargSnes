/*
    This file is part of a modifictation of blargSnes by Soulcake.
	
	blargSnes Copyright 2014 StapleButter

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

#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <3ds.h>

#include "saves.h"
#include "mem.h"
#include "cpu.h"
#include "spc700.h"
#include "ppu.h"
#include "snes.h"
#include "dsp.h"
#include "audio.h"

#define PATH "/snes/"
#define EXT ".sav"

//char* filepath = NULL;

//a is dest, b is src
void cpyStatusData(SNES_StatusData* a,SNES_StatusData* b){

	memcpy(a->__pad3, b->__pad3, sizeof(b->__pad3));		
	a->LastBusVal = b->LastBusVal;		
	a->__pad2 = b->__pad2;			
	a->SPC_CyclesPerLine = b->SPC_CyclesPerLine;	
	a->SPC_CycleRatio=b->SPC_CycleRatio;		
	a->SPC_LastCycle = b->SPC_LastCycle;		
	a->IRQ_VMatch=b->IRQ_VMatch;		
	a->IRQ_HMatch=b->IRQ_HMatch;		
	a->IRQ_CurHMatch=b->IRQ_CurHMatch; 
	a->VCount=b->VCount;		
	memcpy(a->__pad1, b->__pad1, sizeof(b->__pad1));
	a->HCount=b->HCount;	
	a->SRAMMask=b->SRAMMask;	
	a->TotalLines=b->TotalLines;		
	a->ScreenHeight=b->ScreenHeight; 	
	a->IRQCond=b->IRQCond;		
	a->HVBFlags=b->HVBFlags;	
	a->SRAMDirty=b->SRAMDirty;	

}
/*
void cpyPPU_ModeSection(PPU_ModeSection* dest, PPU_ModeSection* src){
	dest-> EndOffset =src->EndOffset;
	dest-> Mode =src-Mode>;
	dest-> MainScreen=src->MainScreen;
	dest-> SubScreen=src->SubScreen;
	dest-> ColorMath1=src->ColorMath1;
	dest-> ColorMath2=src->ColorMath2;
} 
*/

char* generateFilepath(){

	//Some stuff I found in another function which came in useful
	int useid = 0;
    const char* infoFilePath = "romfs:/rom.txt";
    FILE *cFile = fopen(infoFilePath, "r");
    if(cFile == NULL) useid = 1;
	
    char tempstr[256];
    char cPath[] = "/snes/";
	char cExt[] = ".sav"; 
	
    if (useid)
    {
        u64 ID;
        aptOpenSession();
        APT_GetProgramID(&ID);
        aptCloseSession();
        sprintf(tempstr, "%lx", ID);
    }
    else fgets(tempstr, sizeof(tempstr), cFile);

	fclose(cFile);

	//There might be a newline that needs killing
	if(tempstr[strlen(tempstr)-1]=='\n')
		tempstr[strlen(tempstr)-1]='\0';
	
	//puzzling together the filepath
	//allocate memory for the filepath and saving one index for the \0 terminator
	char* filepath= malloc(strlen(cPath)+strlen(tempstr)+strlen(cExt)+1);
	strcpy(filepath,cPath);
	strcat(filepath,tempstr);
	strcat(filepath,cExt);

	/*
	DEBUG stuff
	FILE* tmp = fopen("/snes/log.txt", "a");
	fputs(filepath,tmp);
	fclose(tmp);
	*/
	return filepath;

}

bool saveGame(){

	//Helpers
	SaveState* temp=malloc(sizeof(SaveState));

	char* filepath = generateFilepath();

	
	//Filling my Savegame Struct
	memcpy(temp->SysRam, SNES_SysRAM, sizeof(SNES_SysRAM));
	memcpy(temp->SpcRam, SPC_RAM, sizeof(SPC_RAM));
	memcpy(temp->DSP_MEM_Back,DSP_MEM, sizeof(DSP_MEM));
	memcpy(temp->SPC_ROM_Back,SPC_ROM, sizeof(SPC_ROM));
	memcpy(temp->SPC_IOPorts_back,SPC_IOPorts, sizeof(SPC_IOPorts));
	//memcpy(temp->SPC_IOUnread_back,SPC_IOUnread, sizeof(SPC_IOUnread));
	temp->SNES_JoyBit_back =SNES_JoyBit;
    temp->SNES_AutoJoypad_back=SNES_AutoJoypad;
    temp->SNES_JoyBuffer_back=SNES_JoyBuffer;
    temp->SNES_Joy16_back=SNES_Joy16;
	
	cpyStatusData(&temp->Status_Temp, SNES_Status);
	temp->CPU_Reg_Temp=CPU_Regs;
	temp->SPC_Reg_Temp=SPC_Regs;
	temp->PPU_Temp = PPU;


	
	//Writing the savestate to a file
	//FILE * SaveFile = fopen("/snes/temp.bin", "wb");
	FILE * SaveFile = fopen(filepath, "wb");

	if(SaveFile!=NULL){
		fwrite(temp, sizeof(SaveState),1,SaveFile);
		fclose(SaveFile);
	}else
		return false;
	
	free(temp);
	free(filepath);
	return true;
}

bool retrieveSavegame(){
	
	//Helpers
	SaveState* temp=malloc(sizeof(SaveState));
	
	char* filepath = generateFilepath();
	
	//Grabbing the save
	//FILE * SaveFile = fopen("/snes/temp.bin", "rb");
	FILE * SaveFile = fopen(filepath, "rb");

	if(SaveFile != NULL)
	{
		fread(temp, sizeof(SaveState),1,SaveFile);
		fclose(SaveFile);
	
		//Restoring the safe
		memcpy(SNES_SysRAM, temp->SysRam,sizeof(temp->SysRam));
		memcpy(SPC_RAM,temp->SpcRam,sizeof(temp->SpcRam));
		memcpy(DSP_MEM, temp->DSP_MEM_Back, sizeof(temp->DSP_MEM_Back));
		memcpy(SPC_ROM,temp->SPC_ROM_Back, sizeof(temp->SPC_ROM_Back));
		memcpy(SPC_IOPorts,temp->SPC_IOPorts_back, sizeof(temp->SPC_IOPorts_back));
		//memcpy(SPC_IOUnread,temp->SPC_IOUnread_back, sizeof(temp->SPC_IOUnread_back));
		
		//Getting the joypad back
		SNES_JoyBit=temp->SNES_JoyBit_back;
     	SNES_AutoJoypad=temp->SNES_AutoJoypad_back;
    	SNES_JoyBuffer=temp->SNES_JoyBuffer_back;
    	SNES_Joy16=temp->SNES_Joy16_back;
		
		cpyStatusData(SNES_Status, &temp->Status_Temp);
		CPU_Regs = temp->CPU_Reg_Temp;
		SPC_Regs = temp->SPC_Reg_Temp;
		//PPU = temp->PPU_Temp;   //This will come back to haunt me

		
		//Restoring the PPU
		memcpy(PPU.OBJBuffer,temp->PPU_Temp.OBJBuffer, sizeof(temp->PPU_Temp.OBJBuffer));
		
		PPU.CGRAMAddr=temp->PPU_Temp.CGRAMAddr;
		PPU.CGRAMVal=temp->PPU_Temp.CGRAMVal;
		memcpy(PPU.CGRAM,temp->PPU_Temp.CGRAM, sizeof(temp->PPU_Temp.CGRAM));		// SNES CGRAM, xBGR1555
		memcpy(PPU.Palette,temp->PPU_Temp.Palette, sizeof(temp->PPU_Temp.Palette));	// our own palette, converted to RGBx5551
		memcpy(PPU.PaletteUpdateCount,temp->PPU_Temp.PaletteUpdateCount, sizeof(temp->PPU_Temp.PaletteUpdateCount));
		
		PPU.PaletteUpdateCount256=temp->PPU_Temp.PaletteUpdateCount256;
		
		memcpy(PPU.VRAM,temp->PPU_Temp.VRAM, sizeof(temp->PPU_Temp.VRAM));
		memcpy(PPU.VRAM7,temp->PPU_Temp.VRAM7, sizeof(temp->PPU_Temp.VRAM7));
		memcpy(PPU.VRAMUpdateCount,temp->PPU_Temp.VRAM, sizeof(temp->PPU_Temp.VRAMUpdateCount));
		memcpy(PPU.VRAM7UpdateCount,temp->PPU_Temp.VRAM7, sizeof(temp->PPU_Temp.VRAM7UpdateCount));
		PPU.VRAMAddr =temp->PPU_Temp.VRAMAddr;
		PPU.VRAMPref =temp->PPU_Temp.VRAMPref;
		PPU.VRAMInc =temp->PPU_Temp.VRAMInc;
		PPU.VRAMStep =temp->PPU_Temp.VRAMStep;

		memcpy(PPU.TileBitmap,temp->PPU_Temp.TileBitmap, sizeof(temp->PPU_Temp.TileBitmap));
		memcpy(PPU.TileEmpty,temp->PPU_Temp.TileEmpty, sizeof(temp->PPU_Temp.TileEmpty));
		
		PPU.OAMAddr=temp->PPU_Temp.OAMAddr;
		PPU.OAMVal=temp->PPU_Temp.OAMVal;
		PPU.OAMPrio=temp->PPU_Temp.OAMPrio;
		PPU.FirstOBJ=temp->PPU_Temp.FirstOBJ;
		PPU.OAMReload=temp->PPU_Temp.OAMReload;
		memcpy(PPU.OAM,temp->PPU_Temp.OAM, sizeof(temp->PPU_Temp.OAM));

		memcpy(PPU.BG,temp->PPU_Temp.BG, sizeof(temp->PPU_Temp.BG));
		
	}
	else 
		return false;
	//Initializing the audio if off
	Audio_Init();
	free(temp);
	return true;
}
