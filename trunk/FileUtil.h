#ifndef __FILEUTIL_H__
#define __FILEUTIL_H__

#include <vector>
#include <string>

#ifdef __APPLE__
#include <dirent.h>
#else
#include <windows.h>
#endif

using namespace std;

#ifndef MAX_PATH
#define MAX_PATH   260
#endif

void GetDirList(char* szDir, vector<string> &vFiles, vector<string> &vDirs);
void ExtractFilename(char* szPath, char* szOutFile);
void ExtractDirname(char* szPath, char* szOutDir);
void ExtractExtension(char* szPath, char* szOutExt);
void ReplaceSlashes(char* szPath);
bool AreFilesSame(char* szFile1, char* szFile2);

float HashExtension(char* szPath);


#endif // __FILEUTIL_H__
