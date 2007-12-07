
#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <vector>
#include <string>
#include <errno.h>

#include "FileUtil.h"
#include "Results.h"


// Since apparently MySql won't let you make a second query while one is
// still open, we need two connections
mysqlpp::Connection *g_pdbcon1 = NULL;
mysqlpp::Connection *g_pdbcon2 = NULL;

bool  g_bMac = false;
float g_fSumToler;
//int   g_nMaxCoeff = 5;
bool  g_bReportAll = false;
// Collected statistics
int  g_nKeys = 0;
int  g_nRows = 0; //get from SQL?
int  g_nIdentical = 0;
__int64  g_nDataSize = 0;
int* g_pnKeyHits = NULL;
int* g_pnCollisions = NULL;
int* g_pnSumHits = NULL; 
// thrown out because of size mismatch counter
vector<string> g_vStrKeys;

int g_i = 0;

#define MIN_KEY_TOL   1
#define MIN_SUM_TOL   1

#define COL_NAME   0  // column index of file size
#define COL_PATH   1  // column index of file size
#define COL_SIZE   2  // column index of file size
#define COL_KEY1   3  // column index of file size
int     COL_TAG1 = 4; // number of fields preceeding 1st tag


bool g_bReport = true;

/////////////////////////////////////////////////////////////////////////////
// SimFind: main() and subroutines

void ResetCounters(bool bResetArrays)
{
	if (bResetArrays)
	{
		memset(g_pnKeyHits, 0, g_nKeys*sizeof(int));
		memset(g_pnSumHits, 0, g_nKeys*sizeof(int));
		memset(g_pnCollisions, 0, g_nKeys*sizeof(int));
	}
	g_nRows = 0;
	g_nIdentical = 0;
	g_nDataSize = 0;
}


bool AreRowsSameFile(mysqlpp::Row& row1, mysqlpp::Row& row2)
{
	char szPath1[MAX_PATH];
	char szPath2[MAX_PATH];
	sprintf(szPath1, "%s%s", row1.raw_data(1), row1.raw_data(0));
	sprintf(szPath2, "%s%s", row2.raw_data(1), row2.raw_data(0));
	return AreFilesSame(szPath1, szPath2);
}	// AreRowsSameFile


DWORD ComputeDistance(mysqlpp::Row& row1, mysqlpp::Row& row2)
{
	DWORD dwDist = 0;
	int nRows = (int) row1.size();
	for (int i = COL_TAG1; i < nRows; i++)
		dwDist += (DWORD) abs( atoi(row1.raw_data(i)) - atoi(row2.raw_data(i)) );
	return dwDist;
}	// ComputeDistance


void FindSimilarities(FILE* fp, char* szTable, mysqlpp::Row& row, bool bFindForward)
{
	int nKeyIndex = COL_KEY1+g_i;
	DWORD dwKeyVal = (DWORD) _atoi64(row.raw_data(nKeyIndex)); //offset for multikeys
	DWORD dwFileSize = (DWORD) _atoi64(row.raw_data(COL_SIZE));
	DWORD dwKeyTolerance = (int)( ((float)dwFileSize) * g_fSumToler / 100.0f );
	dwKeyTolerance = max(dwKeyTolerance, MIN_KEY_TOL);
	DWORD ub = dwKeyVal + dwKeyTolerance;
	DWORD lb = bFindForward ? dwKeyVal : dwKeyVal - dwKeyTolerance;

	mysqlpp::Query query = g_pdbcon2->query();
	query.reset();
	query << "SELECT * FROM " << szTable << " WHERE ";
	query << g_vStrKeys[g_i].c_str() << " BETWEEN " << lb << " AND " << ub;
//	printf(query.str().c_str());
	mysqlpp::ResUse res = query.use();
	if (!res)
	{
		printf("FindSimilarities: Failed to find similar rows");
		return; //error with query
	}

	mysqlpp::Row rowsim;
	char szFilePath[MAX_PATH];
	char szSimPath[MAX_PATH];
	DWORD dwSimKeyVal;
	sprintf(szFilePath, "%s%s", row.raw_data(COL_PATH), row.raw_data(COL_NAME));
	bool bAddedHeader = false;
	try
	{
		while (rowsim = res.fetch_row())
		{
			sprintf(szSimPath, "%s%s", rowsim.raw_data(COL_PATH), rowsim.raw_data(COL_NAME));
			dwSimKeyVal = (DWORD) _atoi64(rowsim.raw_data(nKeyIndex));
			// NOTE: Our SQL query doesn't filter out the same file.
			// It also double-counts rows with same key.  This weeds those out.
			// Hopefully, this works.
			if ( (dwKeyVal == dwSimKeyVal) && (strcmp(szSimPath, szFilePath) <= 0) )
				continue;

			g_pnKeyHits[g_i]++;
			
			// Count hash key collisions
			// TODO: Do this BEFORE or AFTER other weed-out checks?
			if (dwKeyVal == dwSimKeyVal)
				g_pnCollisions[g_i]++;

			// Discard files that differ more than 50% in size
			// Perhaps this should be tolerance% of the file?
			// ***** Add a global to count this 
			DWORD dwSimSize = (DWORD) _atoi64(rowsim.raw_data(COL_SIZE));
			if ( (dwSimSize > 3*dwFileSize/2) || (dwFileSize > 3*dwSimSize/2) ) 
				continue;
				
			// Discard files with too large a sum table distance
			DWORD dwDist = ComputeDistance(row, rowsim);
			DWORD dwMaxSumDist = (int) ( ((float)(dwFileSize+dwSimSize) * g_fSumToler / 200.0f) );
			if (dwDist > max(dwMaxSumDist, MIN_SUM_TOL))
				continue;

			// Files are similar... report
			g_pnSumHits[g_i]++;
			
			// Print current "row" for the first time
			if (!bAddedHeader && g_bReportAll)
			{
				fprintf(fp, "%s\n", szFilePath);
				bAddedHeader = true;
			}

			// Print found similarity
			if ( false && (dwDist == 0) && AreRowsSameFile(row, rowsim) ) //don't print identical
			{
				if (g_bReportAll)
					fprintf(fp, "  <same> %s\n", szFilePath);
				/* // This code deletes identical files, should that be desired
				char szFilePath[MAX_PATH];
				sprintf(szFilePath, "%s%s", row.raw_data(1), row.raw_data(0));
				remove(szFilePath);
				*/
				g_nIdentical++;
			}
			else if (g_bReportAll)
				fprintf(fp, "  %6d %s\n", dwDist, szSimPath);
		}
	}
	catch (const mysqlpp::EndOfResults& ){} // thrown when no more rows

	if (bAddedHeader)
		fprintf(fp, "\n");
}	// FindSimilarities


void FindSimilaritiesForOne(FILE* fp, char* szTable, char* szFilePath)
{
	char szFilename[MAX_PATH];
	char szDirname[MAX_PATH];
	ExtractFilename(szFilePath, szFilename);
	ExtractDirname(szFilePath, szDirname);

	// Retrieve all rows
	mysqlpp::Query query = g_pdbcon1->query();
	query << "SELECT * FROM " << szTable;
	query << " WHERE filename= '" << szFilename << "' AND";
	query << " directoryname= '" << szDirname << "'";
	mysqlpp::ResUse res = query.use();
	if (!res)
	{
		printf("FindSimilaritiesForOne: Failed to query row");
		return; //error with query
	}

	mysqlpp::Row row;
	try
	{
		while (row = res.fetch_row())
			FindSimilarities(fp, szTable, row, false);
	}
	catch (const mysqlpp::EndOfResults& ){} // thrown when no more rows
}	// FindSimilaritiesForOne


void FindSimilaritiesForAll(FILE* fp, char* szTable)
{
	// Retrieve all rows
	mysqlpp::Query query = g_pdbcon1->query();
	query << "SELECT * FROM " << szTable << " ORDER BY " << g_vStrKeys[g_i].c_str() << ", filename";
	mysqlpp::ResUse res = query.use();
	if (!res)
	{
		printf("FindSimilaritiesForAll: Failed to query rows");
		return; //error with query
	}

	mysqlpp::Row row;
	try
	{
		while (row = res.fetch_row())
		{
			FindSimilarities(fp, szTable, row, true);
			if ( (g_nRows % 100 == 0) && g_nRows )
				printf(" %d", g_nRows);
			g_nRows++; // replace with sql query?
			g_nDataSize += (DWORD) _atoi64(row.raw_data(COL_SIZE));
		}
	}
	catch (const mysqlpp::EndOfResults& ){} // thrown when no more rows
}	// FindSimilaritiesForAll


void GetKeyNames(char* szTable)
{
	mysqlpp::Query query = g_pdbcon1->query();
	query << "DESCRIBE " << szTable;
	mysqlpp::ResUse res = query.use();
	mysqlpp::Row row;
	try
	{
		for (int i = 0; i < COL_TAG1; i++)
		{
			row = res.fetch_row();
			if (i < COL_KEY1)
				continue;
			g_vStrKeys.push_back(row.raw_data(0));
		}
	}
	catch (const mysqlpp::EndOfResults& ){} // thrown when no more rows
}	// GetKeyNames


int main(int argc, char* const argv[])
{
	int i;
	char szSimFindIni[MAX_PATH];
	char szTable[MAX_PATH];
	char szOutFile[MAX_PATH];
	char szStatFile[MAX_PATH];
	char szMatchFile[MAX_PATH];
	
#ifdef __APPLE__
	g_bMac = true;
#endif

	// Read specs for run out of INI file
	if (argc > 1)
		strcpy(szSimFindIni, argv[1]);
	else if (g_bMac)
		sprintf(szSimFindIni, "/Users/ipye/Desktop/cmps221/_hashproj/sub/simhash/MacFind.ini");
	else
		sprintf(szSimFindIni, "WinFind.ini");


	FILE* fp = fopen(szSimFindIni, "rt");
	if (fp == NULL)
	{
		printf("main: failed to load %s\n", szSimFindIni);
		return 1;
	}

	if ((fscanf(fp, "Table=%s\n", szTable) != 1) ||
		(fscanf(fp, "Keys=%d\n", &g_nKeys) != 1) ||
		(fscanf(fp, "OutFile=%s\n", szOutFile) != 1) ||
		(fscanf(fp, "StatFile=%s\n", szStatFile) != 1) ||
		(fscanf(fp, "MatchFile=%s\n", szMatchFile) != 1) ||
		(fscanf(fp, "Tolerance=%f\n", &g_fSumToler) != 1) )
	{
		printf("main: Invalid INI file '%s'\n", szSimFindIni);
		return 1;
	}
	fclose(fp);
	COL_TAG1 = COL_KEY1 + g_nKeys;
	g_pnKeyHits = (int*) calloc(g_nKeys, sizeof(int));
	g_pnCollisions = (int*) calloc(g_nKeys, sizeof(int));
	g_pnSumHits = (int*) calloc(g_nKeys, sizeof(int));

	// Open output file for writing
	if (g_bReportAll)
	{
		fp = fopen(szOutFile, "wt");
		if (fp == NULL)
		{
			printf("main: failed to open '%s'\n", szOutFile);
			return false;
		}
	}
	
	// Open statt file for writing
	FILE* fp2 = fopen(szStatFile, "wt");
	if (fp2 == NULL)
	{
		fclose(fp);
		printf("main: failed to open '%s'\n", szStatFile);
		return false;
	}

	// Open two needed db connections
	g_pdbcon1 = new mysqlpp::Connection(mysqlpp::use_exceptions);
	g_pdbcon2 = new mysqlpp::Connection(mysqlpp::use_exceptions);
	g_pdbcon1->connect(MYSQL_DATABASE, MYSQL_HOST, MYSQL_USER, MYSQL_PASS);
	g_pdbcon2->connect(MYSQL_DATABASE, MYSQL_HOST, MYSQL_USER, MYSQL_PASS);

	// Print header for stats file
	GetKeyNames(szTable);
	fprintf(fp2, "# Table = '%s'\n", szTable);
	fprintf(fp2, "# TOLERANCE ");
	for (i = 0; i < g_nKeys; i++)
		fprintf(fp2, "%s SUMHITS ", g_vStrKeys[i].c_str());
	fprintf(fp2, "\n");
	//fprintf(fp2, "SUMHITS\n");

	// Loop through tolerance values (given in percents)
	float afTols[] = { 0.01f }; //{ 3.2f, 1.0f, 0.32f, 0.1f, 0.032f, 0.01f };
	for (i = 0; i < sizeof(afTols)/sizeof(float); i++) 
	{
		ResetCounters(true);
		g_fSumToler = afTols[i];
		fprintf(fp2, "%8f  ", g_fSumToler);
		
		for (g_i = 0; g_i < g_nKeys; g_i++)
		{
			ResetCounters(false);
			// Find file similarities
			if (strlen(szMatchFile) > 1)
				FindSimilaritiesForOne(fp, szTable, szMatchFile);
			else
				FindSimilaritiesForAll(fp, szTable);
			fprintf(fp2, "%8d ", g_pnKeyHits[g_i]);
			fprintf(fp2, "%8d ", g_pnSumHits[g_i]);
			
			if (g_bReport)
				printf("Tolerance = %f, Key = : %d\n", g_fSumToler, g_i+1);
		}
		fprintf(fp2, "\n");
	}
	
	fprintf(fp2, "\n# Rows = %d\n", g_nRows);
	fprintf(fp2, "# Total Size = %d\n", g_nDataSize);
	fprintf(fp2, "# Identical = %d\n", g_nIdentical);
	for (i = 0; i < g_nKeys; i++)
		fprintf(fp2, "# '%s' Collisions = %d\n", g_vStrKeys[i].c_str(), g_pnCollisions[i]);

	fclose(fp);
	fclose(fp2);
	delete g_pdbcon1;
	delete g_pdbcon2;
	free(g_pnKeyHits);
	free(g_pnCollisions);
	return 0;
}
