/*
* Copyright (c) 2009 Nokia Corporation and/or its subsidiary(-ies).
* All rights reserved.
* This component and the accompanying materials are made available
* under the terms of "Eclipse Public License v1.0"
* which accompanies this distribution, and is available
* at the URL "http://www.eclipse.org/legal/epl-v10.html".
*
* Initial Contributors:
* Nokia Corporation - initial contribution.
*
* Contributors:
*
* Description: 
*
*/
/*****************************************************************************/
/**
*  © ST-Ericsson, 2009 - All rights reserved
*  Reproduction and Communication of this document is strictly prohibited
*  unless specifically authorized in writing by ST-Ericsson
*
*  File Name:t_genepi_st_08.cpp Test application file
* author  ST-Ericsson
*/
/*****************************************************************************/ 

/****************************************************************************
 * Includes
 ****************************************************************************/
#include "t_genepi_st_08blocks.h"

/****************************************************************************
FUNCTION : MainL
PURPOSE  : Genepi driver tests
 *****************************************************************************/
LOCAL_C void MainL()
{
	Ct_genepi_st_08blocks* gTest = Ct_genepi_st_08blocks::NewL( );
	CleanupStack::PushL( gTest );
	gTest->RunAllTest05();
	CleanupStack::Pop();
}

/****************************************************************************
FUNCTION : E32Main
PURPOSE  : Entry routine for the Test program
 *****************************************************************************/

GLDEF_C TInt E32Main()
	/**
	 * @return - Standard Epoc error code on exit
	 */
{
	CTrapCleanup* cleanup = CTrapCleanup::New();
	if(cleanup == NULL)
	{
		return KErrNoMemory;
	}
	TRAP_IGNORE(MainL());
	delete cleanup;
	return KErrNone;
}

