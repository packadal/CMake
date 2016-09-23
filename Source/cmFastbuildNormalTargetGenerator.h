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

  struct FastbuildTargetNames
  {
    std::string targetNameOut;
    std::string targetNameReal;
    std::string targetNameImport;
    std::string targetNamePDB;
    std::string targetNameSO;

    std::string targetOutput;
    std::string targetOutputReal;
    std::string targetOutputImplib;
    std::string targetOutputDir;
    std::string targetOutputPDBDir;
    std::string targetOutputCompilePDBDir;
  };
  void DetectOutput(FastbuildTargetNames& targetNamesOut,
                    const std::string& configName);

  void DetectLinkerLibPaths(std::string& linkerLibPath,
                            const std::string& configName);
  bool DetectBaseLinkerCommand(std::string& command,
                               const std::string& configName);

  void ComputeLinkCmds(std::vector<std::string>& linkCmds,
                       std::string configName);

  std::string ComputeDefines(const cmSourceFile* source,
                             const std::string& configName,
                             const std::string& language);

  void DetectBaseCompileCommand(std::string& command,
                                const std::string& language);

  void DetectTargetObjectDependencies(const std::string& configName,
                                      std::vector<std::string>& dependencies);

  void DetectTargetLinkDependencies(const std::string& configName,
                                    std::vector<std::string>& dependencies);

  std::string DetectTargetCompileOutputDir(std::string configName) const;

  static void UnescapeFastbuildVariables(std::string& string);

  static bool isConfigDependant(const cmCustomCommandGenerator* ccg);

  static std::string BuildCommandLine(
    const std::vector<std::string>& cmdLines);

  static void SplitExecutableAndFlags(const std::string& command,
                                      std::string& executable,
                                      std::string& options);

  static void EnsureDirectoryExists(const std::string& path,
                                    const char* homeOutputDirectory);

  static std::string GetLastFolderName(const std::string& string);

  static void ResolveFastbuildVariables(std::string& string,
                                        const std::string& configName);

  cmGlobalFastbuildGenerator::Detail::FileContext& m_fileContext;
  cmGlobalFastbuildGenerator::Detail::Generation::CustomCommandAliasMap
    m_customCommandAliases;
};

#endif // CMFASTBUILDNORMALTARGETGENERATOR_H
