/*============================================================================
  CMake - Cross Platform Makefile Generator
  Copyright 2000-2009 Kitware, Inc., Insight Software Consortium

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/
#ifndef cmGlobalFastbuildGenerator_h
#define cmGlobalFastbuildGenerator_h

#include "cmGlobalGenerator.h"

class cmGlobalGeneratorFactory;

/** \class cmGlobalFastbuildGenerator
 * \brief Class for global fastbuild generator.
 */
class cmGlobalFastbuildGenerator 
	: public cmGlobalGenerator
{
public:
  cmGlobalFastbuildGenerator();
  virtual ~cmGlobalFastbuildGenerator();

  static cmGlobalGeneratorFactory* NewFactory();

  virtual void Generate();

  ///! create the correct local generator
  virtual cmLocalGenerator *CreateLocalGenerator();

private:
	class Factory;
};

#endif
