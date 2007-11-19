
#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include "Results.h"

using namespace std;

#define STRLEN_FILE   16
#define STRLEN_TAG    8

/////////////////////////////////////////////////////////////////////////////
// CResults

CResults::CResults(int nTags)
{
	m_nTags = nTags;
	m_pnSumTable = (DWORD*) calloc(nTags, sizeof(DWORD));
	memset(m_szFileName, 0, MAX_PATH);

	m_szRowBuffer = (char*) malloc(STRLEN_FILE + 20 + (STRLEN_TAG + 6)*m_nTags);
}

CResults::~CResults()
{
	free(m_pnSumTable);
	if (m_szRowBuffer != NULL)
		free(m_szRowBuffer);
}


void CResults::NewFile(char* szFile)
{
	strcpy(m_szFileName, szFile);
	memset(m_pnSumTable, 0, m_nTags*sizeof(DWORD));
}	// CResults::NewFile


void CResults::IncrTag(int nTag)
{
	m_pnSumTable[nTag]++;
}	// CResults::IncrTag


void CResults::CloseFile()
{
	if (strlen(m_szFileName) == 0)
		return;
/*
	// Make file name exactly 15 characters
	char szTruncName[MAX_PATH];
	ExtractFilename(m_szFileName, szTruncName);
	szTruncName[16] = 0;
	int nTrunc = (int) strlen(szTruncName);
	while (nTrunc < 16)
	{
		strcat(szTruncName, " ");
		nTrunc++;
	}

	// Output results row to console
	printf("%s  ", szTruncName);
	for (int i = 0; i < m_nTags; i++)
		printf("%8d ", m_pnSumTable[i]);
	printf("\n");
*/
	FormatRowBufferTxt();
	printf("%s", m_szRowBuffer);
}	// CResults::CloseFile


int CResults::ComputeHashKey(CTags* pTags)
{
	int nKey = 0;
	for (int i = 0; i < m_nTags; i++)
		nKey += (m_pnSumTable[i] * pTags->GetTag(i)->nWeight);
	return nKey;
}	// CResults::ComputeHashKey


void CResults::FormatRowBufferTxt()
{
	// Make file name exactly 15 characters
	char szTruncName[MAX_PATH];
	ExtractFilename(m_szFileName, szTruncName);
	szTruncName[STRLEN_FILE] = 0;
	int nTrunc = (int) strlen(szTruncName);
	while (nTrunc < STRLEN_FILE)
	{
		strcat(szTruncName, " ");
		nTrunc++;
	}

	// Output results row to console
	char szTag[15];
	sprintf(m_szRowBuffer, "%s  ", szTruncName);
	for (int i = 0; i < m_nTags; i++)
	{
		sprintf(szTag, ", %8d", m_pnSumTable[i]);
		strcat(m_szRowBuffer, szTag);
	}
	strcat(m_szRowBuffer, "\n");
//	printf("\n");
}	// CResults::FormatRowBufferTxt


void CResults::ExtractFilename(char* szPath, char* szFile)
{
	char* szPtr = strrchr(szPath, '\\');
	if (szPtr == NULL)
		szPtr = strrchr(szPath, '/');

	if (szPtr == NULL)
		strcpy(szFile, szPath);
	else
		strcpy(szFile, szPtr+1);
}	// CResults::ExtractFilename



/////////////////////////////////////////////////////////////////////////////
// CResultsSQL

CResultsSQL::CResultsSQL(int nTags) : CResults(nTags)
{
	m_dbcon = new mysqlpp::Connection(false);
	m_dbcon.connect (MYSQL_DATABASE, MYSQL_HOST, MYSQL_USER, MYSQL_PASS );
}

CResultsSQL::~CResultsSQL()
{
	m_dbcon.close();
}


bool CResultsSQL::OpenStore(char* szName)
{
	// TODO: Open table szName, begin transaction
	return true;
}	// CResultsSQL::OpenStore


bool CResultsSQL::CommitStore()
{
	// TODO: commit transaction, close table
	return true;
}	// CResultsSQL::CommitStore


void CResultsSQL::NewFile(char* szFile)
{
	// TODO: Start row?
}	// CResultsSQL::NewFile


void CResultsSQL::CloseFile()
{
	// TODO: Write row
}	// CResultsSQL::CloseFile



/////////////////////////////////////////////////////////////////////////////
// CResultsCSV

CResultsCSV::CResultsCSV(int nTags) : CResults(nTags)
{
	m_fp = NULL;
}

CResultsCSV::~CResultsCSV()
{
}


bool CResultsCSV::OpenStore(char* szName)
{
	m_fp = fopen(szName, "wt");
	if (m_fp == NULL)
	{
		printf("CResultsCSV::OpenStore: failed to open %s\n", szName);
		return false;
	}

	return true;
}	// CResultsCSV::OpenStore


bool CResultsCSV::CommitStore()
{
	fclose(m_fp);
	m_fp = NULL;
	return true;
}	// CResultsCSV::CommitStore


void CResultsCSV::CloseFile()
{
	if (strlen(m_szFileName) == 0)
		return;

	FormatRowBufferTxt();
	fprintf(m_fp, "%s", m_szRowBuffer);
}	// CResultsCSV::CloseFile