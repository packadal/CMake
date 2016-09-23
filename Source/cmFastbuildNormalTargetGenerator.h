#ifndef CMFASTBUILDNORMALTARGETGENERATOR_H
#define CMFASTBUILDNORMALTARGETGENERATOR_H

#include "cmFastbuildTargetGenerator.h"
#include "cmGlobalFastbuildGenerator.h"

class cmFastbuildNormalTargetGenerator : public cmFastbuildTargetGenerator
{
public:
  cmFastbuildNormalTargetGenerator(
    cmGeneratorTarget* gt,
    cmGlobalFastbuildGenerator::Detail::Generation::GenerationContext
      m_context);

  virtual void Generate();

private:
  void DetectTargetCompileDependencies(cmGlobalCommonGenerator* gg,
                                       std::vector<std::string>& dependencies);
  cmGlobalFastbuildGenerator::Detail::Generation::GenerationContext m_context;
};

#endif // CMFASTBUILDNORMALTARGETGENERATOR_H
