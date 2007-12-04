
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

bool g_bMac = false;
int  g_nSumToler = 5;
int  g_nMaxCoeff = 5;
bool g_bReportAll = false;
// Collected statistics
int  g_nRows = 0;
int  g_nKeyHits = 0;
int  g_nSumHits = 0;
int  g_nIdentical = 0;
int  g_nDataSize = 0;
int  g_nCollisions = 0;

#define MIN_KEY_TOL   1
#define MIN_SUM_TOL   1

#define COL_SIZE   2 // column index of file size
#define COL_HASH   3 // column index of file size
#define COL_TAG1   4 // number of fields preceeding 1st tag

/////////////////////////////////////////////////////////////////////////////
// SimFind: main() and subroutines

void ResetCounters()
{
	g_nRows = 0;
	g_nKeyHits = 0;
	g_nSumHits = 0;
	g_nIdentical = 0;
	g_nDataSize = 0;
	g_nCollisions = 0;
}


bool AreRowsSameFile(mysqlpp::Row& row1, mysqlpp::Row& row2)
{
	char szPath1[MAX_PATH];
	char szPath2[MAX_PATH];
	sprintf(szPath1, "%s%s", row1.raw_data(1), row1.raw_data(0));
	sprintf(szPath2, "%s%s", row2.raw_data(1), row2.raw_data(0));
	return AreFilesSame(szPath1, szPath2);
}	// AreRowsSameFile


int ComputeDistance(mysqlpp::Row& row1, mysqlpp::Row& row2)
{
	int nDist = 0;
	int nRows = row1.size();
	for (int i = COL_TAG1; i < nRows; i++)
		nDist += abs( atoi(row1.raw_data(i)) - atoi(row2.raw_data(i)) );
	return nDist;
}	// ComputeDistance


void FindSimilarities(FILE* fp, char* szTable, mysqlpp::Row& row, bool bFindForward)
{
	int nKey = atoi(row.raw_data(COL_HASH));
	int nFileSize = atoi(row.raw_data(COL_SIZE));
	int nKeyTolerance = g_nMaxCoeff * nFileSize / g_nSumToler;
	nKeyTolerance = max(nKeyTolerance, MIN_KEY_TOL);
	int ub = nKey + nKeyTolerance;
	int lb = bFindForward ? nKey : nKey - nKeyTolerance;

	mysqlpp::Query query = g_pdbcon2->query();
	query.reset();
	query << "SELECT * FROM " << szTable << " WHERE ";
	query << "hashkey BETWEEN " << lb << " AND " << ub;
//	printf(query.str().c_str());
	mysqlpp::ResUse res = query.use();
	if (!res)
	{
		printf("FindSimilarities: Failed to find similar rows");
		return; //error with query
	}

	mysqlpp::Row rowsim;
	bool bAddedHeader = false;
	try
	{
		while (rowsim = res.fetch_row())
		{
			// Same row
			if ( (strcmp(row.raw_data(0), rowsim.raw_data(0)) == 0) && 
					(strcmp(row.raw_data(1), rowsim.raw_data(1)) == 0) )
				continue;

			g_nKeyHits++;
			int nDist = ComputeDistance(row, rowsim);
			// Discard files that differ more than 50% in size
			int nSimSize = atoi(rowsim.raw_data(COL_SIZE));
			if ( (nSimSize > 3*nFileSize/2) || (nFileSize > 3*nSimSize/2) )
				continue;
			// Discard files with too large a sum table distance
			int nMaxSumDist = (nFileSize+nSimSize)/(g_nSumToler*2);
			if (nDist > max(nMaxSumDist, MIN_SUM_TOL))
				continue;

			// Files are similar... report
			g_nSumHits++;
			// Print current "row" for the first time
			if (!bAddedHeader && g_bReportAll)
			{
				fprintf(fp, "%s\n", row.raw_data(0));
				bAddedHeader = true;
			}
			
			// Count hash key collisions
			if (strcmp(row.raw_data(COL_HASH), rowsim.raw_data(COL_HASH)) == 0)
				g_nCollisions++;

			// Print found similarity
			if ( (nDist == 0) && AreRowsSameFile(row, rowsim) )
			{
				if (g_bReportAll)
					fprintf(fp, "  <same> %s\n", rowsim.raw_data(0));
				/* // This code removes identical files, should that be desired
				char szFilePath[MAX_PATH];
				sprintf(szFilePath, "%s%s", row.raw_data(1), row.raw_data(0));
				remove(szFilePath);
				*/
				g_nIdentical++;
			}
			else if (g_bReportAll)
				fprintf(fp, "  %6d %s\n", nDist, rowsim.raw_data(0));
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
	query << "SELECT * FROM " << szTable << " ORDER BY hashkey, filename";
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
			g_nRows++;
			g_nDataSize += atoi(row.raw_data(COL_SIZE));
		}
	}
	catch (const mysqlpp::EndOfResults& ){} // thrown when no more rows
}	// FindSimilaritiesForAll


int main(int argc, char* const argv[])
{
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
		(fscanf(fp, "OutFile=%s\n", szOutFile) != 1) ||
		(fscanf(fp, "StatFile=%s\n", szStatFile) != 1) ||
		(fscanf(fp, "MatchFile=%s\n", szMatchFile) != 1) ||
		(fscanf(fp, "Tolerance=%d\n", &g_nSumToler) != 1) ||
		(fscanf(fp, "MaxTagCoeff=%d", &g_nMaxCoeff) != 1) )
	{
		printf("main: Invalid INI file '%s'\n", szSimFindIni);
		return 1;
	}
	fclose(fp);

	// Open output file for writing
	fp = fopen(szOutFile, "wt");
	if (fp == NULL)
	{
		printf("main: failed to open '%s'\n", szOutFile);
		return false;
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

	fprintf(fp2, "# Table = '%s'\n", szTable);	
	fprintf(fp2, "# TOLERANCE  KEYHITS   SUMHITS \n");
	int anTols[] = { 10, 25, 50, 100, 250, 500, 1000, 2500 };
	for (int i = 0; i < 8; i++)
	{
		ResetCounters();
		g_nSumToler = anTols[i];
		// Find file similarities
		if (strlen(szMatchFile) > 1)
			FindSimilaritiesForOne(fp, szTable, szMatchFile);
		else
			FindSimilaritiesForAll(fp, szTable);

		fprintf(fp2, "%8d  %8d  %8d\n", g_nSumToler, g_nKeyHits, g_nSumHits);
	}
	
	fprintf(fp2, "\n# Rows = %d\n", g_nRows);
	fprintf(fp2, "# Total Size = %d\n", g_nDataSize);
	fprintf(fp2, "# Collisions = %d\n", g_nCollisions);
	fprintf(fp2, "# Identical = %d\n", g_nIdentical);


	fclose(fp);
	fclose(fp2);
	delete g_pdbcon1;
	delete g_pdbcon2;
	return 0;
}
