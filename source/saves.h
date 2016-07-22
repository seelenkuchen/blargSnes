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


#ifndef _SAVES_H_
#define _SAVES_H_

#include "cpu.h"
#include "spc700.h"
#include "ppu.h"
#include "snes.h"

//#include "dsp.h"

//Still figuring out which values are needed, saving the whole PPU is probably overkill
 typedef struct
{
	u8 SysRam[0x20000];
	u8 SpcRam[0x10040];
	u8 DSP_MEM_Back[0x100];
    u8 SPC_ROM_Back[0x40];
    u8 SPC_IOPorts_back[8];

    //u8 SPC_IOUnread_back[4];
    u8 SNES_JoyBit_back;
    u8 SNES_AutoJoypad_back;
    u32 SNES_JoyBuffer_back;
    u8 SNES_Joy16_back;

    SNES_StatusData Status_Temp;
	CPU_Regs_t CPU_Reg_Temp;
	SPC_Regs_t SPC_Reg_Temp;
	PPUState PPU_Temp;
	
} SaveState;


char* generateFilepath();
bool saveGame();
bool retrieveSavegame();

#endif
