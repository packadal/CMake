#include "cmFastbuildTargetGenerator.h"
#include "cmLocalCommonGenerator.h"
#include "cmGlobalGenerator.h"
#include "cmGlobalFastbuildGenerator.h"

cmFastbuildTargetGenerator::cmFastbuildTargetGenerator(cmGeneratorTarget* gt)
  : cmCommonTargetGenerator(gt)
{
}

void cmFastbuildTargetGenerator::AddIncludeFlags(std::string& /* flags */,
                                                 const std::string& /* lang */)
{
}

std::string cmFastbuildTargetGenerator::ConvertToFastbuildPath(
  const std::string& path)
{
  return ((cmGlobalFastbuildGenerator*)GlobalGenerator)
    ->ConvertToFastbuildPath(path);
}
