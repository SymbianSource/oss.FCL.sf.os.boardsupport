// Copyright (c) 2009 Nokia Corporation and/or its subsidiary(-ies).
// All rights reserved.
// This component and the accompanying materials are made available
// under the terms of the License "Eclipse Public License v1.0"
// which accompanies this distribution, and is available
// at the URL "http://www.eclipse.org/legal/epl-v10.html".
//
// Initial Contributors:
// Nokia Corporation - initial contribution.
//
// Contributors:
//
// Description:
// Partition Management for Embedded MMC devices
// 
//

#include "emmcptn.h"

#define ST_FLASHER
#define BGAHSMMC_PI_STR_SIZE sizeof( BGAHSMMC_PI_STR )

//- Constants ---------------------------------------------------------------

const TUint8  KMaxNbrOfTocItems     = 16; //condition false in decode partition info function 32; //should be 16 (32*16=512)
const TUint8  KXMaxNbrOfTocItems    = 16;
const TUint32 KEndOfToc             = 0xFFFFFFFFUL;
const TInt  KMaxItemNameLen   		= 12;

const TUint32 KMegaByte 			= 1024 * 1024;
const TUint32 KEraseStepSize 		= 4 * KMegaByte;

// for Emmc(S) min erase is 256KB for Emmc(T) min erase is 512KB
const TUint32 KEraseMinSize			= 512 * 1024;
const TInt KDiskSectorSize			=512;
const TInt KDiskSectorShift			=9;

// UnistoreII definitions
#define VOLNUM							0			
#define SECTOR_SIZE						512
#define TWO_SECTOR_SIZE					1024

struct STocItem
	{
	TUint32 iByteStartAddress;
	TUint32 iByteSize;
	TUint32 iFlags;
	TUint32 iAlign;
	TUint32 iLoadAddress;
	TText8  iFileName[KMaxItemNameLen];
	};


class Toc
    {
    public:
        TInt Init();
        TInt GetItem(const TText8* aItemName, STocItem& aItem);
        TInt GetItemEx(const TText8* aName, STocItem& aItem);
	public:
		STocItem iTOC[KMaxNbrOfTocItems];
		STocItem iEmmcPartitionTable[4];		
		TUint32  iTocStartSector;
		TUint64  iUserAreaInBytes;
		TInt 	 iPartitionCount;
		TBool    iEMMCPtnUpdate;
		
#if defined(_VERSION_INFO)
		TUint32 iVersionInfoItems;
		TVersionInfoItem iVersionInfo[KMaxSectionItems];
#endif

	private:
        TInt GetItemX(const TText8* aItemName, STocItem& aItem);

    };


class DLegacyEMMCPartitionInfo : public DEMMCPartitionInfo
	{
public:
	 DLegacyEMMCPartitionInfo();
	~DLegacyEMMCPartitionInfo();
	Toc *iTocPtr;


public:
	virtual TInt Initialise(DMediaDriver* aDriver);
	virtual TInt PartitionInfo(TPartitionInfo& anInfo, const TMMCCallBack& aCallBack);
	virtual TInt PartitionCaps(TLocDrv& aDrive, TDes8& aInfo);
	
protected:
	void SetPartitionEntry(TPartitionEntry* aEntry, TUint aFirstSector, TUint aNumSectors);
	
private:
	static void SessionEndCallBack(TAny* aSelf);
		   void DoSessionEndCallBack();
	virtual TInt DecodePartitionInfo();
	
protected:
	DMediaDriver*   iDriver;
	TPartitionInfo* iPartitionInfo;
	TMMCCallBack	iSessionEndCallBack;
	TMMCCallBack 	iCallBack;         // Where to report the PartitionInfo completion
	DMMCSession*	iSession;
	TMMCard*		iCard;
	TUint8*			iIntBuf;
	};

DLegacyEMMCPartitionInfo::DLegacyEMMCPartitionInfo()
  : iSessionEndCallBack(DLegacyEMMCPartitionInfo::SessionEndCallBack, this)
	{
	}

DLegacyEMMCPartitionInfo::~DLegacyEMMCPartitionInfo()
	{
	delete iSession;
	}

TInt DLegacyEMMCPartitionInfo::Initialise(DMediaDriver* aDriver)
	{
	iDriver = aDriver;

	DMMCSocket* socket = ((DMMCSocket*)((DPBusPrimaryMedia*)(iDriver->iPrimaryMedia))->iSocket);
	if(socket == NULL)
		return(KErrNoMemory);

	DMMCStack* stack = socket->Stack(0);
	iCard = stack->CardP(((DPBusPrimaryMedia*)(iDriver->iPrimaryMedia))->iSlotNumber);
	
	iSession = stack->AllocSession(iSessionEndCallBack);
	if (iSession == NULL)
		return(KErrNoMemory);

	iSession->SetStack(stack);
	iSession->SetCard(iCard);

	// this gets used before any access
	TInt bufLen, minorBufLen;
	stack->BufferInfo(iIntBuf, bufLen, minorBufLen);

	return(KErrNone);
	}

TInt DLegacyEMMCPartitionInfo::PartitionInfo(TPartitionInfo& anInfo, const TMMCCallBack& aCallBack)
	{
	iPartitionInfo = &anInfo;
	iCallBack = aCallBack;
	// If media driver is persistent (see EMediaDriverPersistent), 
	// the card may have changed since last power down, so reset CID
	iSession->SetCard(iCard);
	
	TUint32 TocOffset =0; //TOC is on first Block

#ifndef ST_FLASHER
	TocOffset = 1280; //TOC mapped 0x000A0000 bytes offset i.e. 1280 blocks
#endif //!ST_FLASHER

	iSession->SetupCIMReadBlock(TocOffset, iIntBuf);	// aBlocks = 1

	TInt r = iDriver->InCritical();
	if (r == KErrNone)
		r = iSession->Engage();

	if(r != KErrNone)
		iDriver->EndInCritical();
	
	return(r);
	}

TInt DLegacyEMMCPartitionInfo::PartitionCaps(TLocDrv& aDrive, TDes8& aInfo)
	{
	 TLocalDriveCapsV6Buf& Info = static_cast< TLocalDriveCapsV6Buf&> (aInfo);
	__KTRACE_OPT(KPBUSDRV,Kern::Printf(">>iPartitionType=%d",aDrive.iPartitionType));	
	

	if (aDrive.iPartitionType == KPartitionTypeRofs)
		{
		__KTRACE_OPT(KPBUSDRV,Kern::Printf("eMMC proto: Caps for ROFS,drive NO =%d",aDrive.iDriveNumber));

		Info().iType 			= EMediaHardDisk;		
		Info().iDriveAtt 		= KDriveAttLocal | KDriveAttInternal;
		Info().iFileSystemId  	= KDriveFileSysROFS;
		Info().iPartitionType 	= KPartitionTypeRofs;
		Info().iMediaAtt      	|= KMediaAttWriteProtected;
		Info().iSize 		 	= TUint32(iTocPtr->iEmmcPartitionTable[1].iByteSize);

		}
	else if	(aDrive.iPartitionType == KPartitionTypeEmpty) //for CoreOS
		{

		__KTRACE_OPT(KPBUSDRV,Kern::Printf("eMMC proto: Caps for EmptyP,drive NO =%d",aDrive.iDriveNumber));
		
		Info().iType 			= EMediaHardDisk;
		Info().iDriveAtt 		= KDriveAttLocal | KDriveAttInternal;	
		Info().iFileSystemId  	= KDriveFileNone;
		Info().iPartitionType 	= KPartitionTypeEmpty;
		Info().iMediaAtt      	|= KMediaAttWriteProtected;
		Info().iSize 		    = iTocPtr->iEmmcPartitionTable[0].iByteSize;
		
		}
		
	else if((aDrive.iDriveNumber == 2) && (!iTocPtr->iEMMCPtnUpdate))
		{
		__KTRACE_OPT(KPBUSDRV,Kern::Printf("eMMC proto: Caps iDriveNumber =%d",aDrive.iDriveNumber));
		

		Info().iType 			= EMediaHardDisk;
		Info().iDriveAtt 		|= KDriveAttLocal | KDriveAttInternal ;

		Info().iFileSystemId 	= KDriveFileSysFAT;
		Info().iPartitionType 	= KPartitionTypeWin95FAT32;
		Info().iMediaAtt      	|= KMediaAttFormattable;
		Info().iSize 		 	= TUint64(iTocPtr->iUserAreaInBytes);
		}
		
	// is this query for the swap partition ?
	if (aDrive.iPartitionType == KPartitionTypePagedData)
		{
		Info().iFileSystemId = KDriveFileNone;
		Info().iDriveAtt|= KDriveAttHidden;
		}

	// is this query for the ROFS partition ?
	if (aDrive.iPartitionType == KPartitionTypeRofs)
		{
		Info().iFileSystemId = KDriveFileSysROFS;
		Info().iMediaAtt&= ~KMediaAttFormattable;
		Info().iMediaAtt|= KMediaAttWriteProtected;
		}
	
	// is this query for the ROM partition ?
	if (aDrive.iPartitionType == KPartitionTypeROM)
		{
		Info().iFileSystemId = KDriveFileNone;
		Info().iMediaAtt&= ~KMediaAttFormattable;
		Info().iMediaAtt|= KMediaAttWriteProtected;
		}
	
	return KErrNone;
	}

void DLegacyEMMCPartitionInfo::SessionEndCallBack(TAny* aSelf)
	{
	DLegacyEMMCPartitionInfo& self = *static_cast<DLegacyEMMCPartitionInfo*>(aSelf);
	self.DoSessionEndCallBack();
	}

void DLegacyEMMCPartitionInfo::DoSessionEndCallBack()
	{
	iDriver->EndInCritical();

	TInt r = iSession->EpocErrorCode();

	if (r == KErrNone)
		r = DecodePartitionInfo();

	iDriver->PartitionInfoComplete(r == KErrNone ? r : KErrNotReady);
	}

TInt DLegacyEMMCPartitionInfo::DecodePartitionInfo()
//
// decode partition info that was read into internal buffer 
//
	{
	__KTRACE_OPT(KPBUSDRV,Kern::Printf(">>emcptn:DecodePartitionInfo"));

	TUint partitionCount=iPartitionInfo->iPartitionCount=0;
	TInt defaultPartitionNumber=-1;
	TMBRPartitionEntry* pe;
	const TUint KMBRFirstPartitionOffsetAligned = KMBRFirstPartitionOffset & ~3;
	TInt i;
	iTocPtr = reinterpret_cast<Toc*>(iIntBuf);
	iTocPtr->iEMMCPtnUpdate = EFalse;
	iTocPtr->iPartitionCount = 0;

	STocItem item; 
		
#ifdef ST_FLASHER
	for (TUint8 nCnt = 0; nCnt < 5; nCnt++) 
#else
	for (TUint8 nCnt = 0; nCnt < KMaxNbrOfTocItems; nCnt++) 
#endif //ST_FLASHER	
		{ 

#ifdef ST_FLASHER
		if((iTocPtr->GetItemEx((TText8*)"NORMAL", item) == KErrNone) ||(iTocPtr->GetItemEx((TText8*)"X-LOADER", item)== KErrNone))
#else
		if(iTocPtr->GetItemEx("SOS+CORE", item)== KErrNone) 
#endif //ST_FLASHER
			{
			__KTRACE_OPT(KPBUSDRV,Kern::Printf(">>Valid TOC/PIB found"));
			}
		else 
			{
			__KTRACE_OPT(KPBUSDRV,Kern::Printf("ERROR eMMC proto: No Valid TOC/PIB structure !"));
			iTocPtr->iEMMCPtnUpdate = ETrue; 
			}
		}

	if(iTocPtr)
	{
	if(!iTocPtr->iEMMCPtnUpdate)
		{

		TInt ret = KErrNone;

		//SOS+CORE/NORMAL=0,SOS+ROFS1=1,SOS+ROFS2=2,SOS-USER/PRODUCTION=3

#ifdef ST_FLASHER



		ret = iTocPtr->GetItemEx((TText8*)"NORMAL", item);
#else
		ret = iTocPtr->GetItemEx("SOS+CORE", item); 
#endif //ST_FLASHER
					
		if(ret == KErrNone)	
			{
			__KTRACE_OPT(KPBUSDRV,Kern::Printf(">>SOS+CORE/NORMAL partition found"));
			//SOS+CORE Partition
			iTocPtr->iEmmcPartitionTable[0] = item;	
			}
		else 
			{
			__KTRACE_OPT(KPBUSDRV,Kern::Printf(">>ERROR SOS+CORE/NORMAL partition NOT found"));
			}

		//In ST flasher ROFS partitions are not define
#ifndef ST_FLASHER
		ret = iTocPtr->GetItemEx("SOS+ROFS1", item); 
		if(ret == KErrNone)	
			{
			__KTRACE_OPT(KPBUSDRV,Kern::Printf(">>SOS+ROFS1 partition found"));
			//Rofs Partition
			iTocPtr->iEmmcPartitionTable[1] = item;	
			}
		else 
			{
			__KTRACE_OPT(KPBUSDRV,Kern::Printf(">>ERROR SOS+ROFS1 partition NOT found"));
			}

		// In ST flasher consider : Normal -> coreos, ADL -> Rofs, No Rofs_Ext,Production -> User

		ret = iTocPtr->GetItemEx("SOS+ROFS2", item); 
		if(ret == KErrNone)	
			{
			__KTRACE_OPT(KPBUSDRV,Kern::Printf(">>SOS+ROFS2 partition found"));
			//ROFX Partition
			iTocPtr->iEmmcPartitionTable[2] = item;	
			}
		else 
			{
			__KTRACE_OPT(KPBUSDRV,Kern::Printf(">>ERROR SOS+ROFS2 partition NOT found"));
			}
#endif //ST_FLASHER

#ifdef ST_FLASHER
		//USER partition is => Start address of PRODUCTION + Size of PRODUCTION
		ret = iTocPtr->GetItemEx((TText8*)"PRODUCTION", item);
#else
		ret = iTocPtr->GetItemEx("SOS-USER", item); 
#endif //ST_FLASHER		
		if(ret == KErrNone)	
			{
			__KTRACE_OPT(KPBUSDRV,Kern::Printf(">>SOS-USER partition found"));
			//SOS-USER Partition
			iTocPtr->iEmmcPartitionTable[3] = item;	
#ifdef ST_FLASHER			
			TUint64 NoOfBlock512KAlign 							= (iTocPtr->iEmmcPartitionTable[3].iByteStartAddress + iTocPtr->iEmmcPartitionTable[3].iByteSize)/(KEraseMinSize);	
			iTocPtr->iEmmcPartitionTable[3].iByteStartAddress	= (NoOfBlock512KAlign * KEraseMinSize) + KEraseStepSize;//align with 512KB + extra 4MB
			iTocPtr->iUserAreaInBytes 							= (iCard->DeviceSize64()  - ((TUint64)(iTocPtr->iEmmcPartitionTable[3].iByteStartAddress)));		
#else //ST_FLASHER
			iTocPtr->iEmmcPartitionTable[3].iByteStartAddress 	= iTocPtr->iEmmcPartitionTable[3].iByteStartAddress;
			iTocPtr->iUserAreaInBytes 							= iTocPtr->iEmmcPartitionTable[3].iByteSize;
#endif //ST_FLASHER			
			}
		else //search MEM_INIT partition
			{
			__KTRACE_OPT(KPBUSDRV,Kern::Printf(">>ERROR SOS-USER partition NOT found search for MEM_INIT"));
			//USER partition is => Start address of MEM_INIT + Size of MEM_INIT
			ret = iTocPtr->GetItemEx((TText8*)"MEM_INIT", item); 
			if(ret == KErrNone)	
				{
				__KTRACE_OPT(KPBUSDRV,Kern::Printf(">>MEM_INIT partition found"));
				//SOS-USER Partition
				iTocPtr->iEmmcPartitionTable[3] 					= item;	
				TUint64 NoOfBlock512KAlign 							= (iTocPtr->iEmmcPartitionTable[3].iByteStartAddress + iTocPtr->iEmmcPartitionTable[3].iByteSize)/(KEraseMinSize);	
				iTocPtr->iEmmcPartitionTable[3].iByteStartAddress	= (NoOfBlock512KAlign * KEraseMinSize) + KEraseStepSize;//align with 512KB + extra 4MB
				iTocPtr->iUserAreaInBytes 							= (iCard->DeviceSize64()  - ((TUint64)(iTocPtr->iEmmcPartitionTable[3].iByteStartAddress)));		
				}
			else 
				{
				__KTRACE_OPT(KPBUSDRV,Kern::Printf(">>ERROR MEM_INIT partition NOT found"));
				}
			}
			

		//	User Partition : iPartitionCount =0
		iPartitionInfo->iEntry[iTocPtr->iPartitionCount].iPartitionBaseAddr 	= iTocPtr->iEmmcPartitionTable[3].iByteStartAddress;
		//iTocPtr->iUserAreaInBytes 											= iTocPtr->iUserAreaInBytes - (TUint64)(KEraseStepSize - KDiskSectorSize);// (0x3ffe00)		
		iPartitionInfo->iEntry[iTocPtr->iPartitionCount].iPartitionLen 			= iTocPtr->iUserAreaInBytes;		

		__KTRACE_OPT(KPBUSDRV,Kern::Printf(">>iTocPtr->iEmmcPartitionTable[3].iByteSize=0x%x",iTocPtr->iUserAreaInBytes));		

		SetPartitionEntry( &iPartitionInfo->iEntry[iTocPtr->iPartitionCount], iTocPtr->iEmmcPartitionTable[3].iByteStartAddress/KDiskSectorSize, iTocPtr->iUserAreaInBytes/(TUint64)(KDiskSectorSize ));		
		iPartitionInfo->iEntry[iTocPtr->iPartitionCount].iPartitionType 		= KPartitionTypeFAT16;
		iTocPtr->iPartitionCount++;

		//	CoreOs Partition : iPartitionCount =1
		iPartitionInfo->iEntry[iTocPtr->iPartitionCount].iPartitionBaseAddr 	= iTocPtr->iEmmcPartitionTable[0].iByteStartAddress;
		iPartitionInfo->iEntry[iTocPtr->iPartitionCount].iPartitionLen 			= iTocPtr->iEmmcPartitionTable[0].iByteSize;

		SetPartitionEntry( &iPartitionInfo->iEntry[iTocPtr->iPartitionCount], iTocPtr->iEmmcPartitionTable[0].iByteStartAddress/KDiskSectorSize, iTocPtr->iEmmcPartitionTable[0].iByteSize/KDiskSectorSize );		
		iPartitionInfo->iEntry[iTocPtr->iPartitionCount].iPartitionType 		=  KPartitionTypeROM;//KPartitionTypeEmpty indar
		iTocPtr->iPartitionCount ++;				
		}

	}

	__KTRACE_OPT(KPBUSDRV,Kern::Printf(">>partitionCount=%d",iTocPtr->iPartitionCount));

	partitionCount=iTocPtr->iPartitionCount;

	// Read of the first sector successful so check for a Master Boot Record
	if (*(TUint16*)(&iIntBuf[KMBRSignatureOffset])!=0xAA55)
	// If no valid signature give up now, No way to re-format an internal drive correctly
			{
			__KTRACE_OPT(KPBUSDRV, Kern::Printf("mmc:No MBR Found"));
			// return KErrCorrupt;
			}
				

	__ASSERT_COMPILE(KMBRFirstPartitionOffsetAligned + KMBRMaxPrimaryPartitions * sizeof(TMBRPartitionEntry) <= KMBRSignatureOffset);

	memmove(&iIntBuf[0], &iIntBuf[2],
		KMBRFirstPartitionOffsetAligned + KMBRMaxPrimaryPartitions * sizeof(TMBRPartitionEntry)); 


	for (i=0, pe = (TMBRPartitionEntry*)(&iIntBuf[KMBRFirstPartitionOffsetAligned]);
		pe->iPartitionType != 0 && i < KMaxPartitionEntries; i++,pe--)
		{
		if (pe->IsDefaultBootPartition())
			{
			SetPartitionEntry(&iPartitionInfo->iEntry[0],pe->iFirstSector,pe->iNumSectors);
			defaultPartitionNumber=i;
			partitionCount++;
			break;
			}
		}

	// Now add any other partitions
	for (i=0, pe = (TMBRPartitionEntry*)(&iIntBuf[KMBRFirstPartitionOffsetAligned]);
		pe->iPartitionType != 0 && i < KMaxPartitionEntries; i++,pe--)
		{
		if (defaultPartitionNumber==i)
			{
			// Already sorted
			}

		// FAT partition ?
		else if (pe->IsValidDosPartition() || pe->IsValidFAT32Partition())
			{
			SetPartitionEntry(&iPartitionInfo->iEntry[partitionCount],pe->iFirstSector,pe->iNumSectors);
			__KTRACE_OPT(KLOCDPAGING, Kern::Printf("Mmc: FAT partition found at sector #%u", pe->iFirstSector));
			partitionCount++;
			}

		else if (pe->iPartitionType == KPartitionTypeROM)
			{
			TPartitionEntry& partitionEntry = iPartitionInfo->iEntry[partitionCount];
			SetPartitionEntry(&iPartitionInfo->iEntry[partitionCount],pe->iFirstSector,pe->iNumSectors);
			partitionEntry.iPartitionType = pe->iPartitionType;
			partitionCount++;				 

			__KTRACE_OPT(KLOCDPAGING, Kern::Printf("Mmc: KPartitionTypeROM found at sector #%u", pe->iFirstSector));
			}

		// ROFS partition ?
		else if (pe->iPartitionType == KPartitionTypeRofs)
			{
			
// Don't expose this for normal operation only boot?			
			TPartitionEntry& partitionEntry = iPartitionInfo->iEntry[partitionCount];
			SetPartitionEntry(&iPartitionInfo->iEntry[partitionCount],pe->iFirstSector,pe->iNumSectors);
			partitionEntry.iPartitionType = pe->iPartitionType;
			__KTRACE_OPT(KLOCDPAGING, Kern::Printf("Mmc: KPartitionTypeRofs found at sector #%u", pe->iFirstSector));
			partitionCount++;
			}
 
		// Swap partition ?
		else if (pe->iPartitionType == KPartitionTypePagedData)
			{
			__KTRACE_OPT(KLOCDPAGING, Kern::Printf("Mmc: KPartitionTypePagedData found at sector #%u", pe->iFirstSector));

			TPartitionEntry& partitionEntry = iPartitionInfo->iEntry[partitionCount];
			SetPartitionEntry(&iPartitionInfo->iEntry[partitionCount],pe->iFirstSector,pe->iNumSectors);
			partitionEntry.iPartitionType = pe->iPartitionType;
			partitionCount++;
			}
		}

	// Check the validity of the partition address boundaries
	// If there is any MBR errors
	if(partitionCount > 0)
		{
		const TInt64 deviceSize = iCard->DeviceSize64();
		TPartitionEntry& part = iPartitionInfo->iEntry[partitionCount - 1];
		// Check that the card address space boundary is not exceeded by the last partition
		if(part.iPartitionBaseAddr + part.iPartitionLen > deviceSize)
			{
			__KTRACE_OPT(KPBUSDRV, Kern::Printf("Mmc: MBR partition exceeds card memory space"));
			return KErrCorrupt;
			}
		
		// More than one partition. Go through all of them
		if (partitionCount > 0)
			{
			for(i=partitionCount-1; i>0; i--)
				{
				const TPartitionEntry& curr = iPartitionInfo->iEntry[i];
				TPartitionEntry& prev = iPartitionInfo->iEntry[i-1];
				// Check if partitions overlap
				if(curr.iPartitionBaseAddr < (prev.iPartitionBaseAddr + prev.iPartitionLen))
					{
					__KTRACE_OPT(KPBUSDRV, Kern::Printf("Mmc: Overlapping partitions"));
						//return KErrCorrupt;
					}
				}
			}
		}

	if (defaultPartitionNumber==(-1) && partitionCount==0)
		{
		__KTRACE_OPT(KPBUSDRV, Kern::Printf("No Valid Partitions Found!"));
		return KErrCorrupt;
		}

	
	iPartitionInfo->iPartitionCount=partitionCount;
	iPartitionInfo->iMediaSizeInBytes=iCard->DeviceSize64();

#ifdef _DEBUG
	__KTRACE_OPT(KPBUSDRV, Kern::Printf("<Mmc:PartitionInfo (C:%d)",partitionCount));
	for (TUint x=0; x<partitionCount; x++)
		__KTRACE_OPT(KPBUSDRV, Kern::Printf("     Partition%d (B:%xH L:%xH)",x,I64LOW(iPartitionInfo->iEntry[x].iPartitionBaseAddr),I64LOW(iPartitionInfo->iEntry[x].iPartitionLen)));
#endif


	//Notify medmmc that partitioninfo is complete.
	iCallBack.CallBack();
	
	return(KErrNone);
	}


void DLegacyEMMCPartitionInfo::SetPartitionEntry(TPartitionEntry* aEntry, TUint aFirstSector, TUint aNumSectors)
//
// auxiliary static function to record partition information in TPartitionEntry object
//
	{
	aEntry->iPartitionBaseAddr=aFirstSector;
	aEntry->iPartitionBaseAddr<<=KDiskSectorShift;
	aEntry->iPartitionLen=aNumSectors;
	aEntry->iPartitionLen<<=KDiskSectorShift;
	aEntry->iPartitionType=KPartitionTypeFAT12;
	}

// End - DLegacyEMMCPartitionInfo


EXPORT_C DEMMCPartitionInfo* CreateEmmcPartitionInfo()
	{
	return new DLegacyEMMCPartitionInfo;
	}

DECLARE_STANDARD_EXTENSION()
	{
	return KErrNone;
	}




/**************************************************************************
* TInt Toc::GetItemX(const TText8* aItemName, STocItem& aItem)
*-----------------------------------------------------------------------
* Search entry in XLOADER TOC with ItemName.
* End of TOC limited by amount of entries only
*-----------------------------------------------------------------------
* Parameters:
* TText8		: Item Name
* STocItem		: Reference of Item
*-----------------------------------------------------------------------
* Return Value
* KErrNone		: If Item Found other wise return KErrNotFound
*************************************************************************/

TInt Toc::GetItemX(const TText8* aItemName, STocItem& aItem)
    {
	TUint8 i = 0;

    if ( aItemName == NULL )
        {
        return KErrNotFound;
        }

    // check all items
	while ( i < KXMaxNbrOfTocItems )
		{
		TUint8 j;	
		for ( j = 0; j < KMaxItemNameLen; j++ )				
			{
				if ( aItemName[j] == iTOC[i].iFileName[j] )
					{
					if ( aItemName[j] == 0 )
						{
						// item found
						aItem = iTOC[i];
						return KErrNone;
						}
					}
				else
					{
					break;
					}
			}

		i++;
		}
	
	return KErrNotFound;
    }


/**************************************************************************
* TInt Toc::GetItem(const TText8* aItemName, STocItem& aItem)
*-----------------------------------------------------------------------
* Search entry in TOC with ItemName.
*-----------------------------------------------------------------------
* Parameters:
* TText8		: Item Name
* STocItem		: Reference of Item
*-----------------------------------------------------------------------
* Return Value
* KErrNone		: If Item Found other wise return KErrNotFound
*************************************************************************/

TInt Toc::GetItem(const TText8* aItemName, STocItem& aItem)
    {
	TUint8 i = 0;

    if ( aItemName == NULL )
        {
        return KErrNotFound;
        }

	// check all items
	while ( i < KMaxNbrOfTocItems && iTOC[i].iByteStartAddress != KEndOfToc )
		{
		TUint8 j;	
		for ( j = 0; j < KMaxItemNameLen; j++ )				
			{
				if ( aItemName[j] == iTOC[i].iFileName[j] )
					{
					if ( aItemName[j] == 0 )
						{
						// item found
						aItem = iTOC[i];
						aItem.iByteStartAddress += ((TUint32)SECTOR_SIZE) * iTocStartSector;
						return KErrNone;
						}
					}
				else
					{
					break;
					}
			}

		i++;
		}
	
	return KErrNotFound;
    }

/**************************************************************************
* TInt Toc::GetItem(const TText8* aItemName, STocItem& aItem)
*-----------------------------------------------------------------------
* Search entry in TOC with aName as part of ItemName.
*-----------------------------------------------------------------------
* Parameters:
* TText8		: Item Name
* STocItem		: Reference of Item
*-----------------------------------------------------------------------
* Return Value
* KErrNone		: If Item Found other wise return KErrNotFound
*************************************************************************/

TInt Toc::GetItemEx(const TText8* aName, STocItem& aItem)
    {
	TInt i = 0;
	TInt l1 =0;
	TInt j , k,l2;

    if ( aName == NULL )
        {
        return KErrNotFound;
        }
        
	// calculate length for name to be searched
	while ( i < KMaxItemNameLen && aName[i] != 0 ) { i++; l1++; }	
	if ( !l1 ) return KErrGeneral; // zero length

	// check all items
	i = 0;
	while ( i < KMaxNbrOfTocItems && iTOC[i].iByteStartAddress != KEndOfToc )
		{	
		// calculate length of current item
		j = 0; l2 = 0;
		while ( j < KMaxItemNameLen && iTOC[i].iFileName[j] != 0 ) { j++; l2++; }
		if ( l2 < l1 ) { i++; continue; } // too short name, skip it
	

		// compare Item with aName
		for ( j = 0; j <= (l2 - l1); j++ )				
			{
				for ( k = 0; k < l1; k++ )
					{
						if ( aName[k] != iTOC[i].iFileName[j+k] ) break;
					}

				if ( k == l1 )
					{
					// item found
					aItem = iTOC[i];
			//		aItem.iByteStartAddress += ((TUint32)SECTOR_SIZE) * iTocStartSector; //indar check*
					return KErrNone;
					}
			}		

		i++;
		}
	
	return KErrNotFound;
    }
//  End of File

