// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the COMPRESSION_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// COMPRESSION_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#ifdef COMPRESSION_EXPORTS
#define COMPRESSION_API __declspec(dllexport)
#else
#define COMPRESSION_API __declspec(dllimport)
#endif


#define ZEXTERN extern __declspec(dllexport)

#include "zlib.h"

// This class is exported from the Compression.dll
/*
class COMPRESSION_API CCompression {
public:
	CCompression(void);
	// TODO: add your methods here.
};
*/

