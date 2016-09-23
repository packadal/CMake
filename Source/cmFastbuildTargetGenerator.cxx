#include "cmFastbuildTargetGenerator.h"

cmFastbuildTargetGenerator::cmFastbuildTargetGenerator(cmGeneratorTarget* gt)
  : cmCommonTargetGenerator(cmOutputConverter::HOME_OUTPUT, gt)
{
}

void cmFastbuildTargetGenerator::AddIncludeFlags(std::string& flags,
                                                 const std::string& lang)
{
}
