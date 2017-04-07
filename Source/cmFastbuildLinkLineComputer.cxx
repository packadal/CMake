#include "cmFastbuildLinkLineComputer.h"

#include "cmGlobalFastBuildGenerator.h"

cmFastBuildLinkLineComputer::cmFastBuildLinkLineComputer(
  cmOutputConverter* outputConverter, cmStateDirectory stateDir,
  const cmGlobalFastbuildGenerator* gg)
  : cmLinkLineComputer(outputConverter, stateDir)
  , GG(gg)
{
}

std::string cmFastBuildLinkLineComputer::ConvertToLinkReference(
  const std::string& input) const
{
  return GG->ConvertToFastbuildPath(input);
}
