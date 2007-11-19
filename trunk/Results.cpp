
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


void CResults::ExtractFilename(char* szPath, char* szOutFile)
{
	char* szPtr = strrchr(szPath, '\\');
	if (szPtr == NULL)
		szPtr = strrchr(szPath, '/');

	if (szPtr == NULL)
		strcpy(szOutFile, szPath);
	else
		strcpy(szOutFile, szPtr+1);
}	// CResults::ExtractFilename



/////////////////////////////////////////////////////////////////////////////
// CResultsSQL

//a function which creates a table given some tags, or returns the matching one
//a funciton which adds columns to a table

CResultsSQL::CResultsSQL(int nTags) : CResults(nTags)
{
	m_dbcon = new mysqlpp::Connection(false);
	m_dbcon.connect (MYSQL_DATABASE, MYSQL_HOST, MYSQL_USER, MYSQL_PASS );
	m_pTags = NULL;
}

CResultsSQL::~CResultsSQL()
{
	m_dbcon.close();
}


bool CResultsSQL::OpenStore(char* szName, CTags* pTags)
{
	if (!m_dbcon) return false;
	
	m_pTags = pTags;
	
	//open table szName 
	// if it does not exist, create it and use the pTags.dwOrigTag as column names
	// TODO: begin transaction
	
	return true;
	
}	// CResultsSQL::OpenStore


bool CResultsSQL::CommitStore()
{
	// TODO: commit transaction, close table
	return true;
}	// CResultsSQL::CommitStore


void CResultsSQL::NewFile(char* szFile)
{
	CResults::NewFile(szFile);
}	// CResultsSQL::NewFile


void CResultsSQL::CloseFile()
{
	//store row to query contents of DWORD* m_pnSumTable; and m_szFileName;
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


bool CResultsCSV::OpenStore(char* szName, CTags*)
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