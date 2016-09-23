#ifndef CMFASTBUILDNORMALTARGETGENERATOR_H
#define CMFASTBUILDNORMALTARGETGENERATOR_H

#include "cmFastbuildTargetGenerator.h"
#include "cmGlobalFastbuildGenerator.h"

class cmFastbuildNormalTargetGenerator : public cmFastbuildTargetGenerator
{
public:
  cmFastbuildNormalTargetGenerator(
    cmGeneratorTarget* gt,
    cmGlobalFastbuildGenerator::Detail::Generation::CustomCommandAliasMap
      customCommandAliases);

  virtual void Generate();

private:
  void DetectTargetCompileDependencies(cmGlobalCommonGenerator* gg,
                                       std::vector<std::string>& dependencies);

  void WriteTargetAliases(const std::vector<std::string>& linkableDeps,
                          const std::vector<std::string>& orderDeps);

  void WriteCustomBuildSteps(const std::vector<cmCustomCommand>& commands,
                             const std::string& buildStep,
                             const std::vector<std::string>& orderDeps);

  bool WriteCustomBuildRules();

  void WriteCustomCommand(const cmCustomCommand* cc,
                          const std::string& configName,
                          std::string& targetName,
                          const std::string& hostTargetName);

  static void EnsureDirectoryExists(const std::string& path,
                                    const char* homeOutputDirectory);

  cmGlobalFastbuildGenerator::Detail::FileContext& m_fileContext;
  cmGlobalFastbuildGenerator::Detail::Generation::CustomCommandAliasMap
    m_customCommandAliases;
};

#endif // CMFASTBUILDNORMALTARGETGENERATOR_H
