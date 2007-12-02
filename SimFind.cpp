
#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <vector>
#include <string>

#include <errno.h>

#include "SimHash.h"
#include "Results.h"

using namespace std;

// Since apparently MySql won't let you make a second query while one is
// still open, we need two connections
mysqlpp::Connection *g_pdbcon1 = NULL;
mysqlpp::Connection *g_pdbcon2 = NULL;

bool g_bMac = false;
int  g_nKeyDelta = 5;
int  g_nSumDelta = 5;

#define TAG1   4 // number of fields preceeding 1st tag

/////////////////////////////////////////////////////////////////////////////
// SimFind: main() and subroutines
/*
void GetSumVector(mysqlpp::Row& row, std::vector<int> &vSums)
{
	vSums.clear();
	int nRows = row.size();
	for (int i = 3; i < nRows; i++)
		vSums.push_back( atoi(row.raw_data(i)) );
}	// GetSumVector
*/

int ComputeDistance(mysqlpp::Row& row1, mysqlpp::Row& row2)
{
	int nDist = 0;
	int nRows = row1.size();
	for (int i = TAG1; i < nRows; i++)
		nDist += abs( atoi(row1.raw_data(i)) - atoi(row2.raw_data(i)) );
	return nDist;
}	// ComputeDistance


void FindSimilarities(FILE* fp, char* szTable, mysqlpp::Row& row, bool bFindForward)
{
	int nKey = atoi(row.raw_data(TAG1-1));
	int ub = nKey + g_nKeyDelta;
	int lb = bFindForward ? nKey : nKey - g_nKeyDelta;

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

			int nDist = ComputeDistance(row, rowsim);
			if (nDist <= g_nSumDelta)
			{
				// Print current "row" for the first time
				if (!bAddedHeader)
				{
					fprintf(fp, "%s\n", row.raw_data(0));
					bAddedHeader = true;
				}

				// Print found similarity
				fprintf(fp, "  %5d %s\n", nDist, rowsim.raw_data(0));
			}
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
	CResults::ExtractFilename(szFilePath, szFilename);
	CResults::ExtractDirname(szFilePath, szDirname);

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
			FindSimilarities(fp, szTable, row, true);
	}
	catch (const mysqlpp::EndOfResults& ){} // thrown when no more rows
}	// FindSimilaritiesForAll


int main(int argc, char* const argv[])
{
	char szSimFindIni[MAX_PATH];
	char szTable[MAX_PATH];
	char szOutFile[MAX_PATH];
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
		(fscanf(fp, "MatchFile=%s\n", szMatchFile) != 1) ||
		(fscanf(fp, "Tolerance=%d", &g_nSumDelta) != 1) )
	{
		printf("main: Invalid INI file '%s'\n", szSimFindIni);
		return 1;
	}
	fclose(fp);
	g_nKeyDelta = g_nSumDelta;

	// Open output file for writing
	fp = fopen(szOutFile, "wt");
	if (fp == NULL)
	{
		printf("main: failed to open '%s'\n", szOutFile);
		return false;
	}

	// Open two needed db connections
	g_pdbcon1 = new mysqlpp::Connection(mysqlpp::use_exceptions);
	g_pdbcon2 = new mysqlpp::Connection(mysqlpp::use_exceptions);
	g_pdbcon1->connect(MYSQL_DATABASE, MYSQL_HOST, MYSQL_USER, MYSQL_PASS);
	g_pdbcon2->connect(MYSQL_DATABASE, MYSQL_HOST, MYSQL_USER, MYSQL_PASS);

	// Find file similarities
	if (strlen(szMatchFile) > 1)
		FindSimilaritiesForOne(fp, szTable, szMatchFile);
	else
		FindSimilaritiesForAll(fp, szTable);

	fclose(fp);
	delete g_pdbcon1;
	delete g_pdbcon2;
	return 0;
}


