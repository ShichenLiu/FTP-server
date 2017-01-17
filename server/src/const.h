#include <string.h>
//#define printf(...) fprintf(stderr,__VA_ARGS__)
//#define printf(...) 

const char* my_IP = "127,0,0,1";
const char* S150 = "150 \r\n";
const char* S200 = "200 \r\n";
const char* S200P = "200 Port mode established.\r\n";
const char* S200T = "200 Type set to I.\r\n";
const char* S220 = "220 Anonymous FTP server ready.\r\n";
const char* S226 = "226 Store successful.\r\n";
const char* S226S = "226 Store successful.\r\n";
const char* S226R = "226 Retrive successful.\r\n";
const char* S226L = "226 List finished.\r\n";
const char* S227 = "227 ";
const char* S257 = "257 ";
const char* S250 = "250 Okay\r\n";
const char* S331 = "331 Guest login ok, send your complete e-mail address as password.\r\n";
const char* S230s[] = {"230-\r\n", "230- Welcome to\r\n", "230- School of Software\r\n", "230- FTP Archives at nowhere\r\n", "230-\r\n", "230 Guest login ok, access restrictions apply.\r\n"};
const char* S215 = "215 UNIX Type: L8\r\n";
const char* S221 = "221 Bye!\r\n";
const char* S350 = "350 \r\n";
const char* S451 = "451 Cannot open file.\r\n";
const char* S503 = "503 RNFR first\r\n";
const char* S550 = "550 Failed\r\n";
const char* S550C = "550 no such file or directory\r\n";


