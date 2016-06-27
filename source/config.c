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

#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <3ds.h>

#include "main.h"
#include "config.h"

Config_t Config;

const char* configFilePath = "romfs:/blargSnes.ini";
const char* altConfigFilePath = "/blargSnes.ini";
char saveConfigFilePath[300];

const char* configFileL = 
	"HardwareRenderer=%d\n"
	"ScaleMode=%d\n"
	"DirPath=%[^\t\n]\n"
    "Mode7=%d\n";

const char* configFileS = 
	"HardwareRenderer=%d\n"
	"ScaleMode=%d\n"
	"DirPath=%s\n"
    "Mode7=%d\n";

char * lastDir[0x106];


void LoadConfig(u8 init)
{
    int useid = 0;
    const char* infoFilePath = "romfs:/rom.txt";
    FILE *cFile = fopen(infoFilePath, "r");
    if(cFile == NULL) useid = 1;

    char tempstr[256];
    const char* cPath = "/snes/";
    const char* cExt = ".ini";

    if (useid)
    {
        u64 ID;
        aptOpenSession();
        APT_GetProgramID(&ID);
        aptCloseSession();
        sprintf(tempstr, "%lx", ID);
    }
    else fgets(tempstr, sizeof(tempstr), cFile);

    strncpy(saveConfigFilePath, cPath, 6);
    strncpy(saveConfigFilePath + 6, tempstr, strlen(tempstr));
    strncpy(saveConfigFilePath + strlen(tempstr) + 4, cExt, 4);

    fclose(cFile);

	char tempDir[0x106];
	Config.HardwareRenderer = 1;
	Config.ScaleMode = 0;
    Config.HardwareMode7Filter = 0;
	if(init) {
		strncpy(Config.DirPath,"/\0",2);
		strncpy(lastDir,"/\0",2);
	}

	FILE *pFile = fopen(saveConfigFilePath, "rb");
    
	if(pFile == NULL)
		pFile = fopen(configFilePath, "rb");
    
    if(pFile == NULL)
        pFile = fopen(altConfigFilePath, "rb");

    if(pFile == NULL)
        return;

	fseek(pFile, 0, SEEK_END);
	u32 size = ftell(pFile);
	if(!size)
	{
		fclose(pFile);
		return;
	}

	char* tempbuf = (char*)linearAlloc(size + 1);
	fseek(pFile, 0, SEEK_SET);
	fread(tempbuf, sizeof(char), size, pFile);
	tempbuf[size] = '\0';

	sscanf(tempbuf, configFileL, 
		&Config.HardwareRenderer,
		&Config.ScaleMode,
		tempDir,
        &Config.HardwareMode7Filter);

	if(Config.HardwareMode7Filter == -1)
		Config.HardwareMode7Filter = 0;

	if(init && strlen(tempDir) > 0)
	{
		DIR *pDir = opendir(tempDir);
		if(pDir != NULL)
		{
			strncpy(Config.DirPath, tempDir, 0x106);
			strncpy(lastDir, tempDir, 0x106);
			closedir(pDir);
		}

	}
	
	fclose(pFile);
	linearFree(tempbuf);
}

void SaveConfig(u8 saveCurDir)
{
    int useid = 0;
    const char* infoFilePath = "romfs:/rom.txt";
    FILE *cFile = fopen(infoFilePath, "r");
    if(cFile == NULL) useid = 1;

    char tempstr[256];
    const char* cPath = "/snes/";
    const char* cExt = ".ini";

    if (useid)
    {
        u64 ID;
        aptOpenSession();
        APT_GetProgramID(&ID);
        aptCloseSession();
        sprintf(tempstr, "%lx", ID);
    }
    else fgets(tempstr, sizeof(tempstr), cFile);

    strncpy(saveConfigFilePath, cPath, 6);
    strncpy(saveConfigFilePath + 6, tempstr, strlen(tempstr));
    strncpy(saveConfigFilePath + strlen(tempstr) + 4, cExt, 4);

    fclose(cFile);
    
	char tempDir[0x106];
	if(!saveCurDir)
		strncpy(tempDir,lastDir,0x106);
	else
		strncpy(tempDir,Config.DirPath,0x106);

	FILE *pFile = fopen(saveConfigFilePath, "wb");
	if(pFile == NULL)
	{
		// bprintf("Error while saving config\n");
		return;
	}
	
	char* tempbuf = (char*)linearAlloc(1024);
	u32 size = snprintf(tempbuf, 1024, configFileS, 
		Config.HardwareRenderer,
		Config.ScaleMode,
		tempDir,
        Config.HardwareMode7Filter);
	
	fwrite(tempbuf, sizeof(char), size, pFile);
	fclose(pFile);


	linearFree(tempbuf);

	if(saveCurDir)
		strncpy(lastDir,Config.DirPath,0x106);
}
