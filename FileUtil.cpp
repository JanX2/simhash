
#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
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


