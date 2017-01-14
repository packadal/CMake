#include "cmFastbuildTargetGenerator.h"
#include "cmLocalCommonGenerator.h"
#include "cmGlobalGenerator.h"

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

  const cmLocalCommonGenerator* root =
    (const cmLocalCommonGenerator*)this->LocalGenerator->GetGlobalGenerator()
      ->GetLocalGenerators()[0];

  return root->ConvertToRelativePath(root->GetWorkingDirectory(), path);
}
