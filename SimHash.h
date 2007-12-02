#ifndef __SIM_HASH_H__
#define __SIM_HASH_H__

#include "FileUtil.h"

typedef unsigned long       DWORD;
typedef int                 BOOL;
typedef unsigned char       BYTE;
typedef unsigned short      WORD;

#ifndef max
#define max(x,y)   ((x) < (y) ? (y) : (x))
#define min(x,y)   ((x) > (y) ? (y) : (x))
#endif

typedef struct BINTAG
{
	DWORD dwTag;     // 32 (or fewer)-bit tag to be compared
	DWORD dwMask;    // encodes bit legnth in a useful way
	int   nWeight;   // coefficient in linear combo of hit sums
	int   nLength;   // bit length straight up
	int   nIgnore;   // number of future bits to ignore
	DWORD dwOrigTag; // unmodified tag value
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




#endif // __SIM_HASH_H__