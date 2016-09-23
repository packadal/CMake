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

#include "cmGeneratedFileStream.h"
#include "cmGlobalCommonGenerator.h"
#include "cmGlobalGenerator.h"
#include "cmLocalCommonGenerator.h"
#include "cmLocalFastbuildGenerator.h"

class cmGlobalGeneratorFactory;
class cmCustomCommandGenerator;

/** \class cmGlobalFastbuildGenerator
 * \brief Class for global fastbuild generator.
 */
class cmGlobalFastbuildGenerator : public cmGlobalCommonGenerator
{
public:
  cmGlobalFastbuildGenerator(cmake* cm);
  virtual ~cmGlobalFastbuildGenerator();

  static cmGlobalGeneratorFactory* NewFactory();

  void EnableLanguage(std::vector<std::string> const& lang, cmMakefile* mf,
                      bool optional);
  virtual void Generate();
  virtual void GenerateBuildCommand(
    std::vector<std::string>& makeCommand, const std::string& makeProgram,
    const std::string& projectName, const std::string& projectDir,
    const std::string& targetName, const std::string& config, bool fast,
    bool verbose, std::vector<std::string> const& makeOptions);

  ///! create the correct local generator
  virtual cmLocalGenerator* CreateLocalGenerator(cmMakefile* makefile);
  virtual std::string GetName() const
  {
    return cmGlobalFastbuildGenerator::GetActualName();
  }

  virtual bool IsMultiConfig() { return true; }

  virtual void AppendDirectoryForConfig(const std::string& prefix,
                                        const std::string& config,
                                        const std::string& suffix,
                                        std::string& dir);

  virtual void ComputeTargetObjectDirectory(cmGeneratorTarget*) const;
  virtual const char* GetCMakeCFGIntDir() const;

  virtual void GetTargetSets(TargetDependSet& projectTargets,
                             TargetDependSet& originalTargets,
                             cmLocalGenerator* root, GeneratorVector const&);

  static std::string GetActualName() { return "Fastbuild"; }

  /// Overloaded methods. @see cmGlobalGenerator::GetDocumentation()
  static void GetDocumentation(cmDocumentationEntry& entry);

  static bool SupportsToolset() { return false; }

  class Detail
  {
  public:
    class FileContext
    {
    public:
      FileContext();
      void setFileName(const std::string fileName);
      void close();
      void WriteComment(const std::string& comment);
      void WriteBlankLine();
      void WriteHorizontalLine();
      void WriteSectionHeader(const char* section);
      void WritePushScope(char begin = '{', char end = '}');
      void WritePushScopeStruct();
      void WritePopScope();
      void WriteVariable(const std::string& key, const std::string& value,
                         const std::string& operation = "=");
      void WriteCommand(const std::string& command,
                        const std::string& value = std::string());
      void WriteArray(const std::string& key,
                      const std::vector<std::string>& values,
                      const std::string& operation = "=");

    private:
      cmGeneratedFileStream fout;
      std::string linePrefix;
      std::string closingScope;
    };
    class Definition;
    class Detection
    {
    public:
      static std::string GetLastFolderName(const std::string& string);

      static bool IsExcludedFromAll(cmGeneratorTarget* target);

      static void UnescapeFastbuildVariables(std::string& string);

      static void ResolveFastbuildVariables(std::string& string,
                                            const std::string& configName);

      static std::string BuildCommandLine(
        const std::vector<std::string>& cmdLines);

      static void DetectConfigurations(
        cmGlobalFastbuildGenerator* self, cmMakefile* mf,
        std::vector<std::string>& configurations);

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

      static void DetectOutput(cmLocalCommonGenerator* lg,
                               FastbuildTargetNames& targetNamesOut,
                               const cmGeneratorTarget* generatorTarget,
                               const std::string& configName);

      static void ComputeLinkCmds(std::vector<std::string>& linkCmds,
                                  cmLocalCommonGenerator* lg,
                                  const cmGeneratorTarget* gt,
                                  std::string configName);

      static std::string ComputeDefines(
        cmLocalCommonGenerator* lg, const cmGeneratorTarget* generatorTarget,
        const cmSourceFile* source, const std::string& configName,
        const std::string& language);

      static void DetectLinkerLibPaths(
        std::string& linkerLibPath, cmLocalCommonGenerator* lg,
        const cmGeneratorTarget* generatorTarget,
        const std::string& configName);

      static bool DetectBaseLinkerCommand(std::string& command,
                                          cmLocalFastbuildGenerator* lg,
                                          const cmGeneratorTarget* gt,
                                          const std::string& configName);

      static void SplitExecutableAndFlags(const std::string& command,
                                          std::string& executable,
                                          std::string& options);

      static void DetectBaseCompileCommand(
        std::string& command, cmLocalFastbuildGenerator* lg,
        const cmGeneratorTarget* generatorTarget, const std::string& language);

      static void DetectLanguages(std::set<std::string>& languages,
                                  cmGlobalCommonGenerator* self,
                                  const cmGeneratorTarget* generatorTarget);

      static void FilterSourceFiles(
        std::vector<cmSourceFile const*>& filteredSourceFiles,
        std::vector<cmSourceFile const*>& sourceFiles,
        const std::string& language);

      static void DetectCompilerFlags(std::string& compileFlags,
                                      cmLocalCommonGenerator* lg,
                                      const cmGeneratorTarget* gt,
                                      const cmSourceFile* source,
                                      const std::string& language,
                                      const std::string& configName);

      static void DetectTargetLinkDependencies(
        const cmGeneratorTarget* generatorTarget,
        const std::string& configName, std::vector<std::string>& dependencies);

      static std::string DetectTargetCompileOutputDir(
        cmLocalCommonGenerator* lg, const cmGeneratorTarget* generatorTarget,
        std::string configName);

      static void DetectTargetObjectDependencies(
        cmGlobalCommonGenerator* gg, const cmGeneratorTarget* gt,
        const std::string& configName, std::vector<std::string>& dependencies);

      struct DependencySorter
      {
        struct TargetHelper
        {
          cmGlobalFastbuildGenerator* gg;

          void GetOutputs(const cmGeneratorTarget* entry,
                          std::vector<std::string>& outputs);

          void GetInputs(const cmGeneratorTarget* entry,
                         std::vector<std::string>& inputs);
        };

        struct CustomCommandHelper
        {
          cmGlobalFastbuildGenerator* gg;
          cmLocalCommonGenerator* lg;
          const std::string& configName;

          void GetOutputs(const cmSourceFile* entry,
                          std::vector<std::string>& outputs);
          void GetInputs(const cmSourceFile* entry,
                         std::vector<std::string>& inputs);
        };

        template <class TType, class TTypeHelper>
        static void Sort(TTypeHelper& helper, std::vector<TType*>& entries);
      };

      typedef std::vector<const cmGeneratorTarget*> OrderedTargetSet;
      static void ComputeTargetOrderAndDependencies(
        cmGlobalFastbuildGenerator* gg, OrderedTargetSet& orderedTargets);

      // Iterate over all targets and remove the ones that are
      // not needed for generation.
      // i.e. the nested global targets
      struct RemovalTest
      {
        bool operator()(const cmGeneratorTarget* target) const;
      };

      static void StripNestedGlobalTargets(OrderedTargetSet& orderedTargets);

      static bool isConfigDependant(const cmCustomCommandGenerator* ccg);

      static void DetectCompilerExtraFiles(
        const std::string& compilerID, const std::string& version,
        std::vector<std::string>& extraFiles);
    };

    class Generation
    {
    public:
      struct TargetGenerationContext
      {
        cmGeneratorTarget* target;
        cmLocalCommonGenerator* root;
        std::vector<cmLocalGenerator*> generators;
        cmLocalCommonGenerator* lg;
      };
      typedef std::map<const cmGeneratorTarget*, TargetGenerationContext>
        TargetContextMap;
      typedef std::map<const cmCustomCommand*, std::set<std::string> >
        CustomCommandAliasMap;
      typedef Detection::OrderedTargetSet OrderedTargets;

      struct GenerationContext
      {
        GenerationContext(cmGlobalFastbuildGenerator* globalGen,
                          cmLocalCommonGenerator* localGen,
                          FileContext& fileCtx)
          : self(globalGen)
          , root(localGen)
          , fc(fileCtx)
        {
        }
        cmGlobalFastbuildGenerator* self;
        cmLocalCommonGenerator* root;
        FileContext& fc;
        OrderedTargets orderedTargets;
        TargetContextMap targetContexts;
        CustomCommandAliasMap customCommandAliases;
      };

      static std::string Quote(const std::string& str,
                               const std::string& quotation = "'");

      static std::string Join(const std::vector<std::string>& elems,
                              const std::string& delim);

      struct WrapHelper
      {
        std::string m_prefix;
        std::string m_suffix;

        std::string operator()(const std::string& in)
        {
          return m_prefix + in + m_suffix;
        }
      };

      static std::vector<std::string> Wrap(const std::vector<std::string>& in,
                                           const std::string& prefix = "'",
                                           const std::string& suffix = "'");

      static std::string EncodeLiteral(const std::string& lit);

      static void EnsureDirectoryExists(const std::string& path,
                                        const char* homeOutputDirectory);
      static void BuildTargetContexts(cmGlobalFastbuildGenerator* gg,
                                      TargetContextMap& map);

      static void GenerateRootBFF(cmGlobalFastbuildGenerator* self);

      static void WriteRootBFF(GenerationContext& context);

      static void WritePlaceholders(GenerationContext& context);

      static void WriteSettings(GenerationContext& context);

      struct CompilerDef
      {
        std::string name;
        std::string path;
        std::string cmakeCompilerID;
        std::string cmakeCompilerVersion;
      };

      static bool WriteCompilers(GenerationContext& context);

      static void WriteConfigurations(GenerationContext& context);

      static void WriteCustomCommand(GenerationContext& context,
                                     const cmCustomCommand* cc,
                                     cmLocalCommonGenerator* lg,
                                     const std::string& configName,
                                     std::string& targetName,
                                     const std::string& hostTargetName);

      static void WriteCustomBuildSteps(
        GenerationContext& context, cmLocalCommonGenerator* lg,
        const cmGeneratorTarget* generatorTarget,
        const std::vector<cmCustomCommand>& commands,
        const std::string& buildStep,
        const std::vector<std::string>& orderDeps);

      static bool WriteCustomBuildRules(GenerationContext& context,
                                        cmLocalCommonGenerator* lg,
                                        const cmGeneratorTarget* gt);

      struct CompileCommand
      {
        std::string defines;
        std::string flags;
        std::map<std::string, std::vector<std::string> > sourceFiles;
      };

      static void WriteTargetDefinition(
        GenerationContext& context, cmLocalCommonGenerator* lg,
        const cmGeneratorTarget* generatorTarget)
      {
      }

      static void WriteTargetAliases(
        GenerationContext& context, const cmGeneratorTarget* generatorTarget,
        const std::vector<std::string>& linkableDeps,
        const std::vector<std::string>& orderDeps);

      static void WriteTargetUtilityDefinition(
        GenerationContext& context, cmLocalCommonGenerator* lg,
        const cmGeneratorTarget* generatorTarget);
      static void WriteTargetDefinitions(GenerationContext& context,
                                         bool outputGlobals);

      static void WriteAliases(GenerationContext& context, bool outputGlobals);
    };
  };

  Detail::FileContext g_fc;

private:
  class Factory;
  class Detail;

  std::vector<std::string> Configurations;
};

#endif
