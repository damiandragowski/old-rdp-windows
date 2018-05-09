// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the RDCRYPT_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// RDCRYPT_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#ifdef RDCRYPT_EXPORTS
#define RDCRYPT_API __declspec(dllexport)
#else
#define RDCRYPT_API __declspec(dllimport)
#endif

// This class is exported from the RDCrypt.dll
#include "aes.h"
#include "aes.c"
#include <string>
using namespace std;

typedef struct
{
    unsigned long int total[2];
    unsigned long int state[5];
    unsigned char     buffer[64];
}sha1_context;

class RDCRYPT_API SHA1
{
public:
	static void crypt(unsigned char * input, unsigned int length, char * buffer);
};

class RDCRYPT_API AES
{
public:
	static void encrypt(unsigned char * input, unsigned char * output,unsigned char * key, int nbits);
	static void decrypt(unsigned char * input, unsigned char * output,unsigned char * key, int nbits);
};

