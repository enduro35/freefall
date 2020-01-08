#include "stdafx.h"

#define MAX_DPCTIMER_COUNT 250

typedef struct _MyDpcTimerInfo{
	ULONG  TimerAddress;    //KTIMER賦凳華硊
	ULONG  Period;        //悜遠潔路
	ULONG  DpcAddress;      //DPC賦凳華硊
	ULONG  DpcRoutineAddress;  //瞰最華硊
	char lpszModule[260];
}MyDpcTimerInfo,*PMyDpcTimerInfo;

typedef struct _MyDpcTimer{
	ULONG  ulCount;
	MyDpcTimerInfo MyTimer[1];
}MyDpcTimer,*PMyDpcTimer;

PMyDpcTimer DpcTimer;

//CString DpcTimerNum;
CImageList DpcTimerImg;// 輛最芞梓

CHAR* setClipboardText(CHAR* str);