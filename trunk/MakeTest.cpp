
#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <vector>
#include <string>
#include <errno.h>
#include <io.h>

#include "FileUtil.h"

#define CHUNK_SIZE    4096 // size of file block to be read
#define FILE_COUNT    500    // number of files to generate

#ifdef __APPLE__
	bool g_bMac = true;
	char g_szSlash[2] = "/";
#else
	bool g_bMac = false;
	char g_szSlash[2] = "\\";
#endif

char g_szSourceDir[] = "C:\\Program Files\\";
char g_szDestDir[] = "C:\\Code\\Local\\221\\Diff\\";
int  g_nFiles = 0;
bool g_bReport = true;
unsigned char* g_pBuf = (unsigned char*) malloc(CHUNK_SIZE);


/////////////////////////////////////////////////////////////////////////////
// MakeTest: main() and subroutines

// ProcessFile() computes the hash and auxiliary count table for szFilePath
void ProcessFile(char* szFilePath)
{
	char szFilename[MAX_PATH];
	char szTargetPath[MAX_PATH];
	ExtractFilename(szFilePath, szFilename);
	sprintf(szTargetPath, "%s%s", g_szDestDir, szFilename);
	if (_access(szTargetPath, 0) == 0) // file w/same name already exists
		return;

	// Can't open source file... ignore and move on
	FILE* fpSrc = fopen(szFilePath, "rb");
	if (fpSrc == NULL)
		return;

	// Compute file size
	fseek(fpSrc , 0 , SEEK_END);
	int nSize = ftell(fpSrc);
	if (nSize < CHUNK_SIZE) // too small... throw it back
	{
		fclose(fpSrc);
		return;
	}
	rewind(fpSrc);

	FILE* fpDst = fopen(szTargetPath, "wb");
	if (fpDst == NULL) // can't make file?
	{
		fclose(fpDst);
		return;
	}

	//read file chunk into buffer
	if (fread(g_pBuf, 1, CHUNK_SIZE, fpSrc) == CHUNK_SIZE)
	{
		fwrite(g_pBuf, 1, CHUNK_SIZE, fpDst);
		g_nFiles++;
	}

	fclose(fpSrc);
	fclose(fpDst);
}	// ProcessFile


void ProcessDir(char* szDirName)
{
	// Append slash to szDirName
	if (szDirName[strlen(szDirName)-1] != g_szSlash[0])
		strcat(szDirName, g_szSlash);

	// Ignore folders starting with "$"
	if (szDirName[strlen(szDirName)-2] == '$')
		return;

	std::vector<string> vFiles;
	std::vector<string> vDirs;
	GetDirList(szDirName, vFiles, vDirs);
	char szFilePath[MAX_PATH];

	// Process files in this directory
	if (g_bReport)
		printf("Processing dir: %s\n", szDirName);
	for (int i = 0; (i < (int) vFiles.size()) && (g_nFiles < FILE_COUNT); i++)
	{
		sprintf(szFilePath, "%s%s", szDirName, vFiles[i].c_str());
		ProcessFile(szFilePath);
		if ( g_bReport && (i % 100 == 0) && i )
			printf(" %d", i);
	}
	if ( g_bReport && (vFiles.size() > 100) )
		printf("\n");
	
	// Recusively process subdirectories
	vector<string>::iterator iter = vDirs.begin();
	for (; (iter != vDirs.end()) && (g_nFiles < FILE_COUNT); iter++)
	{
		sprintf(szFilePath, "%s%s", szDirName, (*iter).c_str());
		ProcessDir(szFilePath);
	}
}	// ProcessDir


int main(int argc, char* const argv[])
{
	ProcessDir(g_szSourceDir);
	return 0;
}


