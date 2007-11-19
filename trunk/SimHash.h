ifndef __SIM_HASH_H__
#define __SIM_HASH_H__
 
#ifdef __APPLE__
#include <dirent.h>
#else
#include <windows.h>
#endif
 
 
typedef unsigned long       DWORD;
typedef int                 BOOL;
typedef unsigned char       BYTE;
typedef unsigned short      WORD;
 
#ifndef max
#define max(x,y)   ((x) < (y) ? (y) : (x))
#define min(x,y)   ((x) > (y) ? (y) : (x))
#endif
 
#ifndef MAX_PATH
#define MAX_PATH   260
#endif
 
typedef struct BINTAG
{
	DWORD dwTag;     // 32 (or fewer)-bit tag to be compared
	DWORD dwMask;    // encodes bit legnth in a useful way
	int   nWeight;   // coefficient in linear combo of hit sums
	int   nLength;   // bit length straight up
	int   nIgnore;   // number of future bits to ignore
}	BINTAG;
 
 
typedef struct SUMTABLE
{
	char   szFileName[MAX_PATH];
	int    nTags;
	DWORD* pnSumTable;
	DWORD  dwKey;                  // maybe not used
}	SUMTABLE;
 
 
/////////////////////////////////////////////////////////////////////////////
// CTags
/* This class reads the tag file, manages tag info, and deals with the
   matching of binary strings.
*/ 
 
class CTags
{
public:
	CTags(char* szTagFile);
	~CTags();
 
	bool    IsBigEndian()    { return m_bBigEndian; }
	int     GetTagCount()    { return m_nTags; }
	BINTAG* GetTag(int nTag) { return &(m_pTags[nTag]); }
	int     MatchBitString(DWORD dwString);
 
protected:
	bool ReadTagFile(char* szFilePath);
	void FixTag(BINTAG* pTag);
	bool Compare(DWORD s1, DWORD s2, DWORD dwMask);
 
	BINTAG* m_pTags;
	int     m_nTags;
	bool    m_bBigEndian;
};
 
 
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
 
	virtual bool OpenStore(char* szName) { return true; }
	virtual bool CommitStore() { return true; }
 
	virtual void NewFile(char* szFile);
	virtual void IncrTag(int nTag);
 
protected:
	virtual void CloseFile();
	int ComputeHashKey(CTags* pTags);
	static void ExtractFilename(char* szPath, char* szFile);
 
	char   m_szStoreName[MAX_PATH];
	char   m_szFileName[MAX_PATH];
 
	int    m_nTags;
	DWORD* m_pnSumTable;
};
 
 
/////////////////////////////////////////////////////////////////////////////
// CResultsSQL
/* This derivation of CResults dumps the results into a SQL table
CAITLIN: Fill in these functions
*/
 
class CResultsSQL : public CResults
{
	CResultsSQL(int nTags);
	~CResultsSQL();
 
	bool OpenStore(char* szName);
	bool CommitStore();
	void NewFile(char* szFile);
 
protected:
	void CloseFile();
	// TODO: db info
};
 
 
#endif // __SIM_HASH_H__
