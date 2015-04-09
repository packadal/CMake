/*============================================================================
  CMake - Cross Platform Makefile Generator
  Copyright 2000-2009 Kitware, Inc., Insight Software Consortium

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/
#include "cmLocalFastbuildGenerator.h"
#include "cmGlobalGenerator.h"
#include "cmMakefile.h"
#include "cmSourceFile.h"
#include "cmSystemTools.h"
#include "cmCustomCommandGenerator.h"
#include "windows.h"

//----------------------------------------------------------------------------
cmLocalFastbuildGenerator::cmLocalFastbuildGenerator()
{
#ifdef _WIN32
  this->WindowsShell = true;
#endif
}

//----------------------------------------------------------------------------
cmLocalFastbuildGenerator::~cmLocalFastbuildGenerator()
{

}

//----------------------------------------------------------------------------
void cmLocalFastbuildGenerator::Generate()
{
	// Debug messages
	std::cout << "======== LOCAL Fastbuild Gen ========\n";
	GetMakefile()->Print();

	// Now generate information for this generator
}

//----------------------------------------------------------------------------
void cmLocalFastbuildGenerator::ExpandRuleVariables(std::string& s, 
	const RuleVariables& replaceValues)
{
	return cmLocalGenerator::ExpandRuleVariables( s, replaceValues );
}

//----------------------------------------------------------------------------
