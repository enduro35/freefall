#include "stdafx.h"


typedef struct _OBJECTHOOKINFO_INFORMATION {          //INLINEHOOKINFO_INFORMATION
	ULONG ulMemoryFunctionBase;    //Ô­Ê¼µØÖ·
	ULONG ulMemoryHookBase;        //HOOK µØÖ·
	CHAR lpszFunction[256];
	CHAR lpszHookModuleImage[256];
	ULONG ulObjectTypeBase;
	ULONG ulHookType;
	CHAR lpszObjectTypeName[256];

} OBJECTHOOKINFO_INFORMATION, *POBJECTHOOKINFO_INFORMATION;

typedef struct _OBJECTHOOKINFO {          //InlineHook
	ULONG ulCount;
	OBJECTHOOKINFO_INFORMATION ObjectHook[1];
} OBJECTHOOKINFO, *POBJECTHOOKINFO;

POBJECTHOOKINFO ObjectHookInfo;

CImageList ObjectHookImg;//Í¼±ê

extern BOOL bIsPhysicalCheck;
extern WCHAR PhysicalFile[260];
extern void SaveToFile(CHAR *lpszBuffer,WCHAR *lpwzFilePath);