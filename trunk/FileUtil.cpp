
#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <math.h>
#include <errno.h>
#include "FileUtil.h"


// GetDirList() returns lists of all files and folders in szDir
#ifdef __APPLE__
void GetDirList(char* szDir, std::vector<string> &vFiles, std::vector<string> &vDirs)
{
	DIR *dp; 
	struct dirent *dirp;
	
	if (((dp  = opendir(szDir)) == NULL))
		std::cout << "Error(" << errno << ") opening " << szDir << endl;
 
	while ((dirp = readdir(dp)) != NULL)
	{
		if (dirp->d_type == DT_REG) //only try to process regular files
			vFiles.push_back(string(dirp->d_name));
		else if ((dirp->d_type == DT_DIR) && (dirp->d_name[0] != '.'))
			vDirs.push_back(string(dirp->d_name));
	}
	
	closedir(dp);
	free(dirp);
}	// GetDirList (Mac)
#else
void GetDirList(char* szDir, vector<string> &vFiles, vector<string> &vDirs)
{
	// Build wildcard file specifier
	char szWildCard[MAX_PATH];
	if (strlen(szDir) == 0)
		sprintf(szWildCard, "*");
	else
		sprintf(szWildCard, "%s*", szDir);

	// Collect file names
	WIN32_FIND_DATAA fileData;
	HANDLE hFind = FindFirstFileA(szWildCard, &fileData);
	BOOL bMoreFiles = 1;
	string str;
	while ((hFind != INVALID_HANDLE_VALUE) && bMoreFiles)
	{
		str.assign(fileData.cFileName);
		if ( !(fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) )
			vFiles.push_back(str);
		else if (fileData.cFileName[0] != '.')
			vDirs.push_back(str);
		bMoreFiles = FindNextFileA(hFind, &fileData);
	}
    FindClose(hFind);
}	// GetDirList (Windows)
#endif


// Extracts the file name from a full path
void ExtractFilename(char* szPath, char* szOutFile)
{
	char* szPtr = strrchr(szPath, '\\');
	if (szPtr == NULL)
		szPtr = strrchr(szPath, '/');

	if (szPtr == NULL)
		strcpy(szOutFile, szPath);
	else
		strcpy(szOutFile, &(szPtr[1]));
}	// ExtractFilename


// Extracts the directory path (including slash) from a full path
void ExtractDirname(char* szPath, char* szOutDir)
{
	strcpy(szOutDir, szPath);
	char* szPtr = strrchr(szOutDir, '\\');
	if (szPtr == NULL)
		szPtr = strrchr(szOutDir, '/');

	if (szPtr != NULL)
		szPtr[1] = 0;
}	// ExtractDirname


// Doesn't include dot
void ExtractExtension(char* szPath, char* szOutExt)
{
	char* szSlashPtr = strrchr(szPath, '\\');
	if (szSlashPtr == NULL)
		szSlashPtr = strrchr(szPath, '/');
	char* szDotPtr = strrchr(szPath, '.');

	if ( (szDotPtr == NULL) || (szDotPtr < szSlashPtr) )
		sprintf(szOutExt, "");
	else
		strcpy(szOutExt, &(szDotPtr[1]));
}	// ExtractExtension


// Replaces all \'s with /'s
void ReplaceSlashes(char* szPath)
{
	char* szPtr = strrchr(szPath, '\\');
	while (szPtr != NULL)
	{
		szPtr[0] = '/';
		szPtr = strrchr(szPath, '\\');
	}
}	// ReplaceSlashes


#define  CHUNK_SIZE   4096
// Check if these two files are identical
bool AreFilesSame(char* szFile1, char* szFile2)
{
	// Open files
	FILE* fp1 = fopen(szFile1, "rb");
	if (fp1 == NULL)
	{
		printf("ProcessFile: failed to load %s\n", szFile1);
		return false;
	}
	FILE* fp2 = fopen(szFile2, "rb");
	if (fp2 == NULL)
	{
		fclose(fp1);
		printf("ProcessFile: failed to load %s\n", szFile2);
		return false;
	}

	// Compute file sizes
	fseek(fp1 , 0 , SEEK_END);
	int nSize1 = ftell(fp1);
	rewind(fp1);
	fseek(fp2 , 0 , SEEK_END);
	int nSize2 = ftell(fp2);
	rewind(fp2);
	if (nSize1 != nSize2)
	{
		fclose(fp1);
		fclose(fp2);
		return false;
	}

	bool bSame = true;
	int* pBuf1 = (int*) malloc(CHUNK_SIZE+8);
	int* pBuf2 = (int*) malloc(CHUNK_SIZE+8);
	while (nSize1 > 0)
	{
		//read file chunks into buffers
		int nRead = min(nSize1, CHUNK_SIZE);
		fread(pBuf1, 1, nRead, fp1);
		fread(pBuf2, 1, nRead, fp2);
		nSize1 -= nRead;

		// Check for similarities
		int nNumInts = (nRead+3)/4;
		for (int i = 0; i < nNumInts; i++)
		{
			if (pBuf1[i] != pBuf2[i])
			{
				bSame = false;
				nSize1 = 0; // break while loop
				break;
			}
		}
	}

	fclose(fp1);
	fclose(fp2);
	free(pBuf1);
	free(pBuf2);
	return bSame;
}	// AreFilesSame


// Returns a float between 0 and 1, (almost certainly) unique to the
// extension, where all three chars are weighted about equally
float HashExtension(char* szPath)
{
	char szExt[MAX_PATH];
	ExtractExtension(szPath, szExt);
	strupr(szExt);
	int nLen = (int) strlen(szExt);
	int nTemp, nHash = 0;
	int nPow35[] = { 1, 35, 1225, 42875 }; // powers of 35
	for (int i = 0; i < min(3, nLen); i++)
	{
		// Shift ASCII code to the 0-35 range
		nTemp = max(0, szExt[i]-48);
		if (nTemp >= 17)
			nTemp -= 7; // shift letters down next to numbers
		nHash += nTemp * (nPow35[3] + nPow35[i]);
	}
	float fHash = ((float)nHash)/4501875.0f; // divide by max possible value
	return (float) (abs(fHash-0.5)*2.0); // fold 
}	// HashExtension
