#include "cmFastbuildTargetGenerator.h"

#include "cmComputeLinkInformation.h"
#include "cmCustomCommandGenerator.h"
#include "cmGeneratorTarget.h"
#include "cmMakefile.h"
#include "cmSourceFile.h"

#define FASTBUILD_DOLLAR_TAG "FASTBUILD_DOLLAR_TAG"

cmGlobalFastbuildGenerator::Detail::Generation::CustomCommandAliasMap
  cmFastbuildTargetGenerator::s_customCommandAliases;

cmFastbuildTargetGenerator::cmFastbuildTargetGenerator(cmGeneratorTarget* gt)
  : cmCommonTargetGenerator(gt)
  , m_fileContext(((cmGlobalFastbuildGenerator*)GlobalGenerator)->g_fc)
{
}

void cmFastbuildTargetGenerator::DetectTargetCompileDependencies(
  cmGlobalCommonGenerator* gg, std::vector<std::string>& dependencies)
{
  if (GeneratorTarget->GetType() == cmState::GLOBAL_TARGET) {
    // Global targets only depend on other utilities, which may not appear in
    // the TargetDepends set (e.g. "all").
    std::set<std::string> const& utils = GeneratorTarget->GetUtilities();
    std::copy(utils.begin(), utils.end(), std::back_inserter(dependencies));
  } else {
    cmTargetDependSet const& targetDeps =
      gg->GetTargetDirectDepends(GeneratorTarget);
    for (cmTargetDependSet::const_iterator i = targetDeps.begin();
         i != targetDeps.end(); ++i) {
      const cmTargetDepend& depTarget = *i;
      if (depTarget->GetType() == cmState::INTERFACE_LIBRARY) {
        continue;
      }
      dependencies.push_back(depTarget->GetName());
    }
  }
}

void cmFastbuildTargetGenerator::WriteTargetAliases(
  const std::vector<std::string>& linkableDeps,
  const std::vector<std::string>& orderDeps)
{
  const std::string& targetName = GeneratorTarget->GetName();

  std::vector<std::string> configs;
  Makefile->GetConfigurations(configs, false);
  for (std::vector<std::string>::const_iterator iter = configs.begin();
       iter != configs.end(); ++iter) {
    const std::string& configName = *iter;

    if (!linkableDeps.empty()) {
      m_fileContext.WriteCommand(
        "Alias", cmGlobalFastbuildGenerator::Quote(targetName + "-" +
                                                   configName + "-products"));
      m_fileContext.WritePushScope();
      m_fileContext.WriteArray(
        "Targets",
        cmGlobalFastbuildGenerator::Wrap(linkableDeps, "'" + targetName + "-",
                                         "-" + configName + "'"));
      m_fileContext.WritePopScope();
    }

    if (!orderDeps.empty() || !linkableDeps.empty()) {
      m_fileContext.WriteCommand("Alias", cmGlobalFastbuildGenerator::Quote(
                                            targetName + "-" + configName));
      m_fileContext.WritePushScope();
      m_fileContext.WriteArray(
        "Targets",
        cmGlobalFastbuildGenerator::Wrap(linkableDeps, "'" + targetName + "-",
                                         "-" + configName + "'"));
      m_fileContext.WriteArray("Targets", cmGlobalFastbuildGenerator::Wrap(
                                            orderDeps, "'" + targetName + "-",
                                            "-" + configName + "'"),
                               "+");
      m_fileContext.WritePopScope();
    }
  }
}

void cmFastbuildTargetGenerator::WriteCustomBuildSteps(
  const std::vector<cmCustomCommand>& commands, const std::string& buildStep,
  const std::vector<std::string>& orderDeps)
{
  if (commands.empty()) {
    return;
  }

  const std::string& targetName = GeneratorTarget->GetName();

  // Now output the commands
  std::vector<std::string> configs;
  Makefile->GetConfigurations(configs, false);
  for (std::vector<std::string>::const_iterator iter = configs.begin();
       iter != configs.end(); ++iter) {
    const std::string& configName = *iter;

    m_fileContext.WriteVariable("buildStep_" + buildStep + "_" + configName,
                                "");
    m_fileContext.WritePushScopeStruct();

    m_fileContext.WriteCommand("Using", ".BaseConfig_" + configName);

    m_fileContext.WriteArray(
      "PreBuildDependencies",
      cmGlobalFastbuildGenerator::Wrap(orderDeps, "'", "-" + configName + "'"),
      "+");

    std::string baseName = targetName + "-" + buildStep + "-" + configName;
    int commandCount = 1;
    std::vector<std::string> customCommandTargets;
    for (std::vector<cmCustomCommand>::const_iterator ccIter =
           commands.begin();
         ccIter != commands.end(); ++ccIter) {
      const cmCustomCommand& cc = *ccIter;

      std::stringstream customCommandTargetName;
      customCommandTargetName << baseName << (commandCount++);

      std::string customCommandTargetNameStr = customCommandTargetName.str();
      WriteCustomCommand(&cc, configName, customCommandTargetNameStr,
                         targetName);
      customCommandTargets.push_back(customCommandTargetNameStr);
    }

    // Write an alias for this object group to group them all together
    m_fileContext.WriteCommand("Alias",
                               cmGlobalFastbuildGenerator::Quote(baseName));
    m_fileContext.WritePushScope();
    m_fileContext.WriteArray("Targets", cmGlobalFastbuildGenerator::Wrap(
                                          customCommandTargets, "'", "'"));
    m_fileContext.WritePopScope();

    m_fileContext.WritePopScope();
  }
}

void cmFastbuildTargetGenerator::AddIncludeFlags(std::string& /* flags */,
                                                 const std::string& /* lang */)
{
}

bool cmFastbuildTargetGenerator::WriteCustomBuildRules()
{
  bool hasCustomCommands = false;
  const std::string& targetName = GeneratorTarget->GetName();

  // Iterating over all configurations
  std::vector<std::string> configs;
  Makefile->GetConfigurations(configs, false);
  for (std::vector<std::string>::const_iterator iter = configs.begin();
       iter != configs.end(); ++iter) {
    std::string configName = *iter;

    m_fileContext.WriteVariable("CustomCommands_" + configName, "");
    m_fileContext.WritePushScopeStruct();

    m_fileContext.WriteCommand("Using", ".BaseConfig_" + configName);

    // Figure out the list of custom build rules in use by this target
    // get a list of source files
    std::vector<cmSourceFile const*> customCommands;
    GeneratorTarget->GetCustomCommands(customCommands, configName);

    if (!customCommands.empty()) {
      // Presort the commands to adjust for dependencies
      // In a number of cases, the commands inputs will be the outputs
      // from another command. Need to sort the commands to output them in
      // order.
      cmGlobalFastbuildGenerator::Detail::Detection::DependencySorter::
        CustomCommandHelper ccHelper = { GlobalGenerator, LocalGenerator,
                                         configName };
      cmGlobalFastbuildGenerator::Detail::Detection::DependencySorter::Sort(
        ccHelper, customCommands);

      std::vector<std::string> customCommandTargets;

      // Write the custom command build rules for each configuration
      int commandCount = 1;
      std::string customCommandNameBase = "CustomCommand-" + configName + "-";
      for (std::vector<cmSourceFile const*>::iterator ccIter =
             customCommands.begin();
           ccIter != customCommands.end(); ++ccIter) {
        const cmSourceFile* sourceFile = *ccIter;

        std::stringstream customCommandTargetName;
        customCommandTargetName << customCommandNameBase << (commandCount++);
        customCommandTargetName
          << "-" << cmSystemTools::GetFilenameName(sourceFile->GetFullPath());
        ;

        std::string customCommandTargetNameStr = customCommandTargetName.str();
        WriteCustomCommand(sourceFile->GetCustomCommand(), configName,
                           customCommandTargetNameStr, targetName);

        customCommandTargets.push_back(customCommandTargetNameStr);
      }

      std::string customCommandGroupName =
        targetName + "-CustomCommands-" + configName;

      // Write an alias for this object group to group them all together
      m_fileContext.WriteCommand(
        "Alias", cmGlobalFastbuildGenerator::Quote(customCommandGroupName));
      m_fileContext.WritePushScope();
      m_fileContext.WriteArray("Targets", cmGlobalFastbuildGenerator::Wrap(
                                            customCommandTargets, "'", "'"));
      m_fileContext.WritePopScope();

      // Now make everything use this as prebuilt dependencies
      std::vector<std::string> tmp;
      tmp.push_back(customCommandGroupName);
      m_fileContext.WriteArray("PreBuildDependencies",
                               cmGlobalFastbuildGenerator::Wrap(tmp), "+");

      hasCustomCommands = true;
    }

    m_fileContext.WritePopScope();
  }

  return hasCustomCommands;
}

void cmFastbuildTargetGenerator::WriteCustomCommand(
  const cmCustomCommand* cc, const std::string& configName,
  std::string& targetName, const std::string& hostTargetName)
{
  // We need to generate the command for execution.
  cmCustomCommandGenerator ccg(*cc, configName, LocalGenerator);

  const std::vector<std::string>& outputs = ccg.GetOutputs();
  const std::vector<std::string>& byproducts = ccg.GetByproducts();
  std::vector<std::string> mergedOutputs;
  mergedOutputs.insert(mergedOutputs.end(), outputs.begin(), outputs.end());
  mergedOutputs.insert(mergedOutputs.end(), byproducts.begin(),
                       byproducts.end());

  // TODO: Double check that none of the outputs are 'symbolic'
  // In which case, FASTBuild won't want them treated as
  // outputs.
  {
    // Loop through all outputs, and attempt to find it in the
    // source files.
    for (size_t index = 0; index < mergedOutputs.size(); ++index) {
      const std::string& outputName = mergedOutputs[index];

      cmSourceFile* outputSourceFile = Makefile->GetSource(outputName);
      // Check if this file is symbolic
      if (outputSourceFile &&
          outputSourceFile->GetPropertyAsBool("SYMBOLIC")) {
        // We need to remove this file from the list of outputs
        // Swap with back and pop
        mergedOutputs[index] = mergedOutputs.back();
        mergedOutputs.pop_back();
      }
    }
  }

  std::vector<std::string> inputs;
  std::vector<std::string> orderDependencies;

  // If this exec node always generates outputs,
  // then we need to make sure we don't define outputs multiple times.
  // but if the command should always run (i.e. post builds etc)
  // then we will output a new one.
  if (!mergedOutputs.empty()) {
    // Check if this custom command has already been output.
    // If it has then just drop an alias here to the original
    cmGlobalFastbuildGenerator::Detail::Generation::CustomCommandAliasMap::
      iterator findResult = s_customCommandAliases.find(cc);
    if (findResult != s_customCommandAliases.end()) {
      const std::set<std::string>& aliases = findResult->second;
      if (aliases.find(targetName) != aliases.end()) {
        // This target has already been generated
        // with the correct name somewhere else.
        return;
      }
      if (!isConfigDependant(&ccg)) {
        // This command has already been generated.
        // But under a different name so setup an alias to redirect
        // No merged outputs, so this command must always be run.
        // Make it's name unique to its host target
        targetName += "-";
        targetName += hostTargetName;

        std::vector<std::string> targets;
        targets.push_back(*findResult->second.begin());

        m_fileContext.WriteCommand(
          "Alias", cmGlobalFastbuildGenerator::Quote(targetName));
        m_fileContext.WritePushScope();
        {
          m_fileContext.WriteArray("Targets",
                                   cmGlobalFastbuildGenerator::Wrap(targets));
        }
        m_fileContext.WritePopScope();
        return;
      }
    }
    s_customCommandAliases[cc].insert(targetName);
  } else {
    // No merged outputs, so this command must always be run.
    // Make it's name unique to its host target
    targetName += "-";
    targetName += hostTargetName;
  }

  // Take the dependencies listed and split into targets and files.
  const std::vector<std::string>& depends = ccg.GetDepends();
  for (std::vector<std::string>::const_iterator iter = depends.begin();
       iter != depends.end(); ++iter) {
    const std::string& dep = *iter;

    bool isTarget = GlobalGenerator->FindTarget(dep) != NULL;
    if (isTarget) {
      orderDependencies.push_back(dep + "-" + configName);
    } else {
      inputs.push_back(dep);
    }
  }

#ifdef _WIN32
  const std::string shellExt = ".bat";
#else
  const std::string shellExt = ".sh";
#endif

  std::string workingDirectory = ccg.GetWorkingDirectory();
  if (workingDirectory.empty()) {
    workingDirectory = Makefile->GetCurrentBinaryDirectory();
    workingDirectory += "/";
  }

  std::string scriptFileName(workingDirectory + targetName + ".bat");
  cmsys::ofstream scriptFile(scriptFileName.c_str());

  for (unsigned i = 0; i != ccg.GetNumberOfCommands(); ++i) {
    std::string args;
    ccg.AppendArguments(i, args);
    cmSystemTools::ReplaceString(args, "$$", "$");
    cmSystemTools::ReplaceString(args, FASTBUILD_DOLLAR_TAG, "$");
#ifdef _WIN32
    // in windows batch, '%' is a special character that needs to be doubled
    // to be escaped
    cmSystemTools::ReplaceString(args, "%", "%%");
#endif
    ResolveFastbuildVariables(args, configName);

    std::string command(ccg.GetCommand(i));
    cmSystemTools::ReplaceString(command, FASTBUILD_DOLLAR_TAG, "$");
    ResolveFastbuildVariables(command, configName);

    scriptFile << cmGlobalFastbuildGenerator::Quote(command, "\"") << args
               << std::endl;
  }

  // Write out an exec command
  /*
  Exec(alias); (optional)Alias
  {
      .ExecExecutable; Executable to run
      .ExecInput; Input file to pass to executable
      .ExecOutput; Output file generated by executable
      .ExecArguments; (optional)Arguments to pass to executable
      .ExecWorkingDir; (optional)Working dir to set for executable

      ; Additional options
      .PreBuildDependencies; (optional)Force targets to be built before this
  Exec(Rarely needed,
      ; but useful when Exec relies on externally generated files).
  }
  */

  std::for_each(inputs.begin(), inputs.end(), &UnescapeFastbuildVariables);
  std::for_each(mergedOutputs.begin(), mergedOutputs.end(),
                &UnescapeFastbuildVariables);

  m_fileContext.WriteCommand("Exec",
                             cmGlobalFastbuildGenerator::Quote(targetName));
  m_fileContext.WritePushScope();
  {
#ifdef _WIN32
    m_fileContext.WriteVariable("ExecExecutable",
                                cmGlobalFastbuildGenerator::Quote(
                                  cmSystemTools::FindProgram("cmd.exe")));
    m_fileContext.WriteVariable(
      "ExecArguments",
      cmGlobalFastbuildGenerator::Quote("/C " + scriptFileName));
#else
      m_fileContext.WriteVariable("ExecExecutable", cmGlobalFastbuildGenerator::Quote(scriptFileName));
#endif
    if (!workingDirectory.empty()) {
      m_fileContext.WriteVariable(
        "ExecWorkingDir", cmGlobalFastbuildGenerator::Quote(workingDirectory));
    }

    if (inputs.empty()) {
      // inputs.push_back("dummy-in");
    }
    m_fileContext.WriteArray("ExecInput",
                             cmGlobalFastbuildGenerator::Wrap(inputs));

    if (mergedOutputs.empty()) {
      m_fileContext.WriteVariable("ExecUseStdOutAsOutput", "true");

      std::string outputDir =
        LocalGenerator->GetMakefile()->GetHomeOutputDirectory();
      mergedOutputs.push_back(outputDir + "/dummy-out-" + targetName + ".txt");
    }
    // Currently fastbuild doesn't support more than 1
    // output for a custom command (soon to change hopefully).
    // so only use the first one
    m_fileContext.WriteVariable(
      "ExecOutput", cmGlobalFastbuildGenerator::Quote(mergedOutputs[0]));
  }
  m_fileContext.WritePopScope();
}

void cmFastbuildTargetGenerator::DetectOutput(
  FastbuildTargetNames& targetNamesOut, const std::string& configName)
{
  if (GeneratorTarget->GetType() == cmState::EXECUTABLE) {
    GeneratorTarget->GetExecutableNames(
      targetNamesOut.targetNameOut, targetNamesOut.targetNameReal,
      targetNamesOut.targetNameImport, targetNamesOut.targetNamePDB,
      configName);
  } else {
    GeneratorTarget->GetLibraryNames(
      targetNamesOut.targetNameOut, targetNamesOut.targetNameSO,
      targetNamesOut.targetNameReal, targetNamesOut.targetNameImport,
      targetNamesOut.targetNamePDB, configName);
  }

  if (GeneratorTarget->HaveWellDefinedOutputFiles()) {
    targetNamesOut.targetOutputDir =
      GeneratorTarget->GetSupportDirectory() + "/";

    targetNamesOut.targetOutput = GeneratorTarget->GetFullPath(configName);
    targetNamesOut.targetOutputReal =
      GeneratorTarget->GetFullPath(configName,
                                   /*implib=*/false,
                                   /*realpath=*/true);
    targetNamesOut.targetOutputImplib =
      GeneratorTarget->GetFullPath(configName,
                                   /*implib=*/true);
  } else {
    targetNamesOut.targetOutputDir = GeneratorTarget->GetSupportDirectory();
    if (targetNamesOut.targetOutputDir.empty() ||
        targetNamesOut.targetOutputDir == ".") {
      targetNamesOut.targetOutputDir = GeneratorTarget->GetName();
    } else {
      targetNamesOut.targetOutputDir += "/";
      targetNamesOut.targetOutputDir += GeneratorTarget->GetName();
    }
    targetNamesOut.targetOutputDir += "/";
    targetNamesOut.targetOutputDir += configName;
    targetNamesOut.targetOutputDir += "/";

    targetNamesOut.targetOutput =
      targetNamesOut.targetOutputDir + "/" + targetNamesOut.targetNameOut;
    targetNamesOut.targetOutputImplib =
      targetNamesOut.targetOutputDir + "/" + targetNamesOut.targetNameImport;
    targetNamesOut.targetOutputReal =
      targetNamesOut.targetOutputDir + "/" + targetNamesOut.targetNameReal;
  }

  if (GeneratorTarget->GetType() == cmState::EXECUTABLE ||
      GeneratorTarget->GetType() == cmState::STATIC_LIBRARY ||
      GeneratorTarget->GetType() == cmState::SHARED_LIBRARY ||
      GeneratorTarget->GetType() == cmState::MODULE_LIBRARY) {
    targetNamesOut.targetOutputPDBDir =
      GeneratorTarget->GetPDBDirectory(configName);
    targetNamesOut.targetOutputPDBDir += "/";
  }
  if (GeneratorTarget->GetType() <= cmState::OBJECT_LIBRARY) {
    targetNamesOut.targetOutputCompilePDBDir =
      GeneratorTarget->GetCompilePDBPath(configName);
    if (targetNamesOut.targetOutputCompilePDBDir.empty()) {
      targetNamesOut.targetOutputCompilePDBDir =
        GeneratorTarget->GetSupportDirectory() + "/";
    }
  }

  // Make sure all obey the correct slashes
  cmSystemTools::ConvertToOutputSlashes(targetNamesOut.targetOutput);
  cmSystemTools::ConvertToOutputSlashes(targetNamesOut.targetOutputImplib);
  cmSystemTools::ConvertToOutputSlashes(targetNamesOut.targetOutputReal);
  cmSystemTools::ConvertToOutputSlashes(targetNamesOut.targetOutputDir);
  cmSystemTools::ConvertToOutputSlashes(targetNamesOut.targetOutputPDBDir);
  cmSystemTools::ConvertToOutputSlashes(
    targetNamesOut.targetOutputCompilePDBDir);
}

void cmFastbuildTargetGenerator::DetectLinkerLibPaths(
  std::string& linkerLibPath, const std::string& configName)
{
  cmComputeLinkInformation* pcli =
    GeneratorTarget->GetLinkInformation(configName);
  if (!pcli) {
    // No link information, then no linker library paths
    return;
  }
  cmComputeLinkInformation& cli = *pcli;

  std::string libPathFlag =
    Makefile->GetRequiredDefinition("CMAKE_LIBRARY_PATH_FLAG");
  std::string libPathTerminator =
    Makefile->GetSafeDefinition("CMAKE_LIBRARY_PATH_TERMINATOR");

  // Append the library search path flags.
  std::vector<std::string> const& libDirs = cli.GetDirectories();
  for (std::vector<std::string>::const_iterator libDir = libDirs.begin();
       libDir != libDirs.end(); ++libDir) {
    std::string libpath = LocalGenerator->ConvertToOutputForExisting(
      *libDir, cmLocalGenerator::SHELL);
    cmSystemTools::ConvertToOutputSlashes(libpath);

    // Add the linker lib path twice, once raw, then once with
    // the configname attached
    std::string configlibpath = libpath + "/" + configName;
    cmSystemTools::ConvertToOutputSlashes(configlibpath);

    linkerLibPath += " " + libPathFlag;
    linkerLibPath += libpath;
    linkerLibPath += libPathTerminator;

    linkerLibPath += " " + libPathFlag;
    linkerLibPath += configlibpath;
    linkerLibPath += libPathTerminator;
    linkerLibPath += " ";
  }
}

bool cmFastbuildTargetGenerator::DetectBaseLinkerCommand(
  std::string& command, const std::string& configName)
{
  const std::string& linkLanguage =
    GeneratorTarget->GetLinkerLanguage(configName);
  if (linkLanguage.empty()) {
    cmSystemTools::Error("CMake can not determine linker language for "
                         "target: ",
                         GeneratorTarget->GetName().c_str());
    return false;
  }

  this->GeneratorTarget->GetLinkInformation(this->GetConfigName());

  cmLocalGenerator::RuleVariables vars;
  vars.RuleLauncher = "RULE_LAUNCH_LINK";
  vars.CMTarget = GeneratorTarget;
  vars.Language = linkLanguage.c_str();
  const std::string manifests = this->GetManifests();
  vars.Manifests = manifests.c_str();

  std::string responseFlag;
  vars.Objects =
    FASTBUILD_DOLLAR_TAG "FB_INPUT_1_PLACEHOLDER" FASTBUILD_DOLLAR_TAG;
  vars.LinkLibraries = "";

  vars.ObjectDir = FASTBUILD_DOLLAR_TAG "TargetOutDir" FASTBUILD_DOLLAR_TAG;
  vars.Target =
    FASTBUILD_DOLLAR_TAG "FB_INPUT_2_PLACEHOLDER" FASTBUILD_DOLLAR_TAG;

  vars.TargetSOName = FASTBUILD_DOLLAR_TAG "TargetOutSO" FASTBUILD_DOLLAR_TAG;
  vars.TargetPDB = FASTBUILD_DOLLAR_TAG
    "TargetOutPDBDir" FASTBUILD_DOLLAR_TAG FASTBUILD_DOLLAR_TAG
    "TargetNamePDB" FASTBUILD_DOLLAR_TAG;

  // Setup the target version.
  std::string targetVersionMajor;
  std::string targetVersionMinor;
  {
    std::ostringstream majorStream;
    std::ostringstream minorStream;
    int major;
    int minor;
    GeneratorTarget->GetTargetVersion(major, minor);
    majorStream << major;
    minorStream << minor;
    targetVersionMajor = majorStream.str();
    targetVersionMinor = minorStream.str();
  }
  vars.TargetVersionMajor = targetVersionMajor.c_str();
  vars.TargetVersionMinor = targetVersionMinor.c_str();

  vars.Defines =
    FASTBUILD_DOLLAR_TAG "CompileDefineFlags" FASTBUILD_DOLLAR_TAG;
  vars.Flags = FASTBUILD_DOLLAR_TAG "TargetFlags" FASTBUILD_DOLLAR_TAG;
  vars.LinkFlags = FASTBUILD_DOLLAR_TAG "LinkFlags" FASTBUILD_DOLLAR_TAG
                                        " " FASTBUILD_DOLLAR_TAG
                                        "LinkPath" FASTBUILD_DOLLAR_TAG;
  // Rule for linking library/executable.
  std::vector<std::string> linkCmds;
  ComputeLinkCmds(linkCmds, configName);
  for (std::vector<std::string>::iterator i = linkCmds.begin();
       i != linkCmds.end(); ++i) {
    ((cmLocalFastbuildGenerator*)LocalGenerator)
      ->ExpandRuleVariables(*i, vars);
  }

  command = BuildCommandLine(linkCmds);

  return true;
}

void cmFastbuildTargetGenerator::ComputeLinkCmds(
  std::vector<std::string>& linkCmds, std::string configName)
{
  const std::string& linkLanguage =
    GeneratorTarget->GetLinkerLanguage(configName);
  {
    std::string linkCmdVar =
      GeneratorTarget->GetCreateRuleVariable(linkLanguage, configName);
    const char* linkCmd = Makefile->GetDefinition(linkCmdVar);
    if (linkCmd) {
      cmSystemTools::ExpandListArgument(linkCmd, linkCmds);
      return;
    }
  }

  // If the above failed, then lets try this:
  switch (GeneratorTarget->GetType()) {
    case cmState::STATIC_LIBRARY: {
      // We have archive link commands set. First, delete the existing
      // archive.
      {
        std::string cmakeCommand = LocalGenerator->ConvertToOutputFormat(
          Makefile->GetRequiredDefinition("CMAKE_COMMAND"),
          cmLocalGenerator::SHELL);
        linkCmds.push_back(cmakeCommand + " -E remove $TARGET_FILE");
      }
      // TODO: Use ARCHIVE_APPEND for archives over a certain size.
      {
        std::string linkCmdVar = "CMAKE_";
        linkCmdVar += linkLanguage;
        linkCmdVar += "_ARCHIVE_CREATE";
        const char* linkCmd = Makefile->GetRequiredDefinition(linkCmdVar);
        cmSystemTools::ExpandListArgument(linkCmd, linkCmds);
      }
      {
        std::string linkCmdVar = "CMAKE_";
        linkCmdVar += linkLanguage;
        linkCmdVar += "_ARCHIVE_FINISH";
        const char* linkCmd = Makefile->GetRequiredDefinition(linkCmdVar);
        cmSystemTools::ExpandListArgument(linkCmd, linkCmds);
      }
      return;
    }
    case cmState::SHARED_LIBRARY:
    case cmState::MODULE_LIBRARY:
    case cmState::EXECUTABLE:
      break;
    default:
      assert(0 && "Unexpected target type");
  }
  return;
}

std::string cmFastbuildTargetGenerator::ComputeDefines(
  const cmSourceFile* source, const std::string& configName,
  const std::string& language)
{
  std::set<std::string> defines;

  // Add the export symbol definition for shared library objects.
  if (const char* exportMacro = GeneratorTarget->GetExportMacro()) {
    LocalGenerator->AppendDefines(defines, exportMacro);
  }

  // Add preprocessor definitions for this target and configuration.
  LocalGenerator->AddCompileDefinitions(defines, GeneratorTarget, configName,
                                        language);

  if (source) {
    LocalGenerator->AppendDefines(defines,
                                  source->GetProperty("COMPILE_DEFINITIONS"));

    std::string defPropName = "COMPILE_DEFINITIONS_";
    defPropName += cmSystemTools::UpperCase(configName);
    LocalGenerator->AppendDefines(defines, source->GetProperty(defPropName));
  }

  // Add a definition for the configuration name.
  // NOTE: CMAKE_TEST_REQUIREMENT The following was added specifically to
  // facillitate cmake testing. Doesn't feel right to do this...
  std::string configDefine = "CMAKE_INTDIR=\"";
  configDefine += configName;
  configDefine += "\"";
  LocalGenerator->AppendDefines(defines, configDefine);

  std::string definesString;
  LocalGenerator->JoinDefines(defines, definesString, language);

  return definesString;
}

void cmFastbuildTargetGenerator::DetectBaseCompileCommand(
  std::string& command, const std::string& language)
{
  cmLocalGenerator::RuleVariables compileObjectVars;
  compileObjectVars.CMTarget = GeneratorTarget;
  compileObjectVars.Language = language.c_str();
  compileObjectVars.Source =
    FASTBUILD_DOLLAR_TAG "FB_INPUT_1_PLACEHOLDER" FASTBUILD_DOLLAR_TAG;
  compileObjectVars.Object =
    FASTBUILD_DOLLAR_TAG "FB_INPUT_2_PLACEHOLDER" FASTBUILD_DOLLAR_TAG;
  compileObjectVars.ObjectDir =
    FASTBUILD_DOLLAR_TAG "TargetOutputDir" FASTBUILD_DOLLAR_TAG;
  compileObjectVars.ObjectFileDir = "";
  compileObjectVars.Flags = "";
  compileObjectVars.Includes = "";
  const std::string manifests = this->GetManifests();
  compileObjectVars.Manifests = manifests.c_str();
  compileObjectVars.Defines = "";
  compileObjectVars.TargetCompilePDB = FASTBUILD_DOLLAR_TAG
    "TargetOutCompilePDBDir" FASTBUILD_DOLLAR_TAG FASTBUILD_DOLLAR_TAG
    "TargetNamePDB" FASTBUILD_DOLLAR_TAG;

  // Rule for compiling object file.
  std::string compileCmdVar = "CMAKE_";
  compileCmdVar += language;
  compileCmdVar += "_COMPILE_OBJECT";
  std::string compileCmd =
    LocalGenerator->GetMakefile()->GetRequiredDefinition(compileCmdVar);
  std::vector<std::string> compileCmds;
  cmSystemTools::ExpandListArgument(compileCmd, compileCmds);

  for (std::vector<std::string>::iterator i = compileCmds.begin();
       i != compileCmds.end(); ++i) {
    std::string& compileCmdStr = *i;
    ((cmLocalFastbuildGenerator*)LocalGenerator)
      ->ExpandRuleVariables(compileCmdStr, compileObjectVars);
  }

  command = BuildCommandLine(compileCmds);
}

void cmFastbuildTargetGenerator::DetectTargetObjectDependencies(
  const std::string& configName, std::vector<std::string>& dependencies)
{
  // Iterate over all source files and look for
  // object file dependencies

  std::set<std::string> objectLibs;

  std::vector<cmSourceFile*> sourceFiles;
  GeneratorTarget->GetSourceFiles(sourceFiles, configName);
  for (std::vector<cmSourceFile*>::const_iterator i = sourceFiles.begin();
       i != sourceFiles.end(); ++i) {
    const std::string& objectLib = (*i)->GetObjectLibrary();
    if (!objectLib.empty()) {
      // Find the target this actually is (might be an alias)
      const cmTarget* objectTarget = GlobalGenerator->FindTarget(objectLib);
      if (objectTarget) {
        objectLibs.insert(objectTarget->GetName() + "-" + configName +
                          "-products");
      }
    }
  }

  std::copy(objectLibs.begin(), objectLibs.end(),
            std::back_inserter(dependencies));

  // Now add the external obj files that also need to be linked in
  std::vector<const cmSourceFile*> objFiles;
  GeneratorTarget->GetExternalObjects(objFiles, configName);
  for (std::vector<const cmSourceFile*>::const_iterator i = objFiles.begin();
       i != objFiles.end(); ++i) {
    const cmSourceFile* sourceFile = *i;
    if (sourceFile->GetObjectLibrary().empty()) {
      dependencies.push_back(sourceFile->GetFullPath());
    }
  }
}

void cmFastbuildTargetGenerator::DetectTargetLinkDependencies(
  const std::string& configName, std::vector<std::string>& dependencies)
{
  // Static libraries never depend on other targets for linking.
  if (GeneratorTarget->GetType() == cmState::STATIC_LIBRARY ||
      GeneratorTarget->GetType() == cmState::OBJECT_LIBRARY) {
    return;
  }

  cmComputeLinkInformation* cli =
    GeneratorTarget->GetLinkInformation(configName);
  if (!cli) {
    return;
  }

  const std::vector<std::string>& deps = cli->GetDepends();
  std::copy(deps.begin(), deps.end(), std::back_inserter(dependencies));
}

std::string cmFastbuildTargetGenerator::DetectTargetCompileOutputDir(

  std::string configName) const
{
  std::string result =
    LocalGenerator->GetTargetDirectory(GeneratorTarget) + "/";
  if (!configName.empty()) {
    result = result + configName + "/";
  }
  cmSystemTools::ConvertToOutputSlashes(result);
  return result;
}

void cmFastbuildTargetGenerator::UnescapeFastbuildVariables(
  std::string& string)
{
  // Unescape the Fastbuild configName symbol with $
  cmSystemTools::ReplaceString(string, "^", "^^");
  cmSystemTools::ReplaceString(string, "$$", "^$");
  cmSystemTools::ReplaceString(string, FASTBUILD_DOLLAR_TAG, "$");
  // cmSystemTools::ReplaceString(string, "$$ConfigName$$", "$ConfigName$");
  // cmSystemTools::ReplaceString(string, "^$ConfigName^$", "$ConfigName$");
}

bool cmFastbuildTargetGenerator::isConfigDependant(
  const cmCustomCommandGenerator* ccg)
{
  typedef std::vector<std::string> StringVector;
  StringVector outputs = ccg->GetOutputs();
  StringVector byproducts = ccg->GetByproducts();

  std::for_each(outputs.begin(), outputs.end(), &UnescapeFastbuildVariables);
  std::for_each(byproducts.begin(), byproducts.end(),
                &UnescapeFastbuildVariables);

  // Make sure that the outputs don't depend on the config name
  for (StringVector::const_iterator iter = outputs.begin();
       iter != outputs.end(); ++iter) {
    const std::string& str = *iter;
    if (str.find("$ConfigName$") != std::string::npos) {
      return true;
    }
  }
  for (StringVector::const_iterator iter = byproducts.begin();
       iter != byproducts.end(); ++iter) {
    const std::string& str = *iter;
    if (str.find("$ConfigName$") != std::string::npos) {
      return true;
    }
  }

  return false;
}

std::string cmFastbuildTargetGenerator::BuildCommandLine(
  const std::vector<std::string>& cmdLines)
{
#ifdef _WIN32
  const char* cmdExe = "cmd.exe";
  std::string cmdExeAbsolutePath = cmSystemTools::FindProgram(cmdExe);
#endif

  // If we have no commands but we need to build a command anyway, use ":".
  // This happens when building a POST_BUILD value for link targets that
  // don't use POST_BUILD.
  if (cmdLines.empty()) {
#ifdef _WIN32
    return cmdExeAbsolutePath + " /C \"cd .\"";
#else
    return ":";
#endif
  }

  std::ostringstream cmd;
  for (std::vector<std::string>::const_iterator li = cmdLines.begin();
       li != cmdLines.end(); ++li)
#ifdef _WIN32
  {
    if (li != cmdLines.begin()) {
      cmd << " && ";
    } else if (cmdLines.size() > 1) {
      cmd << cmdExeAbsolutePath << " /C \"";
    }
    cmd << *li;
  }
  if (cmdLines.size() > 1) {
    cmd << "\"";
  }
#else
  {
    if (li != cmdLines.begin()) {
      cmd << " && ";
    }
    cmd << *li;
  }
#endif
  std::string cmdOut = cmd.str();
  UnescapeFastbuildVariables(cmdOut);

  return cmdOut;
}

void cmFastbuildTargetGenerator::SplitExecutableAndFlags(
  const std::string& command, std::string& executable, std::string& options)
{
  // Remove the command from the front
  std::vector<std::string> args =
    cmSystemTools::ParseArguments(command.c_str());

  // Join the args together and remove 0 from the front
  std::stringstream argSet;
  std::copy(args.begin() + 1, args.end(),
            std::ostream_iterator<std::string>(argSet, " "));

  executable = args[0];
  options = argSet.str();
}

void cmFastbuildTargetGenerator::EnsureDirectoryExists(
  const std::string& path, const char* homeOutputDirectory)
{
  if (cmSystemTools::FileIsFullPath(path.c_str())) {
    cmSystemTools::MakeDirectory(path.c_str());
  } else {
    const std::string fullPath = std::string(homeOutputDirectory) + "/" + path;
    cmSystemTools::MakeDirectory(fullPath.c_str());
  }
}

std::string cmFastbuildTargetGenerator::GetLastFolderName(
  const std::string& string)
{
  return string.substr(string.rfind('/') + 1);
}

void cmFastbuildTargetGenerator::ResolveFastbuildVariables(
  std::string& string, const std::string& configName)
{ // Replace Fastbuild configName with the config name
  cmSystemTools::ReplaceString(string, "$ConfigName$", configName);
}

void cmFastbuildTargetGenerator::Generate()
{
  // Detection of the link command as follows:
  std::string linkCommand = "Library";
  switch (GeneratorTarget->GetType()) {
    case cmState::INTERFACE_LIBRARY:
      // We don't write out interface libraries.
      return;
    case cmState::EXECUTABLE: {
      linkCommand = "Executable";
      break;
    }
    case cmState::SHARED_LIBRARY: {
      linkCommand = "DLL";
      break;
    }
    case cmState::STATIC_LIBRARY:
    case cmState::MODULE_LIBRARY:
    case cmState::OBJECT_LIBRARY: {
      // No changes required
      break;
    }
    case cmState::UTILITY:
    case cmState::GLOBAL_TARGET: {
      // No link command used
      linkCommand = "NoLinkCommand";
      break;
    }
    case cmState::UNKNOWN_LIBRARY: {
      // Ignoring this target generation...
      return;
    }
  }

  const std::string& targetName = GeneratorTarget->GetName();

  m_fileContext.WriteComment("Target definition: " + targetName);
  m_fileContext.WritePushScope();

  std::vector<std::string> dependencies;
  DetectTargetCompileDependencies(GlobalGenerator, dependencies);

  // Iterate over each configuration
  // This time to define linker settings for each config
  std::vector<std::string> configs;
  Makefile->GetConfigurations(configs, false);
  for (std::vector<std::string>::const_iterator configIter = configs.begin();
       configIter != configs.end(); ++configIter) {
    const std::string& configName = *configIter;

    m_fileContext.WriteVariable("BaseConfig_" + configName, "");
    m_fileContext.WritePushScopeStruct();

    m_fileContext.WriteCommand("Using", ".ConfigBase");

    m_fileContext.WriteVariable("ConfigName",
                                cmGlobalFastbuildGenerator::Quote(configName));

    m_fileContext.WriteBlankLine();
    m_fileContext.WriteComment("General output details:");
    // Write out the output paths for the outcome of this target
    {
      FastbuildTargetNames targetNames;
      DetectOutput(targetNames, configName);

      m_fileContext.WriteVariable(
        "TargetNameOut",
        cmGlobalFastbuildGenerator::Quote(targetNames.targetNameOut));
      m_fileContext.WriteVariable(
        "TargetNameImport",
        cmGlobalFastbuildGenerator::Quote(targetNames.targetNameImport));
      m_fileContext.WriteVariable(
        "TargetNamePDB",
        cmGlobalFastbuildGenerator::Quote(targetNames.targetNamePDB));
      m_fileContext.WriteVariable(
        "TargetNameSO",
        cmGlobalFastbuildGenerator::Quote(targetNames.targetNameSO));
      m_fileContext.WriteVariable(
        "TargetNameReal",
        cmGlobalFastbuildGenerator::Quote(targetNames.targetNameReal));

      // TODO: Remove this if these variables aren't used...
      // They've been added for testing
      m_fileContext.WriteVariable(
        "TargetOutput",
        cmGlobalFastbuildGenerator::Quote(targetNames.targetOutput));
      m_fileContext.WriteVariable(
        "TargetOutputImplib",
        cmGlobalFastbuildGenerator::Quote(targetNames.targetOutputImplib));
      m_fileContext.WriteVariable(
        "TargetOutputReal",
        cmGlobalFastbuildGenerator::Quote(targetNames.targetOutputReal));
      m_fileContext.WriteVariable(
        "TargetOutDir",
        cmGlobalFastbuildGenerator::Quote(targetNames.targetOutputDir));
      m_fileContext.WriteVariable("TargetOutCompilePDBDir",
                                  cmGlobalFastbuildGenerator::Quote(
                                    targetNames.targetOutputCompilePDBDir));
      m_fileContext.WriteVariable(
        "TargetOutPDBDir",
        cmGlobalFastbuildGenerator::Quote(targetNames.targetOutputPDBDir));

      // Compile directory always needs to exist
      EnsureDirectoryExists(targetNames.targetOutputCompilePDBDir,
                            Makefile->GetHomeOutputDirectory());

      if (GeneratorTarget->GetType() != cmState::OBJECT_LIBRARY) {
        // on Windows the output dir is already needed at compile time
        // ensure the directory exists (OutDir test)
        EnsureDirectoryExists(targetNames.targetOutputDir,
                              Makefile->GetHomeOutputDirectory());
        EnsureDirectoryExists(targetNames.targetOutputPDBDir,
                              Makefile->GetHomeOutputDirectory());
      }
    }

    // Write the dependency list in here too
    // So all dependant libraries are built before this one is
    // This is incase this library depends on code generated from previous
    // ones
    {
      m_fileContext.WriteArray("PreBuildDependencies",
                               cmGlobalFastbuildGenerator::Wrap(
                                 dependencies, "'", "-" + configName + "'"));
    }

    m_fileContext.WritePopScope();
  }

  // Output the prebuild/Prelink commands
  WriteCustomBuildSteps(GeneratorTarget->GetPreBuildCommands(), "PreBuild",
                        dependencies);
  WriteCustomBuildSteps(GeneratorTarget->GetPreLinkCommands(), "PreLink",
                        dependencies);

  // Iterate over each configuration
  // This time to define prebuild and post build targets for each config
  for (std::vector<std::string>::const_iterator configIter = configs.begin();
       configIter != configs.end(); ++configIter) {
    const std::string& configName = *configIter;

    m_fileContext.WriteVariable("BaseCompilationConfig_" + configName, "");
    m_fileContext.WritePushScopeStruct();

    m_fileContext.WriteCommand("Using", ".BaseConfig_" + configName);

    // Add to the list of prebuild deps
    // The prelink and prebuild commands
    {
      std::vector<std::string> preBuildSteps;
      if (!GeneratorTarget->GetPreBuildCommands().empty()) {
        preBuildSteps.push_back("PreBuild");
      }
      if (!GeneratorTarget->GetPreLinkCommands().empty()) {
        preBuildSteps.push_back("PreLink");
      }

      if (!preBuildSteps.empty()) {
        m_fileContext.WriteArray(
          "PreBuildDependencies",
          cmGlobalFastbuildGenerator::Wrap(
            preBuildSteps, "'" + targetName + "-", "-" + configName + "'"),
          "+");
      }
    }

    m_fileContext.WritePopScope();
  }

  // Write the custom build rules
  bool hasCustomBuildRules = WriteCustomBuildRules();

  // Figure out the list of languages in use by this target
  std::vector<std::string> linkableDeps;
  std::vector<std::string> orderDeps;
  std::set<std::string> languages;
  cmGlobalFastbuildGenerator::Detail::Detection::DetectLanguages(
    languages, GeneratorTarget);

  // Write the object list definitions for each language
  // stored in this target
  for (std::set<std::string>::iterator langIter = languages.begin();
       langIter != languages.end(); ++langIter) {
    const std::string& objectGroupLanguage = *langIter;
    std::string ruleObjectGroupName = "ObjectGroup_" + objectGroupLanguage;
    linkableDeps.push_back(ruleObjectGroupName);

    m_fileContext.WriteVariable(ruleObjectGroupName, "");
    m_fileContext.WritePushScopeStruct();

    // Iterating over all configurations
    for (std::vector<std::string>::const_iterator configIter = configs.begin();
         configIter != configs.end(); ++configIter) {
      const std::string& configName = *configIter;
      m_fileContext.WriteVariable("ObjectConfig_" + configName, "");
      m_fileContext.WritePushScopeStruct();

      m_fileContext.WriteCommand("Using",
                                 ".BaseCompilationConfig_" + configName);
      m_fileContext.WriteCommand("Using", ".CustomCommands_" + configName);

      m_fileContext.WriteBlankLine();
      m_fileContext.WriteComment("Compiler options:");

      // Compiler options
      std::string baseCompileFlags;
      {
        // Remove the command from the front and leave the flags behind
        std::string compileCmd;
        DetectBaseCompileCommand(compileCmd, objectGroupLanguage);

        // No need to double unescape the variables
        // Detection::UnescapeFastbuildVariables(compileCmd);

        std::string executable;
        SplitExecutableAndFlags(compileCmd, executable, baseCompileFlags);

        m_fileContext.WriteVariable(
          "CompilerCmdBaseFlags",
          cmGlobalFastbuildGenerator::Quote(baseCompileFlags));

        std::string compilerName = ".Compiler_" + objectGroupLanguage;
        m_fileContext.WriteVariable("Compiler", compilerName);
      }

      std::map<std::string,
               cmGlobalFastbuildGenerator::Detail::Generation::CompileCommand>
        commandPermutations;

      // Source files
      m_fileContext.WriteBlankLine();
      m_fileContext.WriteComment("Source files:");
      {
        // get a list of source files
        std::vector<cmSourceFile const*> objectSources;
        GeneratorTarget->GetObjectSources(objectSources, configName);

        std::vector<cmSourceFile const*> filteredObjectSources;
        cmGlobalFastbuildGenerator::Detail::Detection::FilterSourceFiles(
          filteredObjectSources, objectSources, objectGroupLanguage);

        // Figure out the compilation commands for all
        // the translation units in the compilation.
        // Detect if one of them is a PreCompiledHeader
        // and extract it to be used in a precompiled header
        // generation step.
        for (std::vector<cmSourceFile const*>::iterator sourceIter =
               filteredObjectSources.begin();
             sourceIter != filteredObjectSources.end(); ++sourceIter) {
          cmSourceFile const* srcFile = *sourceIter;
          std::string sourceFile = srcFile->GetFullPath();

          // Detect flags and defines
          std::string compilerFlags;
          cmGlobalFastbuildGenerator::Detail::Detection::DetectCompilerFlags(
            compilerFlags, LocalGenerator, GeneratorTarget, srcFile,
            objectGroupLanguage, configName);
          std::string compileDefines =
            ComputeDefines(srcFile, configName, objectGroupLanguage);

          UnescapeFastbuildVariables(compilerFlags);
          UnescapeFastbuildVariables(compileDefines);

          std::string configKey = compilerFlags + "{|}" + compileDefines;
          cmGlobalFastbuildGenerator::Detail::Generation::CompileCommand&
            command = commandPermutations[configKey];
          command.sourceFiles[srcFile->GetLocation().GetDirectory()].push_back(
            sourceFile);
          command.flags = compilerFlags;
          command.defines = compileDefines;
        }
      }

      // Iterate over all subObjectGroups
      std::string objectGroupRuleName =
        targetName + "-" + ruleObjectGroupName + "-" + configName;
      std::vector<std::string> configObjectGroups;
      int groupNameCount = 1;
      for (std::map<std::string, cmGlobalFastbuildGenerator::Detail::
                                   Generation::CompileCommand>::iterator
             groupIter = commandPermutations.begin();
           groupIter != commandPermutations.end(); ++groupIter) {
        m_fileContext.WritePushScope();

        const cmGlobalFastbuildGenerator::Detail::Generation::CompileCommand&
          command = groupIter->second;

        m_fileContext.WriteVariable(
          "CompileDefineFlags",
          cmGlobalFastbuildGenerator::Quote(command.defines));
        m_fileContext.WriteVariable(
          "CompileFlags", cmGlobalFastbuildGenerator::Quote(command.flags));
        m_fileContext.WriteVariable(
          "CompilerOptions",
          cmGlobalFastbuildGenerator::Quote(
            "$CompileFlags$ $CompileDefineFlags$ $CompilerCmdBaseFlags$"));

        if (objectGroupLanguage == "RC") {
          m_fileContext.WriteVariable(
            "CompilerOutputExtension",
            cmGlobalFastbuildGenerator::Quote(".res"));
        } else {
          m_fileContext.WriteVariable("CompilerOutputExtension",
                                      cmGlobalFastbuildGenerator::Quote(
                                        "." + objectGroupLanguage + ".obj"));
        }

        std::map<std::string, std::vector<std::string> >::const_iterator
          objectListIt;
        for (objectListIt = command.sourceFiles.begin();
             objectListIt != command.sourceFiles.end(); ++objectListIt) {
          const std::string folderName(GetLastFolderName(objectListIt->first));
          std::stringstream ruleName;
          ruleName << objectGroupRuleName << "-" << folderName << "-"
                   << (groupNameCount++);

          m_fileContext.WriteCommand(
            "ObjectList", cmGlobalFastbuildGenerator::Quote(ruleName.str()));
          m_fileContext.WritePushScope();

          m_fileContext.WriteArray(
            "CompilerInputFiles",
            cmGlobalFastbuildGenerator::Wrap(objectListIt->second, "'", "'"));

          configObjectGroups.push_back(ruleName.str());

          std::string targetCompileOutDirectory =
            DetectTargetCompileOutputDir(configName) + "/" + folderName;
          cmSystemTools::ConvertToOutputSlashes(targetCompileOutDirectory);
          m_fileContext.WriteVariable(
            "CompilerOutputPath",
            cmGlobalFastbuildGenerator::Quote(targetCompileOutDirectory));

          // Unity source files:
          m_fileContext.WriteVariable("UnityInputFiles",
                                      ".CompilerInputFiles");

          /*
          if
          (cmGlobalFastbuildGenerator::Detail::Detection::DetectPrecompiledHeader(command.flags
          + " " +
              baseCompileFlags + " " + command.defines,
              preCompiledHeaderInput,
              preCompiledHeaderOutput,
              preCompiledHeaderOptions)
          */
          m_fileContext.WritePopScope();
        }

        m_fileContext.WritePopScope();
      }

      if (!configObjectGroups.empty()) {
        // Write an alias for this object group to group them all together
        m_fileContext.WriteCommand(
          "Alias", cmGlobalFastbuildGenerator::Quote(objectGroupRuleName));
        m_fileContext.WritePushScope();
        m_fileContext.WriteArray("Targets", cmGlobalFastbuildGenerator::Wrap(
                                              configObjectGroups, "'", "'"));
        m_fileContext.WritePopScope();
      }

      m_fileContext.WritePopScope();
    }
    m_fileContext.WritePopScope();
  }

  // Object libraries do not have linker stages
  // nor utilities
  const bool hasLinkerStage =
    GeneratorTarget->GetType() != cmState::OBJECT_LIBRARY &&
    GeneratorTarget->GetType() != cmState::UTILITY &&
    GeneratorTarget->GetType() != cmState::GLOBAL_TARGET;

  // Iterate over each configuration
  // This time to define linker settings for each config
  for (std::vector<std::string>::const_iterator iter = configs.begin();
       iter != configs.end(); ++iter) {
    const std::string& configName = *iter;

    std::string linkRuleName = targetName + "-link-" + configName;

    if (hasLinkerStage) {

      m_fileContext.WriteVariable("LinkerConfig_" + configName, "");
      m_fileContext.WritePushScopeStruct();

      m_fileContext.WriteCommand("Using", ".BaseConfig_" + configName);

      m_fileContext.WriteBlankLine();
      m_fileContext.WriteComment("Linker options:");
      // Linker options
      {
        std::string linkLibs;
        std::string targetFlags;
        std::string linkFlags;
        std::string frameworkPath;
        std::string dummyLinkPath;

        LocalGenerator->GetTargetFlags(configName, linkLibs, targetFlags,
                                       linkFlags, frameworkPath, dummyLinkPath,
                                       GeneratorTarget, false);

        std::string linkPath;
        DetectLinkerLibPaths(linkPath, configName);

        UnescapeFastbuildVariables(linkLibs);
        UnescapeFastbuildVariables(targetFlags);
        UnescapeFastbuildVariables(linkFlags);
        UnescapeFastbuildVariables(frameworkPath);
        UnescapeFastbuildVariables(linkPath);

        linkPath = frameworkPath + linkPath;

        if (GeneratorTarget->IsExecutableWithExports()) {
          const char* defFileFlag =
            LocalGenerator->GetMakefile()->GetDefinition(
              "CMAKE_LINK_DEF_FILE_FLAG");
          const cmSourceFile* defFile =
            GeneratorTarget->GetModuleDefinitionFile(configName);
          if (defFile && !defFile->GetFullPath().empty()) {
            linkFlags += defFileFlag + defFile->GetFullPath();
          }
        }

        m_fileContext.WriteVariable("LinkPath", "'" + linkPath + "'");
        m_fileContext.WriteVariable("LinkLibs", "'" + linkLibs + "'");
        m_fileContext.WriteVariable("LinkFlags", "'" + linkFlags + "'");
        m_fileContext.WriteVariable("TargetFlags", "'" + targetFlags + "'");

        // Remove the command from the front and leave the flags behind
        std::string linkCmd;
        if (!DetectBaseLinkerCommand(linkCmd, configName)) {
          return;
        }
        // No need to do this, because the function above has already escaped
        // things appropriately
        // UnescapeFastbuildVariables(linkCmd);

        std::string executable;
        std::string flags;
        SplitExecutableAndFlags(linkCmd, executable, flags);

        std::string language = GeneratorTarget->GetLinkerLanguage(configName);
        std::string linkerType =
          LocalGenerator->GetMakefile()->GetSafeDefinition(
            "CMAKE_" + language + "_COMPILER_ID");

        m_fileContext.WriteVariable(
          "Linker", cmGlobalFastbuildGenerator::Quote(executable));
        m_fileContext.WriteVariable(
          "LinkerType", cmGlobalFastbuildGenerator::Quote(linkerType));
        m_fileContext.WriteVariable("BaseLinkerOptions",
                                    cmGlobalFastbuildGenerator::Quote(flags));

        m_fileContext.WriteVariable("LinkerOutput", "'$TargetOutput$'");
        m_fileContext.WriteVariable("LinkerOptions",
                                    "'$BaseLinkerOptions$ $LinkLibs$'");

        m_fileContext.WriteArray(
          "Libraries",
          cmGlobalFastbuildGenerator::Wrap(
            linkableDeps, "'" + targetName + "-", "-" + configName + "'"));

        // Now detect the extra dependencies for linking
        {
          std::vector<std::string> extraDependencies;
          DetectTargetObjectDependencies(configName, extraDependencies);
          DetectTargetLinkDependencies(configName, extraDependencies);

          std::for_each(extraDependencies.begin(), extraDependencies.end(),
                        UnescapeFastbuildVariables);

          m_fileContext.WriteArray(
            "Libraries",
            cmGlobalFastbuildGenerator::Wrap(extraDependencies, "'", "'"),
            "+");
        }

        m_fileContext.WriteCommand(
          linkCommand, cmGlobalFastbuildGenerator::Quote(linkRuleName));
        m_fileContext.WritePushScope();
        if (linkCommand == "Library") {
          m_fileContext.WriteComment(
            "Convert the linker options to work with libraries");

          // Push dummy definitions for compilation variables
          // These variables are required by the Library command
          m_fileContext.WriteVariable("Compiler", ".Compiler_dummy");
          m_fileContext.WriteVariable(
            "CompilerOptions",
            "'-c $FB_INPUT_1_PLACEHOLDER$ $FB_INPUT_2_PLACEHOLDER$'");
          m_fileContext.WriteVariable("CompilerOutputPath", "'/dummy/'");

          // These variables are required by the Library command as well
          // we just need to transfer the values in the linker variables
          // to these locations
          m_fileContext.WriteVariable("Librarian", "'$Linker$'");
          m_fileContext.WriteVariable("LibrarianOptions", "'$LinkerOptions$'");
          m_fileContext.WriteVariable("LibrarianOutput", "'$LinkerOutput$'");

          m_fileContext.WriteVariable("LibrarianAdditionalInputs",
                                      ".Libraries");
        }
        m_fileContext.WritePopScope();
      }
      m_fileContext.WritePopScope();
    }
  }

  if (!GeneratorTarget->GetPreBuildCommands().empty()) {
    orderDeps.push_back("PreBuild");
  }
  if (!GeneratorTarget->GetPreLinkCommands().empty()) {
    orderDeps.push_back("PreLink");
  }
  if (hasCustomBuildRules) {
    orderDeps.push_back("CustomCommands");
  }
  if (hasLinkerStage) {
    linkableDeps.push_back("link");
    orderDeps.push_back("link");
  }

  // Output the postbuild commands
  WriteCustomBuildSteps(
    GeneratorTarget->GetPostBuildCommands(), "PostBuild",
    cmGlobalFastbuildGenerator::Wrap(orderDeps, targetName + "-", ""));

  // Always add the pre/post build steps as
  // part of the alias.
  // This way, if there are ONLY build steps, then
  // things should still work too.
  if (!GeneratorTarget->GetPostBuildCommands().empty()) {
    orderDeps.push_back("PostBuild");
  }

  // Output a list of aliases
  this->WriteTargetAliases(linkableDeps, orderDeps);

  m_fileContext.WritePopScope();
}
