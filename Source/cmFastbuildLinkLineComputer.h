/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file Copyright.txt or https://cmake.org/licensing for details.  */

#ifndef cmNinjaLinkLineComputer_h
#define cmNinjaLinkLineComputer_h

#include <cmConfigure.h>

#include <string>

#include "cmLinkLineComputer.h"

class cmGlobalFastbuildGenerator;
class cmOutputConverter;
class cmStateDirectory;

class cmFastBuildLinkLineComputer : public cmLinkLineComputer
{
public:
  cmFastBuildLinkLineComputer(cmOutputConverter* outputConverter,
                              cmStateDirectory stateDir,
                              const cmGlobalFastbuildGenerator *gg);

  virtual std::string ConvertToLinkReference(std::string const& input) const
    CM_OVERRIDE;

private:
  cmGlobalFastbuildGenerator const* GG;
};

#endif
