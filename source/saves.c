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

#define PATH "/snes/"
#define EXT ".sav"

//char* filepath = NULL;

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
		memcpy(SNES_SysRAM, temp->SysRam,sizeof(SNES_SysRAM));
		memcpy(SPC_RAM,temp->SpcRam,sizeof(SPC_RAM));
		memcpy(DSP_MEM, temp->DSP_MEM_Back, sizeof(DSP_MEM));
		
		CPU_Regs = temp->CPU_Reg_Temp;
		SPC_Regs = temp->SPC_Reg_Temp;
		PPU = temp->PPU_Temp;
		
	}
	else 
		return false;
	
	free(temp);
	return true;
}
