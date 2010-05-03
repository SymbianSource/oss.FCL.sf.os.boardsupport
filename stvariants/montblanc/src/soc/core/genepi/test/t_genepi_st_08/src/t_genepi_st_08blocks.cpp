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
*  File Name:t_genepi_st_08blocks.cpp Test application file
* author  ST-Ericsson
*/
/*****************************************************************************/ 

/****************************************************************************
 * Includes
 ****************************************************************************/
#include "t_genepi_st_08blocks.h"
#include <e32test.h>

// -----------------------------------------------------------------------------
// Ct_genepi_st_08blocks::Ct_genepi_st_08blocks
// C++ default constructor
// -----------------------------------------------------------------------------
//
Ct_genepi_st_08blocks::Ct_genepi_st_08blocks( ) : iRTestgenepi(_L("t_genepi_st_08"))
{
}

// -----------------------------------------------------------------------------
// Ct_genepi_st_08blocks::~Ct_genepi_st_08blocks
// C++ default destructor
// -----------------------------------------------------------------------------
//
Ct_genepi_st_08blocks::~Ct_genepi_st_08blocks( )
{
}

// -----------------------------------------------------------------------------
// Ct_genepi_st_08blocks::NewL
// Two-phased constructor.
// -----------------------------------------------------------------------------
//
Ct_genepi_st_08blocks* Ct_genepi_st_08blocks::NewL()
{
	Ct_genepi_st_08blocks* self = new (ELeave) Ct_genepi_st_08blocks( );
	return self;
}	

// -----------------------------------------------------------------------------
// Ct_genepi_st_08blocks::RunAllTest05
// Run all tests in sequence
// -----------------------------------------------------------------------------
//
void Ct_genepi_st_08blocks::RunAllTest05()
{
	TInt ret = KErrNone;

	iRTestgenepi.Start(_L("Testing Genepi  Driver"));
	Printf(0, _L("t_genepi_st_08"), _L("Executing Test Class ID: t_genepi_st_08.\n"));

	iRTestgenepi.Next(_L("Test Case no. t_genepi_st_08_01 Started"));
	ret = t_genepi_core_08_01();
	iRTestgenepi(ret == KErrNone);
	Printf(0, _L("t_genepi_st_08"), _L("Test Case no. t_genepi_st_08_01 Completed\n"));


	Printf(0, _L("t_genepi_st_08"), _L("Ending test.\n"));
	iRTestgenepi.End();
	iRTestgenepi.Close();
}	


void Ct_genepi_st_08blocks::Printf( const TInt aPriority, 
		const TDesC& aDefinition, 
		TRefByValue<const TDesC> aFmt,... )
{
	VA_LIST list;
	VA_START(list,aFmt);
	TName aBuf;

	// Parse parameters
	aBuf.AppendFormatList(aFmt,list);        

	iRTestgenepi.Printf(aBuf); 
}


