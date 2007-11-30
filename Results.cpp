
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
	memset(m_szFilePath, 0, MAX_PATH);

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
	strcpy(m_szFilePath, szFile);
	memset(m_pnSumTable, 0, m_nTags*sizeof(DWORD));
}	// CResults::NewFile


void CResults::IncrTag(int nTag)
{
	m_pnSumTable[nTag]++;
}	// CResults::IncrTag


void CResults::CloseFile()
{
	if (strlen(m_szFilePath) == 0)
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
	ExtractFilename(m_szFilePath, szTruncName);
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
		strcpy(szOutFile, &(szPtr[1]));
}	// CResults::ExtractFilename

void CResults::ExtractDirname(char* szPath, char* szOutDir)
{
	strcpy(szOutDir, szPath);
	char* szPtr = strrchr(szOutDir, '\\');
	if (szPtr == NULL)
		szPtr = strrchr(szOutDir, '/');

	if (szPtr != NULL)
		szPtr[0] = 0;
}	// CResults::ExtractDirname



/////////////////////////////////////////////////////////////////////////////
// CResultsSQL

CResultsSQL::CResultsSQL(int nTags) : CResults(nTags)
{
	m_pdbcon = new mysqlpp::Connection(mysqlpp::use_exceptions);
	m_pdbcon->connect (MYSQL_DATABASE, MYSQL_HOST, MYSQL_USER, MYSQL_PASS );
	m_pTags = NULL;
	m_pTransaction = NULL;
	m_szFilePath[0] = 0;
}

CResultsSQL::~CResultsSQL()
{
	delete m_pTransaction;
	m_pdbcon->close();
}


bool CResultsSQL::OpenStore(char* szName, CTags* pTags)
{
	// If the connection was not properly established with the database, error
	if ( !m_pdbcon || !m_pdbcon->connected() )
		return false;

	// Initialize members
	strcpy(m_szStoreName, szName);
	m_pTags = pTags;

	//check if the table called szName exists
	bool tableExists = false;
	mysqlpp::Query query = m_pdbcon->query();
	query << "show tables";
	mysqlpp::ResUse res = query.use();
	if (!res)
		return false; //error with query

	mysqlpp::Row row;
	try
	{
		while (row = res.fetch_row())
		{
			//printf(res.raw_value().c_str());
			const string& currentTable = row.raw_string(0);
			if (!strcmp(currentTable.c_str(), szName))
				tableExists = true;
		}
	}
	catch (const mysqlpp::EndOfResults& ){} // thrown when no more rows

	//if the table does not exist, then create it and populate the columns from pTags.dwOrigTag
	if (!tableExists)
	{
		query.reset();
		query << "create table " << szName << " (filename text, directoryname text, hashkey int" ;
		for (int i = 0; i < m_nTags; i++)
			query << ", t" << m_pTags->GetTag(i)->dwOrigTag << " int";
		query << ")";

		//printf(query.str().c_str());
		
		if (!query.execute().success)
			return false;
	}

	//start a new transaction object
	m_pTransaction = new mysqlpp::Transaction(*m_pdbcon);

	return (m_pTransaction != NULL);
}	// CResultsSQL::OpenStore


bool CResultsSQL::CommitStore()
{
	try
	{
		m_pTransaction->commit();
	}
	catch (const mysqlpp::Exception& er)
	{
		printf("CResultsSQL::CommitStore() failed: %s\n", er);
		return false;
	}
	return true;
}	// CResultsSQL::CommitStore


void CResultsSQL::NewFile(char* szFile)
{	
	//initialize currentfilepath variable
	strcpy(m_szFilePath, szFile);
	CResults::NewFile(szFile);
}	// CResultsSQL::NewFile


void CResultsSQL::CloseFile()
{
	char szFilename[MAX_PATH];
	char szDirname[MAX_PATH];
	ExtractFilename(m_szFilePath, szFilename);
	ExtractDirname(m_szFilePath, szDirname);
	
	mysqlpp::Query query = m_pdbcon->query();
	query << "insert into " << m_szStoreName;
	query << " values ('" << szFilename << "', '" << szDirname << "', ";
	query << ComputeHashKey(m_pTags);
	for (int i = 0; i < m_nTags; i++)
		query << ", " << m_pnSumTable[i];
	query << ")";
	//store row to query contents of DWORD* m_pnSumTable; and m_szFilePath;
	query.execute();
}	// CResultsSQL::CloseFile


// Returns 'false' if this directory is already associated with rows in the db
bool CResultsSQL::CheckValidDir(char* szDirname)
{
	bool dirExists = false;
	mysqlpp::Query query = m_pdbcon->query();
	query.reset();
	query << "select directoryname from " << m_szStoreName;
	query << " where directoryname= '" << szDirname << "'";
	mysqlpp::ResUse res = query.use();
	if (res)
	{
		mysqlpp::Row row;
		try
		{
			if (row = res.fetch_row())
			{
				dirExists = true;
				printf("'%s' has already been scanned into this table.\n", szDirname);
			}
		}
		catch (const mysqlpp::EndOfResults& ) {} // thrown when no more rows
	}
	return !dirExists;
}	// CResultsSQL::CheckValidDir



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
	strcpy(m_szStoreName, szName);
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
	if (strlen(m_szFilePath) == 0)
		return;

	FormatRowBufferTxt();
	fprintf(m_fp, "%s", m_szRowBuffer);
}	// CResultsCSV::CloseFile