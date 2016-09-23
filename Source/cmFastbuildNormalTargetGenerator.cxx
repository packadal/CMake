#include "cmFastbuildNormalTargetGenerator.h"

#include "cmCustomCommandGenerator.h"
#include "cmMakefile.h"
#include "cmSourceFile.h"

cmFastbuildNormalTargetGenerator::cmFastbuildNormalTargetGenerator(
  cmGeneratorTarget* gt,
  cmGlobalFastbuildGenerator::Detail::Generation::CustomCommandAliasMap
    customCommandAliases)
  : cmFastbuildTargetGenerator(gt)
  , m_customCommandAliases(customCommandAliases)
  , m_fileContext(((cmGlobalFastbuildGenerator*)GlobalGenerator)->g_fc)
{
}

void cmFastbuildNormalTargetGenerator::DetectTargetCompileDependencies(
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

void cmFastbuildNormalTargetGenerator::WriteTargetAliases(
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
        "Alias", cmGlobalFastbuildGenerator::Detail::Generation::Quote(
                   targetName + "-" + configName + "-products"));
      m_fileContext.WritePushScope();
      m_fileContext.WriteArray(
        "Targets",
        cmGlobalFastbuildGenerator::Detail::Generation::Wrap(
          linkableDeps, "'" + targetName + "-", "-" + configName + "'"));
      m_fileContext.WritePopScope();
    }

    if (!orderDeps.empty() || !linkableDeps.empty()) {
      m_fileContext.WriteCommand(
        "Alias", cmGlobalFastbuildGenerator::Detail::Generation::Quote(
                   targetName + "-" + configName));
      m_fileContext.WritePushScope();
      m_fileContext.WriteArray(
        "Targets",
        cmGlobalFastbuildGenerator::Detail::Generation::Wrap(
          linkableDeps, "'" + targetName + "-", "-" + configName + "'"));
      m_fileContext.WriteArray(
        "Targets",
        cmGlobalFastbuildGenerator::Detail::Generation::Wrap(
          orderDeps, "'" + targetName + "-", "-" + configName + "'"),
        "+");
      m_fileContext.WritePopScope();
    }
  }
}

void cmFastbuildNormalTargetGenerator::WriteCustomBuildSteps(
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
      cmGlobalFastbuildGenerator::Detail::Generation::Wrap(
        orderDeps, "'", "-" + configName + "'"),
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
    m_fileContext.WriteCommand(
      "Alias",
      cmGlobalFastbuildGenerator::Detail::Generation::Quote(baseName));
    m_fileContext.WritePushScope();
    m_fileContext.WriteArray(
      "Targets", cmGlobalFastbuildGenerator::Detail::Generation::Wrap(
                   customCommandTargets, "'", "'"));
    m_fileContext.WritePopScope();

    m_fileContext.WritePopScope();
  }
}

bool cmFastbuildNormalTargetGenerator::WriteCustomBuildRules()
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
        "Alias", cmGlobalFastbuildGenerator::Detail::Generation::Quote(
                   customCommandGroupName));
      m_fileContext.WritePushScope();
      m_fileContext.WriteArray(
        "Targets", cmGlobalFastbuildGenerator::Detail::Generation::Wrap(
                     customCommandTargets, "'", "'"));
      m_fileContext.WritePopScope();

      // Now make everything use this as prebuilt dependencies
      std::vector<std::string> tmp;
      tmp.push_back(customCommandGroupName);
      m_fileContext.WriteArray(
        "PreBuildDependencies",
        cmGlobalFastbuildGenerator::Detail::Generation::Wrap(tmp), "+");

      hasCustomCommands = true;
    }

    m_fileContext.WritePopScope();
  }

  return hasCustomCommands;
}

void cmFastbuildNormalTargetGenerator::WriteCustomCommand(
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
      iterator findResult = m_customCommandAliases.find(cc);
    if (findResult != m_customCommandAliases.end()) {
      const std::set<std::string>& aliases = findResult->second;
      if (aliases.find(targetName) != aliases.end()) {
        // This target has already been generated
        // with the correct name somewhere else.
        return;
      }
      if (!cmGlobalFastbuildGenerator::Detail::Detection::isConfigDependant(
            &ccg)) {
        // This command has already been generated.
        // But under a different name so setup an alias to redirect
        // No merged outputs, so this command must always be run.
        // Make it's name unique to its host target
        targetName += "-";
        targetName += hostTargetName;

        std::vector<std::string> targets;
        targets.push_back(*findResult->second.begin());

        m_fileContext.WriteCommand(
          "Alias",
          cmGlobalFastbuildGenerator::Detail::Generation::Quote(targetName));
        m_fileContext.WritePushScope();
        {
          m_fileContext.WriteArray(
            "Targets",
            cmGlobalFastbuildGenerator::Detail::Generation::Wrap(targets));
        }
        m_fileContext.WritePopScope();
        return;
      }
    }
    m_customCommandAliases[cc].insert(targetName);
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
    cmSystemTools::ReplaceString(
      args, cmGlobalFastbuildGenerator::FASTBUILD_DOLLAR_TAG, "$");
#ifdef _WIN32
    // in windows batch, '%' is a special character that needs to be doubled
    // to be escaped
    cmSystemTools::ReplaceString(args, "%", "%%");
#endif
    cmGlobalFastbuildGenerator::Detail::Detection::ResolveFastbuildVariables(
      args, configName);

    std::string command(ccg.GetCommand(i));
    cmSystemTools::ReplaceString(
      command, cmGlobalFastbuildGenerator::FASTBUILD_DOLLAR_TAG, "$");
    cmGlobalFastbuildGenerator::Detail::Detection::ResolveFastbuildVariables(
      command, configName);

    scriptFile << cmGlobalFastbuildGenerator::Detail::Generation::Quote(
                    command, "\"")
               << args << std::endl;
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

  std::for_each(inputs.begin(), inputs.end(),
                &cmGlobalFastbuildGenerator::Detail::Detection::
                  UnescapeFastbuildVariables);
  std::for_each(mergedOutputs.begin(), mergedOutputs.end(),
                &cmGlobalFastbuildGenerator::Detail::Detection::
                  UnescapeFastbuildVariables);

  m_fileContext.WriteCommand(
    "Exec", cmGlobalFastbuildGenerator::Detail::Generation::Quote(targetName));
  m_fileContext.WritePushScope();
  {
#ifdef _WIN32
    m_fileContext.WriteVariable(
      "ExecExecutable", cmGlobalFastbuildGenerator::Detail::Generation::Quote(
                          cmSystemTools::FindProgram("cmd.exe")));
    m_fileContext.WriteVariable(
      "ExecArguments", cmGlobalFastbuildGenerator::Detail::Generation::Quote(
                         "/C " + scriptFileName));
#else
    context.fc.WriteVariable("ExecExecutable", Quote(scriptFileName));
#endif
    if (!workingDirectory.empty()) {
      m_fileContext.WriteVariable(
        "ExecWorkingDir",
        cmGlobalFastbuildGenerator::Detail::Generation::Quote(
          workingDirectory));
    }

    if (inputs.empty()) {
      // inputs.push_back("dummy-in");
    }
    m_fileContext.WriteArray(
      "ExecInput",
      cmGlobalFastbuildGenerator::Detail::Generation::Wrap(inputs));

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
      "ExecOutput",
      cmGlobalFastbuildGenerator::Detail::Generation::Quote(mergedOutputs[0]));
  }
  m_fileContext.WritePopScope();
}

void cmFastbuildNormalTargetGenerator::EnsureDirectoryExists(
  const std::string& path, const char* homeOutputDirectory)
{
  if (cmSystemTools::FileIsFullPath(path.c_str())) {
    cmSystemTools::MakeDirectory(path.c_str());
  } else {
    const std::string fullPath = std::string(homeOutputDirectory) + "/" + path;
    cmSystemTools::MakeDirectory(fullPath.c_str());
  }
}

void cmFastbuildNormalTargetGenerator::Generate()
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

    m_fileContext.WriteVariable(
      "ConfigName",
      cmGlobalFastbuildGenerator::Detail::Generation::Quote(configName));

    m_fileContext.WriteBlankLine();
    m_fileContext.WriteComment("General output details:");
    // Write out the output paths for the outcome of this target
    {
      cmGlobalFastbuildGenerator::Detail::Detection::FastbuildTargetNames
        targetNames;
      cmGlobalFastbuildGenerator::Detail::Detection::DetectOutput(
        LocalGenerator, targetNames, GeneratorTarget, configName);

      m_fileContext.WriteVariable(
        "TargetNameOut", cmGlobalFastbuildGenerator::Detail::Generation::Quote(
                           targetNames.targetNameOut));
      m_fileContext.WriteVariable(
        "TargetNameImport",
        cmGlobalFastbuildGenerator::Detail::Generation::Quote(
          targetNames.targetNameImport));
      m_fileContext.WriteVariable(
        "TargetNamePDB", cmGlobalFastbuildGenerator::Detail::Generation::Quote(
                           targetNames.targetNamePDB));
      m_fileContext.WriteVariable(
        "TargetNameSO", cmGlobalFastbuildGenerator::Detail::Generation::Quote(
                          targetNames.targetNameSO));
      m_fileContext.WriteVariable(
        "TargetNameReal",
        cmGlobalFastbuildGenerator::Detail::Generation::Quote(
          targetNames.targetNameReal));

      // TODO: Remove this if these variables aren't used...
      // They've been added for testing
      m_fileContext.WriteVariable(
        "TargetOutput", cmGlobalFastbuildGenerator::Detail::Generation::Quote(
                          targetNames.targetOutput));
      m_fileContext.WriteVariable(
        "TargetOutputImplib",
        cmGlobalFastbuildGenerator::Detail::Generation::Quote(
          targetNames.targetOutputImplib));
      m_fileContext.WriteVariable(
        "TargetOutputReal",
        cmGlobalFastbuildGenerator::Detail::Generation::Quote(
          targetNames.targetOutputReal));
      m_fileContext.WriteVariable(
        "TargetOutDir", cmGlobalFastbuildGenerator::Detail::Generation::Quote(
                          targetNames.targetOutputDir));
      m_fileContext.WriteVariable(
        "TargetOutCompilePDBDir",
        cmGlobalFastbuildGenerator::Detail::Generation::Quote(
          targetNames.targetOutputCompilePDBDir));
      m_fileContext.WriteVariable(
        "TargetOutPDBDir",
        cmGlobalFastbuildGenerator::Detail::Generation::Quote(
          targetNames.targetOutputPDBDir));

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
      m_fileContext.WriteArray(
        "PreBuildDependencies",
        cmGlobalFastbuildGenerator::Detail::Generation::Wrap(
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
          cmGlobalFastbuildGenerator::Detail::Generation::Wrap(
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
        cmGlobalFastbuildGenerator::Detail::Detection::
          DetectBaseCompileCommand(compileCmd,
                                   (cmLocalFastbuildGenerator*)LocalGenerator,
                                   GeneratorTarget, objectGroupLanguage);

        // No need to double unescape the variables
        // Detection::UnescapeFastbuildVariables(compileCmd);

        std::string executable;
        cmGlobalFastbuildGenerator::Detail::Detection::SplitExecutableAndFlags(
          compileCmd, executable, baseCompileFlags);

        m_fileContext.WriteVariable(
          "CompilerCmdBaseFlags",
          cmGlobalFastbuildGenerator::Detail::Generation::Quote(
            baseCompileFlags));

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
            cmGlobalFastbuildGenerator::Detail::Detection::ComputeDefines(
              LocalGenerator, GeneratorTarget, srcFile, configName,
              objectGroupLanguage);

          cmGlobalFastbuildGenerator::Detail::Detection::
            UnescapeFastbuildVariables(compilerFlags);
          cmGlobalFastbuildGenerator::Detail::Detection::
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
          cmGlobalFastbuildGenerator::Detail::Generation::Quote(
            command.defines));
        m_fileContext.WriteVariable(
          "CompileFlags",
          cmGlobalFastbuildGenerator::Detail::Generation::Quote(
            command.flags));
        m_fileContext.WriteVariable(
          "CompilerOptions",
          cmGlobalFastbuildGenerator::Detail::Generation::Quote(
            "$CompileFlags$ $CompileDefineFlags$ $CompilerCmdBaseFlags$"));

        if (objectGroupLanguage == "RC") {
          m_fileContext.WriteVariable(
            "CompilerOutputExtension",
            cmGlobalFastbuildGenerator::Detail::Generation::Quote(".res"));
        } else {
          m_fileContext.WriteVariable(
            "CompilerOutputExtension",
            cmGlobalFastbuildGenerator::Detail::Generation::Quote(
              "." + objectGroupLanguage + ".obj"));
        }

        std::map<std::string, std::vector<std::string> >::const_iterator
          objectListIt;
        for (objectListIt = command.sourceFiles.begin();
             objectListIt != command.sourceFiles.end(); ++objectListIt) {
          const std::string folderName(
            cmGlobalFastbuildGenerator::Detail::Detection::GetLastFolderName(
              objectListIt->first));
          std::stringstream ruleName;
          ruleName << objectGroupRuleName << "-" << folderName << "-"
                   << (groupNameCount++);

          m_fileContext.WriteCommand(
            "ObjectList",
            cmGlobalFastbuildGenerator::Detail::Generation::Quote(
              ruleName.str()));
          m_fileContext.WritePushScope();

          m_fileContext.WriteArray(
            "CompilerInputFiles",
            cmGlobalFastbuildGenerator::Detail::Generation::Wrap(
              objectListIt->second, "'", "'"));

          configObjectGroups.push_back(ruleName.str());

          std::string targetCompileOutDirectory = cmGlobalFastbuildGenerator::
            Detail::Detection::DetectTargetCompileOutputDir(
              LocalGenerator, GeneratorTarget, configName);
          m_fileContext.WriteVariable(
            "CompilerOutputPath",
            cmGlobalFastbuildGenerator::Detail::Generation::Quote(
              targetCompileOutDirectory + "/" + folderName));

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
          "Alias", cmGlobalFastbuildGenerator::Detail::Generation::Quote(
                     objectGroupRuleName));
        m_fileContext.WritePushScope();
        m_fileContext.WriteArray(
          "Targets", cmGlobalFastbuildGenerator::Detail::Generation::Wrap(
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

        LocalGenerator->GetTargetFlags(linkLibs, targetFlags, linkFlags,
                                       frameworkPath, dummyLinkPath,
                                       GeneratorTarget, false);

        std::string linkPath;
        cmGlobalFastbuildGenerator::Detail::Detection::DetectLinkerLibPaths(
          linkPath, LocalGenerator, GeneratorTarget, configName);

        cmGlobalFastbuildGenerator::Detail::Detection::
          UnescapeFastbuildVariables(linkLibs);
        cmGlobalFastbuildGenerator::Detail::Detection::
          UnescapeFastbuildVariables(targetFlags);
        cmGlobalFastbuildGenerator::Detail::Detection::
          UnescapeFastbuildVariables(linkFlags);
        cmGlobalFastbuildGenerator::Detail::Detection::
          UnescapeFastbuildVariables(frameworkPath);
        cmGlobalFastbuildGenerator::Detail::Detection::
          UnescapeFastbuildVariables(linkPath);

        linkPath = frameworkPath + linkPath;

        if (GeneratorTarget->IsExecutableWithExports()) {
          const char* defFileFlag =
            LocalGenerator->GetMakefile()->GetDefinition(
              "CMAKE_LINK_DEF_FILE_FLAG");
          const cmSourceFile* defFile =
            GeneratorTarget->GetModuleDefinitionFile(configName);
          if (!defFile->GetFullPath().empty()) {
            linkFlags += defFileFlag + defFile->GetFullPath();
          }
        }

        m_fileContext.WriteVariable("LinkPath", "'" + linkPath + "'");
        m_fileContext.WriteVariable("LinkLibs", "'" + linkLibs + "'");
        m_fileContext.WriteVariable("LinkFlags", "'" + linkFlags + "'");
        m_fileContext.WriteVariable("TargetFlags", "'" + targetFlags + "'");

        // Remove the command from the front and leave the flags behind
        std::string linkCmd;
        if (!cmGlobalFastbuildGenerator::Detail::Detection::
              DetectBaseLinkerCommand(
                linkCmd, (cmLocalFastbuildGenerator*)LocalGenerator,
                GeneratorTarget, configName)) {
          return;
        }
        // No need to do this, because the function above has already escaped
        // things appropriately
        // cmGlobalFastbuildGenerator::Detail::Detection::UnescapeFastbuildVariables(linkCmd);

        std::string executable;
        std::string flags;
        cmGlobalFastbuildGenerator::Detail::Detection::SplitExecutableAndFlags(
          linkCmd, executable, flags);

        std::string language = GeneratorTarget->GetLinkerLanguage(configName);
        std::string linkerType =
          LocalGenerator->GetMakefile()->GetSafeDefinition(
            "CMAKE_" + language + "_COMPILER_ID");

        m_fileContext.WriteVariable(
          "Linker",
          cmGlobalFastbuildGenerator::Detail::Generation::Quote(executable));
        m_fileContext.WriteVariable(
          "LinkerType",
          cmGlobalFastbuildGenerator::Detail::Generation::Quote(linkerType));
        m_fileContext.WriteVariable(
          "BaseLinkerOptions",
          cmGlobalFastbuildGenerator::Detail::Generation::Quote(flags));

        m_fileContext.WriteVariable("LinkerOutput", "'$TargetOutput$'");
        m_fileContext.WriteVariable("LinkerOptions",
                                    "'$BaseLinkerOptions$ $LinkLibs$'");

        m_fileContext.WriteArray(
          "Libraries",
          cmGlobalFastbuildGenerator::Detail::Generation::Wrap(
            linkableDeps, "'" + targetName + "-", "-" + configName + "'"));

        // Now detect the extra dependencies for linking
        {
          std::vector<std::string> extraDependencies;
          cmGlobalFastbuildGenerator::Detail::Detection::
            DetectTargetObjectDependencies(GlobalGenerator, GeneratorTarget,
                                           configName, extraDependencies);
          cmGlobalFastbuildGenerator::Detail::Detection::
            DetectTargetLinkDependencies(GeneratorTarget, configName,
                                         extraDependencies);

          std::for_each(extraDependencies.begin(), extraDependencies.end(),
                        cmGlobalFastbuildGenerator::Detail::Detection::
                          UnescapeFastbuildVariables);

          m_fileContext.WriteArray(
            "Libraries", cmGlobalFastbuildGenerator::Detail::Generation::Wrap(
                           extraDependencies, "'", "'"),
            "+");
        }

        m_fileContext.WriteCommand(
          linkCommand,
          cmGlobalFastbuildGenerator::Detail::Generation::Quote(linkRuleName));
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
  WriteCustomBuildSteps(GeneratorTarget->GetPostBuildCommands(), "PostBuild",
                        cmGlobalFastbuildGenerator::Detail::Generation::Wrap(
                          orderDeps, targetName + "-", ""));

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
