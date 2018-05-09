#ifndef LIBHEADERS_H
#define LIBHEADERS_H

struct Rect
{
	unsigned short x1;
	unsigned short y1;
	unsigned short x2;
	unsigned short y2;
};

struct Point
{
	unsigned short x;
	unsigned short y;
};

const unsigned char Welcome = 1;
const unsigned char Password = 2;
const unsigned char GoodPassword = 4;
const unsigned char WrongPassword = 3;
const unsigned char MoreConnection = 6;
const unsigned char CloseConnection = 5;
const unsigned char toServerKeyEvent = 7;
const unsigned char toServerRectEvent = 9;
const unsigned char toServerMouseEvent = 8;
const unsigned char toServerCompressionEvent = 10;
const unsigned char toServerPalleteUpdateEvent = 11;
const unsigned char toServerScreenResolutionEvent = 12;
const unsigned char toServerClientPalleteUpdateEvent = 13;
const unsigned char toClientUpdateRect = 14;
const unsigned char toClientSendingRectBuffer = 15;
const unsigned char RetryPassword = 16;

struct Message {
	unsigned char msgtype;
	unsigned long  msglen;
};

struct RecvMessage
{
	unsigned char msgtype;
	union {
		Rect rect;
		struct {
			Point point;	
			int flags;
		} mouseevent;
		struct {
			int code;
			int flags;
		} keyevent;
		unsigned int msglen;
	} msg;
};

struct  WelcomeMsg {
	unsigned char  isNat; // is computer behin NAT or firewall
	unsigned long  IP; // IP adres in network order
	unsigned int   Port; // My port number
	unsigned char  Compression;
	unsigned char  Encryption;
};

struct  PasswordMsg {
	char password[256];
};

const unsigned int  UpdatehookMessage = 1234;

const unsigned char RAW_COMPRESSION = 1;
const unsigned char ZIP_COMPRESSION = 2;

const unsigned char RAW_ENCRYPTION = 1;
const unsigned char AES_ENCRYPTIOM = 2;


#define MAX_LOADSTRING 100

#include "DSocket.h"
#include "RDesktopCap.h"
#include "RDMutex.h"
#include "RDRegister.h"
#include "RDSemaphore.h"
#include "RDThread.h"
#include "used.h"

#endif