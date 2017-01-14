#include "cmFastbuildTargetGenerator.h"

cmFastbuildTargetGenerator::cmFastbuildTargetGenerator(cmGeneratorTarget* gt)
  : cmCommonTargetGenerator(gt)
{
}

void cmFastbuildTargetGenerator::AddIncludeFlags(std::string& /* flags */,
                                                 const std::string& /* lang */)
{
}
