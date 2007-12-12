
#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <errno.h>

#include "SimHash.h"
#include "Results.h"
 
// NOTE: Do not allow the last x bits to match the first x for x > 2
// Also, choose tags to avoid large overlap (i.e. correlation)
// QUESTION: Is this actually a big deal?
 
// Future improvemnet: store tag values in a bin search tree, so any given
// bit pattern from the file can search the tree and quickly find the one
// (if any) match

#define CHUNK_SIZE    4096 // size of file block to be read


#ifdef __APPLE__
	bool g_bMac = true;
	char g_szSlash[2] = "/";
#else
	bool g_bMac = false;
	char g_szSlash[2] = "\\";
#endif

bool g_bReport = true;

/* TODO:
 * scale sim tolerance based on file size
*/
/////////////////////////////////////////////////////////////////////////////
// CTags

CTags::CTags(char* szTagFile)
{
	// From: http://www.codeproject.com/cpp/endianness.asp
	short word = 0x4321;
	m_bBigEndian = ((*(char *)& word) != 0x21);

	m_pTags = NULL;
	m_nTags = 0;
	m_pfWeights = NULL;
	m_nWeights = 0;
	ReadTagFile(szTagFile);
}

CTags::~CTags()
{
	free(m_pTags);
	free(m_pfWeights);
}


int CTags::MatchBitString(DWORD dwString)
{
	int nMatch = -1;
	for (int i = 0; i < m_nTags; i++)
	{
		if (m_pTags[i].nIgnore)
			m_pTags[i].nIgnore--;
		else if (Compare(dwString, m_pTags[i].dwTag, m_pTags[i].dwMask))
		{
			// Found a matching tag
			nMatch = i;
			// Don't compare this tag with remainder of matched bits
			m_pTags[i].nIgnore = m_pTags[i].nLength - 1;
		}
	}
	return nMatch;
}	// CTags::MatchBitString


bool CTags::ReadTagFile(char* szFilePath)
{
	// Open tag file in text mode
	FILE* fp = fopen(szFilePath, "rt");
	if (fp == NULL)
	{
		printf("ReadTagFile: failed to load %s\n", szFilePath);
		return false;
	}

	// Read tag & key counts out of header
	if ((fscanf(fp, "%d, %d,", &m_nTags, &m_nWeights) != 2) || (m_nTags <= 0))
	{
		printf("ReadTagFile: Invalid format for tag file header\n");
		printf("   Format: #TAGS, #KEYS, [key names]\n");
		fclose(fp);
		return false;
	}

	// Read key name(s) out of header
	int i;
	m_pfWeights = (float*) calloc(m_nTags*m_nWeights, sizeof(float));
	char szBuffer[50];
	for (i = 0; i < m_nWeights; i++)
	{
		if ((fscanf(fp, " %s,", szBuffer) != 1))
			break;
		int nLen = (int) strlen(szBuffer);
		if (szBuffer[nLen-1] == ',')
			szBuffer[nLen-1] = 0;
		m_strKeys.push_back(szBuffer);
	}
	if (i < m_nWeights) // loop broke early due to fscanf error
	{
		printf("ReadTagFile: Invalid keyname %d\n", i+1);
		return false;
	}
	fscanf(fp, "\n");

	// Read tags out of file, fill in BINTAG structs
	m_pTags = (BINTAG*) calloc(m_nTags, sizeof(BINTAG));
	for (i = 0; i < m_nTags; i++)
	{
		// Read tag from file line
		if ( fscanf(fp, "%x, %x",
				&(m_pTags[i].dwTag),
				&(m_pTags[i].dwMask)) != 2 )
			break;
		
		m_pTags[i].dwOrigTag = m_pTags[i].dwTag;
		FixTag( &(m_pTags[i]) );
		// (*ppOutTags)[i].nIgnore is zero'd by calloc

		// Read weights
		m_pTags[i].pfWeights = &(m_pfWeights[i*m_nWeights]);
		for (int j = 0; j < m_nWeights; j++)
		{
			if (fscanf(fp, ", %f", &(m_pTags[i].pfWeights[j])) != 1)
				break;
		}
		fscanf(fp, "\n");
	}

	fclose(fp);
	if (i < m_nTags) // loop broke early due to fscanf error
	{
		printf("ReadTagFile: Invalid format for tag %d\n", i+1);
		m_nTags = 0;
	}
	return (i == m_nTags);
}	// CTags::ReadTagFile


// In order to read from left to right, we need to shift our bits to occupy the
// k *most* significant bits of a DWORD instead of the k *least* significant.
// On input, dwTag and dwMask should be filled in as read.  These get fixed,
// and nLength gets filled in.
void CTags::FixTag(BINTAG* pTag)
{
	// Compute length of tag
	DWORD dwMask = pTag->dwMask;
	while (dwMask)
	{
		pTag->nLength++;
		dwMask = dwMask >> 1;
	}

	// Move bits
	pTag->dwTag <<= (sizeof(DWORD)*8 - pTag->nLength);
	pTag->dwMask <<= (sizeof(DWORD)*8 - pTag->nLength);
}	// CTags::FixTag


bool CTags::Compare(DWORD s1, DWORD s2, DWORD dwMask)
{
	return (s1 & dwMask) == (s2 & dwMask);
}	// CTags::Compare



/////////////////////////////////////////////////////////////////////////////
// main() and subroutines

void ProcessChunk(BYTE* pChunk, int nChunkSize, CTags* pTags, CResults* pResults)
{
	// TODO: add code to properly check last few bytes

	//cycle through bytes 
	DWORD dwString; // bit string currently being examined
	int nTag;
	for (int byte = 0; byte < (nChunkSize - 3); byte++)
	{
		DWORD dwBlock = *((DWORD*) &(pChunk [byte])); // 32-bit block
		if (!pTags->IsBigEndian()) // reverse bytes
			dwBlock = ( ((dwBlock&0x000000FF)<<24) + ((dwBlock&0x0000FF00)<<8) +
						((dwBlock&0x00FF0000)>>8) + ((dwBlock&0xFF000000)>>24) );

		for (int bit = 0; bit < 8; bit++)
		{
			dwString = dwBlock << bit;
			nTag = pTags->MatchBitString(dwString);
			if (nTag >= 0)
				pResults->IncrTag(nTag);
		}
	}
}	// ProcessChunk


// ProcessFile() computes the hash and auxiliary count table for szFilePath
bool ProcessFile(char* szFilePath, CTags* pTags, CResults* pResults)
{
	FILE* fp = fopen(szFilePath, "rb");
	if (fp == NULL)
	{
		printf("ProcessFile: failed to load %s\n", szFilePath);
		return false;
	}

	// Compute file size
	fseek(fp , 0 , SEEK_END);
	int nSize = ftell(fp);
	rewind(fp);

	// Start new "row" in pResults store
	pResults->NewFile(szFilePath, nSize);

	BYTE* pBuf = (BYTE*) malloc(CHUNK_SIZE+8);
	while (nSize > 4)
	{
		//read file chunk into buffer
		int nRead = min(nSize, CHUNK_SIZE);
		nSize -= nRead;
		fread(pBuf, 1, nRead, fp);

		//process contents of buffer
		ProcessChunk(pBuf, nRead, pTags, pResults);

		//rewind the file pointer
		int nPos = ftell(fp) - 4;
		fseek (fp, nPos, SEEK_SET);
		nSize +=4;
	}

	pResults->CloseFile();
	fclose(fp);
	free(pBuf);
	return true;
}	// ProcessFile


// This function should only be called with the pTags associated with
// a given file.  Note that szDirName will have a slash appended to it
// if need be.  Returns TRUE if any files are processed.
bool ProcessDir(char* szDirName, CTags* pTags, CResults* pResults)
{
	// Append slash to szDirName
	if (szDirName[strlen(szDirName)-1] != g_szSlash[0])
		strcat(szDirName, g_szSlash);	

	// This directory has already been processed
	if ( !pResults->CheckValidDir(szDirName) )
		return false;

	bool bRet = false;
	std::vector<string> vFiles;
	std::vector<string> vDirs;
	GetDirList(szDirName, vFiles, vDirs);
	char szFilePath[MAX_PATH];
	
	// Recusively process subdirectories
	vector<string>::iterator iter = vDirs.begin();
	for (; iter != vDirs.end(); iter++)
	{
		sprintf(szFilePath, "%s%s", szDirName, (*iter).c_str());
		bRet |= ProcessDir(szFilePath, pTags, pResults);
	}

	// Process files in this directory
	if (g_bReport)
		printf("Processing dir: %s\n", szDirName);
	for (int i = 0; i < (int) vFiles.size(); i++)
	{
		sprintf(szFilePath, "%s%s", szDirName, vFiles[i].c_str());
		ProcessFile(szFilePath, pTags, pResults);
		if ( g_bReport && (i % 100 == 0) && i )
			printf(" %d", i);
	}
	if ( g_bReport && (vFiles.size() > 100) )
		printf("\n");

	return (bRet || !vFiles.empty());
}	// ProcessDir


int main(int argc, char* const argv[])
{
	char szSimHashIni[MAX_PATH];
	char szTagFile[MAX_PATH];
	int  nStoreType;
	char szStore[MAX_PATH];
	char szDirectory[MAX_PATH];
long long foo = atoll("4123456789");
	// Read specs for run out of INI file
	if (argc > 1)
		strcpy(szSimHashIni, argv[1]);
	else if (g_bMac)
		sprintf(szSimHashIni, "/Users/ipye/Desktop/cmps221/1_hashproj/sub/simhash/MacHash.ini");
	else
		sprintf(szSimHashIni, "WinHash.ini");


	FILE* fp = fopen(szSimHashIni, "rt");
	if (fp == NULL)
	{
		printf("main: failed to load %s\n", szSimHashIni);
		return 1;
	}

	if ((fscanf(fp, "TagFile=%s\n", szTagFile) != 1) ||
		(fscanf(fp, "StoreType=%d\n", &nStoreType) != 1) ||
		(fscanf(fp, "Store=%s\n", szStore) != 1) ||
		(fscanf(fp, "Directory=%s", szDirectory) != 1) )
	{
		printf("main: Invalid INI file '%s'\n", szSimHashIni);
		return 1;
	}
	fclose(fp);

	// Set up CTags and CReport objects
	CTags* pTags = new CTags(szTagFile);
	CResults* pResults = NULL;
	switch (nStoreType)
	{
		case 1:	// SQL
			pResults = new CResultsSQL(pTags->GetTagCount());
			if (strlen(szStore) == 0)
			{
				strcpy(szStore, szTagFile);
				char* szEnd = strrchr(szStore, '.');
				if (szEnd != NULL)
					szEnd[0] = 0;
			}//szStore is now the name of the table
			//else add tags from TagFile to szStore table
			break;
		case 2: // CSV
			pResults = new CResultsCSV(pTags->GetTagCount());
			break;
		default:
			pResults = new CResults(pTags->GetTagCount());
			break;
	}

	// Compute SimHash for each file in directory
	if (pResults->OpenStore(szStore, pTags))
	{
		if ( ProcessDir(szDirectory, pTags, pResults) )
			pResults->CommitStore();
	}

	delete pTags;
	delete pResults;
	return 0; // success
}


