#include "DpcTimer.h"

/************************************************************************
* 函数名称:KdGetDebuggerDataBlock
* 功能描述:获取CPU域控制块KPCR中的KDDEBUGGER_DATA64
* 参数列表:
* 返回 值:PKDDEBUGGER_DATA64
*************************************************************************/
PKDDEBUGGER_DATA64 KdGetDebuggerDataBlock()
{
	PKDDEBUGGER_DATA64 pKDDebuggerData;
	PDBGKD_GET_VERSION64 KdVersionBlock;

	KeSetSystemAffinityThread(1);  //使当前线程运行在第一个处理器上，因为只有第一个处理器的值才有效
	_asm
	{
		mov eax,fs:[0x1c] //SelfPcr就是指向fs指向的_kpcr
		mov eax,[eax+0x34]//kpcr+0x34->KdVersionBlock
		mov KdVersionBlock,eax
	}
	KeRevertToUserAffinityThread();//恢复线程运行的处理器

	if (!MmIsAddressValidEx(KdVersionBlock))
	{
		return NULL;
	}
	pKDDebuggerData =(PKDDEBUGGER_DATA64)(*(PULONG)KdVersionBlock->DebuggerDataList);
	if (!MmIsAddressValidEx(pKDDebuggerData))
	{
		return NULL;
	}
	if (pKDDebuggerData->Header.OwnerTag != KDBG_TAG)
	{
		KdPrint(("(KdGetDebuggerDataBlock) KDDebuggerData failed to get"));
		return NULL;
	}
	return pKDDebuggerData;
}
ULONG QueryTimerTableListHead()
{
	UNICODE_STRING UnicodeTimerHead;
	ULONG ulTimerTable;
	PUCHAR i;
	ULONG ulTimerTableListHead;
	WIN_VER_DETAIL WinVer;

	WinVer = GetWindowsVersion();
	switch (WinVer)
	{
	case WINDOWS_VERSION_XP:
		RtlInitUnicodeString(&UnicodeTimerHead,(PWCHAR)L"KeUpdateSystemTime");
		ulTimerTable = (ULONG)MmGetSystemRoutineAddress(&UnicodeTimerHead);
		if (ulTimerTable == 0) return 0;
		break;
	case WINDOWS_VERSION_2K3_SP1_SP2:
		RtlInitUnicodeString(&UnicodeTimerHead,(PWCHAR)L"KeSetTimerEx");
		ulTimerTable = (ULONG)MmGetSystemRoutineAddress(&UnicodeTimerHead);
		if (ulTimerTable == 0) return 0;
		break;
	case WINDOWS_VERSION_7_7000:
		RtlInitUnicodeString(&UnicodeTimerHead,(PWCHAR)L"KeUpdateSystemTime");
		ulTimerTable = (ULONG)MmGetSystemRoutineAddress(&UnicodeTimerHead);
		if (ulTimerTable == 0) return 0;
		break;
	default:
		return 0;
	}
	for (i=ulTimerTable;i<ulTimerTable+SizeOfProc(ulTimerTable);i++)
	{
		switch (WinVer)
		{
		case WINDOWS_VERSION_XP:
			//80541fcd 8d0cc520405580  lea     ecx,nt!KiTimerTableListHead (80554020)[eax*8]
			if (*i == 0x8d)
			{
				ulTimerTableListHead = *(PULONG)(i+3);
				if (MmIsAddressValidEx(ulTimerTableListHead))
				{
					return ulTimerTableListHead;
				}
			}
			break;
		case WINDOWS_VERSION_2K3_SP1_SP2:
			//80826ea8 81c240738980    add     edx,offset nt!KiTimerTableListHead (80897340)
			if (*i == 0x81)
			{
				ulTimerTableListHead = *(PULONG)(i+2);
				if (MmIsAddressValidEx(ulTimerTableListHead))
				{
					return ulTimerTableListHead;
				}
			}
			break;
		case WINDOWS_VERSION_7_7000:
			//3bb08c067982    cmp     esi,dword ptr nt!KiTimerTableListHead+0xc (8279068c)[eax]
			if (*i == 0x3b)
			{
				ulTimerTableListHead = *(PULONG)(i+2);
				ulTimerTableListHead = ulTimerTableListHead - 0xc;
				if (MmIsAddressValidEx(ulTimerTableListHead))
				{
					return ulTimerTableListHead;
				}
			}
			break;
		}
	}
	return NULL;
}
////////////////////////////////////////////////////////////////////////////////////
ULONG GetKiProcessorBlock()
{
	PKDDEBUGGER_DATA64 KdData64;

	KdData64 = KdGetDebuggerDataBlock();
	if (MmIsAddressValidEx(KdData64))
	{
		return KdData64->KiProcessorBlock;
	}
	return 0;
}
ULONG GetDpcTimerInformation_WIN7600_UP(PMyDpcTimer KDpcTimer)
{
	ULONG            TimerTableOffsetInKprcb;
	ULONG            NumberOfTimerTable;
	ULONG            NumberOfProcessor;
	ULONG            i,j;
	ULONG            ulTemp;
	ULONG            ulCount = 0;
	PULONG            pKiProcessorBlock = NULL;
	PKTIMER_TABLE_ENTRY_WIN7  pTimerTableEntryWin7 = NULL;
	PLIST_ENTRY          pNextList = NULL;
	PKTIMER          pTimer = NULL;
	MyDpcTimer          MyDpc;
	ULONG ulModuleBase;
	ULONG ulModuleSize;


	TimerTableOffsetInKprcb = 0x1960+0x40;                        //首个_KTIMER_TABLE_ENTRY在PRCB中的偏移
	NumberOfTimerTable = 0x100;                                  //_KTIMER_TABLE_ENTRY数量

	NumberOfProcessor = (ULONG)KeNumberProcessors;                        //当前机器处理器数量
	if (NumberOfProcessor > MAX_PROCESSOR_COUNT){
		return 0;
	}

	pKiProcessorBlock = (PULONG)GetKiProcessorBlock();                      //取得KiProcessorBlock,包含了NumberOfProcessor个KPRCB
	if (!pKiProcessorBlock){
		return 0;
	}
	for ( i = 0; i < NumberOfProcessor; i++, pKiProcessorBlock++ )                //DPC timer在每个cpu中都有一个队列,所以枚举每一个KPRCB
	{
		 //检测一下当前的KPRCB地址是否可访问
		if (!MmIsAddressValidEx((PVOID)pKiProcessorBlock)){
			return;
		}

		//取得当前CPU的KPRCB中首个KTIMER_TABLE_ENTRY地址
		ulTemp = *pKiProcessorBlock + TimerTableOffsetInKprcb;
		if (!MmIsAddressValidEx((PVOID)ulTemp)){
			return;
		}
		//此时ulTemp是 timer table entry地址
		pTimerTableEntryWin7 = (PKTIMER_TABLE_ENTRY_WIN7)ulTemp;                

		//准备遍历timer table表
		for ( j = 0; j < NumberOfTimerTable; j++, pTimerTableEntryWin7++ )
		{
			if (!MmIsAddressValidEx((PVOID)pTimerTableEntryWin7)){
				return;
			}
			//为空的数组高位双字为FFFFFFFF
			if ( pTimerTableEntryWin7->Time.HighPart == 0xFFFFFFFF ){
				continue;
			}

			//链表是否可以访问
			if (!MmIsAddressValidEx((PVOID)pTimerTableEntryWin7->Entry.Blink) ||
				!MmIsAddressValidEx((PVOID)pTimerTableEntryWin7->Entry.Flink)){
				continue;
			}

			for (pNextList   = (PLIST_ENTRY)pTimerTableEntryWin7->Entry.Blink;
				 pNextList  != (PLIST_ENTRY)&pTimerTableEntryWin7->Entry;
				 pNextList   = pNextList->Blink)
			{

				pTimer = CONTAINING_RECORD(pNextList,KTIMER,TimerListEntry);          //取得timer对象

				//检查pTimer以及各成员
				if (!MmIsAddressValidEx((PVOID)pTimer)||
					!MmIsAddressValidEx((PVOID)pTimer->Dpc)||
					!MmIsAddressValidEx((PVOID)pTimer->Dpc->DeferredRoutine)){

					continue;
				}
				//ulCount不能大于MAX_DPCTIMER_COUNT，否则可能会导致内存溢出蓝屏~！
				if (ulCount < MAX_DPCTIMER_COUNT){
					//填充到结构
					KDpcTimer->MyTimer[ulCount].TimerAddress = (ULONG)pTimer;
					KDpcTimer->MyTimer[ulCount].DpcAddress = (ULONG)pTimer->Dpc;
					KDpcTimer->MyTimer[ulCount].DpcRoutineAddress = (ULONG)pTimer->Dpc->DeferredRoutine;
					KDpcTimer->MyTimer[ulCount].Period = pTimer->Period;

					memset(KDpcTimer->MyTimer[ulCount].lpszModule,0,sizeof(KDpcTimer->MyTimer[ulCount].lpszModule));
					if (!IsAddressInSystem(
						KDpcTimer->MyTimer[ulCount].DpcRoutineAddress,
						&ulModuleBase,
						&ulModuleSize,
						KDpcTimer->MyTimer[ulCount].lpszModule))
					{
						strcat(KDpcTimer->MyTimer[ulCount].lpszModule,"Unknown");
					}
					ulCount++;
					KDpcTimer->ulCount = ulCount;
				}
				if (!MmIsAddressValidEx((PVOID)pNextList->Blink)){
					break;
				}
			}
		}
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////
ULONG GetDpcTimerInformation_XP_2K3_WIN7000(PMyDpcTimer KDpcTimer)
{
	ULONG  NumberOfTimerTable;
	ULONG  i;
	ULONG  ulCount = 0;
	PLIST_ENTRY  pList = NULL;
	PLIST_ENTRY pNextList = NULL;
	PKTIMER  pTimer = NULL;
	ULONG ulModuleBase;
	ULONG ulModuleSize;
	WIN_VER_DETAIL WinVer;

	WinVer = GetWindowsVersion();
	switch (WinVer)
	{
	case WINDOWS_VERSION_XP:
		NumberOfTimerTable = 0x100;
		break;
	case WINDOWS_VERSION_2K3_SP1_SP2:
	case WINDOWS_VERSION_7_7000:
		NumberOfTimerTable = 0x200;                                  //_KTIMER_TABLE_ENTRY数量
		break;
	default:
		return;
	}
	pList = (PLIST_ENTRY)QueryTimerTableListHead();                    //取得链表头
	if (pList == NULL) return 0;

	for ( i = 0; i < NumberOfTimerTable; i++, pList++ )                      //NumberOfTimerTable 个list
	{
		if (!MmIsAddressValidEx((PVOID)&pList))
		{
			if (DebugOn)
				KdPrint(("pList Failed\r\n"));
			return NULL;
		}
		if (!MmIsAddressValidEx((PVOID)pList->Blink) ||
			!MmIsAddressValidEx((PVOID)pList->Flink))
		{
			if (DebugOn)
				KdPrint(("Blink Failed\r\n"));
			continue;          //如果listentry域地址无效,continue
		}
		for ( pNextList = pList->Blink; pNextList != pList; pNextList = pNextList->Blink )    //遍历blink链
		{
			//pTimer = CONTAINING_RECORD(pNextList,KDPCTIMER,TimerListEntry);            //得到结构首
			pTimer = CONTAINING_RECORD(pNextList,KTIMER,TimerListEntry);            //得到结构首

			if (MmIsAddressValidEx((PVOID)pTimer) &&
				MmIsAddressValidEx((PVOID)pTimer->Dpc) &&
				MmIsAddressValidEx((PVOID)pTimer->Dpc->DeferredRoutine) &&
				MmIsAddressValidEx((PVOID)&pTimer->Period) )                    //过滤
			{
				//ulCount不能大于MAX_DPCTIMER_COUNT，否则可能会导致内存溢出蓝屏~！
				if (ulCount < MAX_DPCTIMER_COUNT){
					KDpcTimer->MyTimer[ulCount].TimerAddress = (ULONG)pTimer;
					KDpcTimer->MyTimer[ulCount].DpcAddress = (ULONG)pTimer->Dpc;
					KDpcTimer->MyTimer[ulCount].DpcRoutineAddress = (ULONG)pTimer->Dpc->DeferredRoutine;
					KDpcTimer->MyTimer[ulCount].Period = pTimer->Period;

					memset(KDpcTimer->MyTimer[ulCount].lpszModule,0,sizeof(KDpcTimer->MyTimer[ulCount].lpszModule));

					if (!IsAddressInSystem(
						KDpcTimer->MyTimer[ulCount].DpcRoutineAddress,
						&ulModuleBase,
						&ulModuleSize,
						KDpcTimer->MyTimer[ulCount].lpszModule))
					{
						strcat(KDpcTimer->MyTimer[ulCount].lpszModule,"Unknown");
					}
					ulCount++;
					KDpcTimer->ulCount = ulCount;
				}
			}
			if (!MmIsAddressValidEx(pNextList->Blink))
			{
				break;                  //过滤
			}
		}
	}
	return ulCount;
}
BOOL KillDcpTimer(PKTIMER Timer)
{
	BOOL bRetOK = FALSE;

	ReLoadNtosCALL(&RKeCancelTimer,L"KeCancelTimer",SystemKernelModuleBase,ImageModuleBase);
	if (RKeCancelTimer)
	{
		if (MmIsAddressValidEx(Timer))
		{
			return RKeCancelTimer(Timer);
		}
	}
	return bRetOK;
}