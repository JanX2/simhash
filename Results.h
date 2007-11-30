#ifndef __RESULTS_H__
#define __RESULTS_H__

#include <mysql++.h>
#include <transaction.h>

#include "SimHash.h"
// MYSQL PARAMETERS
#define MYSQL_HOST		"127.0.0.1"
#define MYSQL_DATABASE  "simhash"
#define MYSQL_USER		"root"
#define MYSQL_PASS		"bilb0"	

class CTags;

/////////////////////////////////////////////////////////////////////////////
// CResults
/* This class collects results of binary string matches on one file at a time.
It serves as a base class for any results-storage medium we care to implement.
OpenStore() would open the output file (db table, CSV file, etc).  NewFile()
starts a new "row" in the output and resets sum counts to zero.  IncrTag()
increments the indicated tag count on the current file.
The default implementation of this class writes results to console.
*/

class CResults
{
public:
	CResults(int nTags);
	~CResults();

	virtual bool OpenStore(char* , CTags* ) { return true; }
	virtual bool CommitStore() { return true; }

	virtual void NewFile(char* szFile);
	virtual void CloseFile();
	virtual void IncrTag(int nTag);
	virtual bool CheckValidDir(char* szDir) { return true; }

	static void ExtractFilename(char* szPath, char* szOutFile);
	static void ExtractDirname(char* szPath, char* szOutDir);

protected:
	int ComputeHashKey(CTags* pTags);
	void FormatRowBufferTxt();

	char   m_szStoreName[MAX_PATH];
	char   m_szFilePath[MAX_PATH];

	int    m_nTags;
	DWORD* m_pnSumTable;
	char*  m_szRowBuffer;  // for writing text-formatted data
};


/////////////////////////////////////////////////////////////////////////////
// CResultsSQL
/* This derivation of CResults dumps the results into a SQL table */

class CResultsSQL : public CResults
{
public:
	CResultsSQL(int nTags);
	~CResultsSQL();

	bool OpenStore(char* szName, CTags* pTags);
	bool CommitStore();
	void NewFile(char* szFilepath);
	void CloseFile();
	bool CheckValidDir(char* szDir);

protected:
	mysqlpp::Connection *m_pdbcon;
	mysqlpp::Transaction* m_pTransaction;
	CTags* m_pTags;
	// TODO: db info
};


/////////////////////////////////////////////////////////////////////////////
// CResultsCSV
/* This derivation of CResults dumps the results into a CSV file
*/

class CResultsCSV : public CResults
{
public:
	CResultsCSV(int nTags);
	~CResultsCSV();

	bool OpenStore(char* szName, CTags* );
	bool CommitStore();
	void CloseFile();

protected:
	FILE* m_fp;
};



#endif // __RESULTS_H__
