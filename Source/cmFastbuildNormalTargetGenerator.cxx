#include "cmFastbuildNormalTargetGenerator.h"

#include "cmMakefile.h"
#include "cmSourceFile.h"

cmFastbuildNormalTargetGenerator::cmFastbuildNormalTargetGenerator(
  cmGeneratorTarget* gt,
  cmGlobalFastbuildGenerator::Detail::Generation::GenerationContext context)
  : cmFastbuildTargetGenerator(gt)
  , m_context(context)
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

  cmGlobalFastbuildGenerator::Detail::FileContext& fileContext =
    ((cmGlobalFastbuildGenerator*)GlobalGenerator)->g_fc;

  fileContext.WriteComment("Target definition: " + targetName);
  fileContext.WritePushScope();

  std::vector<std::string> dependencies;
  DetectTargetCompileDependencies(GlobalGenerator, dependencies);

  // Iterate over each configuration
  // This time to define linker settings for each config
  std::vector<std::string> configs;
  Makefile->GetConfigurations(configs, false);
  for (std::vector<std::string>::const_iterator configIter = configs.begin();
       configIter != configs.end(); ++configIter) {
    const std::string& configName = *configIter;

    fileContext.WriteVariable("BaseConfig_" + configName, "");
    fileContext.WritePushScopeStruct();

    fileContext.WriteCommand("Using", ".ConfigBase");

    fileContext.WriteVariable(
      "ConfigName",
      cmGlobalFastbuildGenerator::Detail::Generation::Quote(configName));

    fileContext.WriteBlankLine();
    fileContext.WriteComment("General output details:");
    // Write out the output paths for the outcome of this target
    {
      cmGlobalFastbuildGenerator::Detail::Detection::FastbuildTargetNames
        targetNames;
      cmGlobalFastbuildGenerator::Detail::Detection::DetectOutput(
        LocalGenerator, targetNames, GeneratorTarget, configName);

      fileContext.WriteVariable(
        "TargetNameOut", cmGlobalFastbuildGenerator::Detail::Generation::Quote(
                           targetNames.targetNameOut));
      fileContext.WriteVariable(
        "TargetNameImport",
        cmGlobalFastbuildGenerator::Detail::Generation::Quote(
          targetNames.targetNameImport));
      fileContext.WriteVariable(
        "TargetNamePDB", cmGlobalFastbuildGenerator::Detail::Generation::Quote(
                           targetNames.targetNamePDB));
      fileContext.WriteVariable(
        "TargetNameSO", cmGlobalFastbuildGenerator::Detail::Generation::Quote(
                          targetNames.targetNameSO));
      fileContext.WriteVariable(
        "TargetNameReal",
        cmGlobalFastbuildGenerator::Detail::Generation::Quote(
          targetNames.targetNameReal));

      // TODO: Remove this if these variables aren't used...
      // They've been added for testing
      fileContext.WriteVariable(
        "TargetOutput", cmGlobalFastbuildGenerator::Detail::Generation::Quote(
                          targetNames.targetOutput));
      fileContext.WriteVariable(
        "TargetOutputImplib",
        cmGlobalFastbuildGenerator::Detail::Generation::Quote(
          targetNames.targetOutputImplib));
      fileContext.WriteVariable(
        "TargetOutputReal",
        cmGlobalFastbuildGenerator::Detail::Generation::Quote(
          targetNames.targetOutputReal));
      fileContext.WriteVariable(
        "TargetOutDir", cmGlobalFastbuildGenerator::Detail::Generation::Quote(
                          targetNames.targetOutputDir));
      fileContext.WriteVariable(
        "TargetOutCompilePDBDir",
        cmGlobalFastbuildGenerator::Detail::Generation::Quote(
          targetNames.targetOutputCompilePDBDir));
      fileContext.WriteVariable(
        "TargetOutPDBDir",
        cmGlobalFastbuildGenerator::Detail::Generation::Quote(
          targetNames.targetOutputPDBDir));

      // Compile directory always needs to exist
      cmGlobalFastbuildGenerator::Detail::Generation::EnsureDirectoryExists(
        targetNames.targetOutputCompilePDBDir,
        Makefile->GetHomeOutputDirectory());

      if (GeneratorTarget->GetType() != cmState::OBJECT_LIBRARY) {
        // on Windows the output dir is already needed at compile time
        // ensure the directory exists (OutDir test)
        cmGlobalFastbuildGenerator::Detail::Generation::EnsureDirectoryExists(
          targetNames.targetOutputDir, Makefile->GetHomeOutputDirectory());
        cmGlobalFastbuildGenerator::Detail::Generation::EnsureDirectoryExists(
          targetNames.targetOutputPDBDir, Makefile->GetHomeOutputDirectory());
      }
    }

    // Write the dependency list in here too
    // So all dependant libraries are built before this one is
    // This is incase this library depends on code generated from previous
    // ones
    {
      fileContext.WriteArray(
        "PreBuildDependencies",
        cmGlobalFastbuildGenerator::Detail::Generation::Wrap(
          dependencies, "'", "-" + configName + "'"));
    }

    fileContext.WritePopScope();
  }

  // Output the prebuild/Prelink commands
  cmGlobalFastbuildGenerator::Detail::Generation::WriteCustomBuildSteps(
    m_context, LocalGenerator, GeneratorTarget,
    GeneratorTarget->GetPreBuildCommands(), "PreBuild", dependencies);
  cmGlobalFastbuildGenerator::Detail::Generation::WriteCustomBuildSteps(
    m_context, LocalGenerator, GeneratorTarget,
    GeneratorTarget->GetPreLinkCommands(), "PreLink", dependencies);

  // Iterate over each configuration
  // This time to define prebuild and post build targets for each config
  for (std::vector<std::string>::const_iterator configIter = configs.begin();
       configIter != configs.end(); ++configIter) {
    const std::string& configName = *configIter;

    fileContext.WriteVariable("BaseCompilationConfig_" + configName, "");
    fileContext.WritePushScopeStruct();

    fileContext.WriteCommand("Using", ".BaseConfig_" + configName);

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
        fileContext.WriteArray(
          "PreBuildDependencies",
          cmGlobalFastbuildGenerator::Detail::Generation::Wrap(
            preBuildSteps, "'" + targetName + "-", "-" + configName + "'"),
          "+");
      }
    }

    fileContext.WritePopScope();
  }

  // Write the custom build rules
  bool hasCustomBuildRules =
    cmGlobalFastbuildGenerator::Detail::Generation::WriteCustomBuildRules(
      m_context, LocalGenerator, GeneratorTarget);

  // Figure out the list of languages in use by this target
  std::vector<std::string> linkableDeps;
  std::vector<std::string> orderDeps;
  std::set<std::string> languages;
  cmGlobalFastbuildGenerator::Detail::Detection::DetectLanguages(
    languages, GlobalGenerator, GeneratorTarget);

  // Write the object list definitions for each language
  // stored in this target
  for (std::set<std::string>::iterator langIter = languages.begin();
       langIter != languages.end(); ++langIter) {
    const std::string& objectGroupLanguage = *langIter;
    std::string ruleObjectGroupName = "ObjectGroup_" + objectGroupLanguage;
    linkableDeps.push_back(ruleObjectGroupName);

    fileContext.WriteVariable(ruleObjectGroupName, "");
    fileContext.WritePushScopeStruct();

    // Iterating over all configurations
    for (std::vector<std::string>::const_iterator configIter = configs.begin();
         configIter != configs.end(); ++configIter) {
      const std::string& configName = *configIter;
      fileContext.WriteVariable("ObjectConfig_" + configName, "");
      fileContext.WritePushScopeStruct();

      fileContext.WriteCommand("Using",
                               ".BaseCompilationConfig_" + configName);
      fileContext.WriteCommand("Using", ".CustomCommands_" + configName);

      fileContext.WriteBlankLine();
      fileContext.WriteComment("Compiler options:");

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

        fileContext.WriteVariable(
          "CompilerCmdBaseFlags",
          cmGlobalFastbuildGenerator::Detail::Generation::Quote(
            baseCompileFlags));

        std::string compilerName = ".Compiler_" + objectGroupLanguage;
        fileContext.WriteVariable("Compiler", compilerName);
      }

      std::map<std::string,
               cmGlobalFastbuildGenerator::Detail::Generation::CompileCommand>
        commandPermutations;

      // Source files
      fileContext.WriteBlankLine();
      fileContext.WriteComment("Source files:");
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
        fileContext.WritePushScope();

        const cmGlobalFastbuildGenerator::Detail::Generation::CompileCommand&
          command = groupIter->second;

        fileContext.WriteVariable(
          "CompileDefineFlags",
          cmGlobalFastbuildGenerator::Detail::Generation::Quote(
            command.defines));
        fileContext.WriteVariable(
          "CompileFlags",
          cmGlobalFastbuildGenerator::Detail::Generation::Quote(
            command.flags));
        fileContext.WriteVariable(
          "CompilerOptions",
          cmGlobalFastbuildGenerator::Detail::Generation::Quote(
            "$CompileFlags$ $CompileDefineFlags$ $CompilerCmdBaseFlags$"));

        if (objectGroupLanguage == "RC") {
          fileContext.WriteVariable(
            "CompilerOutputExtension",
            cmGlobalFastbuildGenerator::Detail::Generation::Quote(".res"));
        } else {
          fileContext.WriteVariable(
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

          fileContext.WriteCommand(
            "ObjectList",
            cmGlobalFastbuildGenerator::Detail::Generation::Quote(
              ruleName.str()));
          fileContext.WritePushScope();

          fileContext.WriteArray(
            "CompilerInputFiles",
            cmGlobalFastbuildGenerator::Detail::Generation::Wrap(
              objectListIt->second, "'", "'"));

          configObjectGroups.push_back(ruleName.str());

          std::string targetCompileOutDirectory = cmGlobalFastbuildGenerator::
            Detail::Detection::DetectTargetCompileOutputDir(
              LocalGenerator, GeneratorTarget, configName);
          fileContext.WriteVariable(
            "CompilerOutputPath",
            cmGlobalFastbuildGenerator::Detail::Generation::Quote(
              targetCompileOutDirectory + "/" + folderName));

          // Unity source files:
          fileContext.WriteVariable("UnityInputFiles", ".CompilerInputFiles");

          /*
          if
          (cmGlobalFastbuildGenerator::Detail::Detection::DetectPrecompiledHeader(command.flags
          + " " +
              baseCompileFlags + " " + command.defines,
              preCompiledHeaderInput,
              preCompiledHeaderOutput,
              preCompiledHeaderOptions)
          */
          fileContext.WritePopScope();
        }

        fileContext.WritePopScope();
      }

      if (!configObjectGroups.empty()) {
        // Write an alias for this object group to group them all together
        fileContext.WriteCommand(
          "Alias", cmGlobalFastbuildGenerator::Detail::Generation::Quote(
                     objectGroupRuleName));
        fileContext.WritePushScope();
        fileContext.WriteArray(
          "Targets", cmGlobalFastbuildGenerator::Detail::Generation::Wrap(
                       configObjectGroups, "'", "'"));
        fileContext.WritePopScope();
      }

      fileContext.WritePopScope();
    }
    fileContext.WritePopScope();
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

      fileContext.WriteVariable("LinkerConfig_" + configName, "");
      fileContext.WritePushScopeStruct();

      fileContext.WriteCommand("Using", ".BaseConfig_" + configName);

      fileContext.WriteBlankLine();
      fileContext.WriteComment("Linker options:");
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

        fileContext.WriteVariable("LinkPath", "'" + linkPath + "'");
        fileContext.WriteVariable("LinkLibs", "'" + linkLibs + "'");
        fileContext.WriteVariable("LinkFlags", "'" + linkFlags + "'");
        fileContext.WriteVariable("TargetFlags", "'" + targetFlags + "'");

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

        fileContext.WriteVariable(
          "Linker",
          cmGlobalFastbuildGenerator::Detail::Generation::Quote(executable));
        fileContext.WriteVariable(
          "LinkerType",
          cmGlobalFastbuildGenerator::Detail::Generation::Quote(linkerType));
        fileContext.WriteVariable(
          "BaseLinkerOptions",
          cmGlobalFastbuildGenerator::Detail::Generation::Quote(flags));

        fileContext.WriteVariable("LinkerOutput", "'$TargetOutput$'");
        fileContext.WriteVariable("LinkerOptions",
                                  "'$BaseLinkerOptions$ $LinkLibs$'");

        fileContext.WriteArray(
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

          fileContext.WriteArray(
            "Libraries", cmGlobalFastbuildGenerator::Detail::Generation::Wrap(
                           extraDependencies, "'", "'"),
            "+");
        }

        fileContext.WriteCommand(
          linkCommand,
          cmGlobalFastbuildGenerator::Detail::Generation::Quote(linkRuleName));
        fileContext.WritePushScope();
        if (linkCommand == "Library") {
          fileContext.WriteComment(
            "Convert the linker options to work with libraries");

          // Push dummy definitions for compilation variables
          // These variables are required by the Library command
          fileContext.WriteVariable("Compiler", ".Compiler_dummy");
          fileContext.WriteVariable(
            "CompilerOptions",
            "'-c $FB_INPUT_1_PLACEHOLDER$ $FB_INPUT_2_PLACEHOLDER$'");
          fileContext.WriteVariable("CompilerOutputPath", "'/dummy/'");

          // These variables are required by the Library command as well
          // we just need to transfer the values in the linker variables
          // to these locations
          fileContext.WriteVariable("Librarian", "'$Linker$'");
          fileContext.WriteVariable("LibrarianOptions", "'$LinkerOptions$'");
          fileContext.WriteVariable("LibrarianOutput", "'$LinkerOutput$'");

          fileContext.WriteVariable("LibrarianAdditionalInputs", ".Libraries");
        }
        fileContext.WritePopScope();
      }
      fileContext.WritePopScope();
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
  cmGlobalFastbuildGenerator::Detail::Generation::WriteCustomBuildSteps(
    m_context, LocalGenerator, GeneratorTarget,
    GeneratorTarget->GetPostBuildCommands(), "PostBuild",
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
  cmGlobalFastbuildGenerator::Detail::Generation::WriteTargetAliases(
    m_context, GeneratorTarget, linkableDeps, orderDeps);

  fileContext.WritePopScope();
}
