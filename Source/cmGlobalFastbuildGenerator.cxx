
/*============================================================================
    CMake - Cross Platform Makefile Generator
    Copyright 2000-2009 Kitware, Inc., Insight Software Consortium

    Distributed under the OSI-approved BSD License (the "License");
    see accompanying file Copyright.txt for details.

    This software is distributed WITHOUT ANY WARRANTY; without even the
    implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
    See the License for more information.
============================================================================*/
/*============================================================================
    Development progress:

    Tasks/Issues:
     - Execute unit tests against the generator somehow
     - Fix target aliases being repeated in the output
     - Fix cmake build using fastbuild (currently appears configuration
incorrect)
     - Running some of the Cmake generation, the pdb files can't be deleted
(shows up errors)
     - Depends upon visual studio generator code to sort dependencies
     - When generating CMAKE from scratch, it sometimes errors with fortran
complaints and fails generation?
     a re-run will succeed.
     - Linker for msvc uses the cmake command to callback. Don't think this is
an issue since
     I think compilation is the part that gets distributed anyway.
     But it might mean that the cache has trouble calculating deps for
obj->lib/exe.
     Not sure if Fastbuild supports that anyway yet
     - Need to sort custom build commands by their outputs

    Fastbuild bugs:
     - Defining prebuild dependencies that don't exist, causes the error output
when that
     target is actually defined. Rather than originally complaining that the
target
     doesn't exist where the reference is attempted.
     - Parsing strings with double $$ doesn't generate a nice error
     - Undocumented that you can escape a $ with ^$
     - ExecInputs is invalid empty
     - Would be great if you could define dummy targets (maybe blank aliases?)
     - Exec nodes need to not worry about dummy output files not being created
     - Would be nice if nodes didn't need to be completely in order. But then
cycles would be possible
     - Implib directory is not created for exeNodes (DLLs work now though)

    Limitations:
     - Only tested/working with MSVC

    Notes:
     - Understanding Custom Build Steps and Build Events
         https://msdn.microsoft.com/en-us/library/e85wte0k.aspx
     very useful documentation detailing the order of execution
     of standard MSVC events. This is useful to determine correct
     behaviour of fastbuild generator (view a project generated to MSVC,
     then apply the same rules/assumptions back into fastbuild).
     i.e. Custom rules are always executed first.

    Current list of unit tests failing:
================ 3.2 ===========================
    91% tests passed, 32 tests failed out of 371
    Total Test time (real) = 1479.86 sec

The following tests FAILED:
     59 - Preprocess (Failed)
     60 - ExportImport (Failed)
     77 - Module.ExternalData (Failed)
    108 - CustomCommandByproducts (Failed)
    112 - BuildDepends (Failed)
    113 - SimpleInstall (Failed)
    114 - SimpleInstall-Stage2 (Failed)
    131 - ExternalProjectLocal (Failed)
    132 - ExternalProjectUpdateSetup (Failed)
    133 - ExternalProjectUpdate (Failed)
    274 - RunCMake.Configure (Failed)
    370 - CMake.CheckSourceTree (Failed)

================ 3.3 ===========================
97% tests passed, 11 tests failed out of 381

Label Time Summary:
Label1    =   1.49 sec (1 test)
Label2    =   1.49 sec (1 test)

Total Test time (real) = 1144.83 sec

The following tests FAILED:
         58 - Preprocess (Failed)
         76 - Module.ExternalData (Failed)
        111 - BuildDepends (Failed)
        112 - SimpleInstall (Failed)
        113 - SimpleInstall-Stage2 (Failed)
        131 - ExternalProjectLocal (Failed)
        133 - ExternalProjectUpdate (Failed)
        274 - RunCMake.CMP0060 (Failed)
        278 - RunCMake.Configure (Failed)
        294 - RunCMake.VisibilityPreset (Failed)
        381 - CMake.CheckSourceTree (Failed)
Errors while running CTest

================ 3.4 ===========================
95% tests passed, 19 tests failed out of 397

Label Time Summary:
Label1    =   0.88 sec (1 test)
Label2    =   0.88 sec (1 test)

Total Test time (real) = 610.23 sec

The following tests FAILED:
         37 - MSManifest (Failed)
         61 - Preprocess (Failed)
         62 - ExportImport (Failed)
         79 - Module.ExternalData (Failed)
        114 - BuildDepends (Failed)
        115 - SimpleInstall (Failed)
        116 - SimpleInstall-Stage2 (Failed)
        132 - ExternalProject (Failed)
        134 - ExternalProjectLocal (Failed)
        136 - ExternalProjectUpdate (Failed)
        156 - SubDir (Failed)
        158 - PDBDirectoryAndName (Failed)
        226 - InterfaceLinkLibraries (Failed)
        275 - RunCMake.CMP0060 (Failed)
        280 - RunCMake.BuildDepends (Failed)
        282 - RunCMake.Configure (Failed)
        303 - RunCMake.VisibilityPreset (Failed)
        366 - RunCMake.AutoExportDll (Failed)
        397 - CMake.CheckSourceTree (Failed)
Errors while running CTest

================ 3.5 ===========================
94% tests passed, 23 tests failed out of 398

Label Time Summary:
Label1    =   0.91 sec (1 test)
Label2    =   0.91 sec (1 test)

Total Test time (real) = 630.50 sec

The following tests FAILED:
         37 - MSManifest (Failed)
         41 - COnly (Failed)
         46 - ObjectLibrary (Failed)
         61 - Preprocess (Failed)
         62 - ExportImport (Failed)
         79 - Module.ExternalData (Failed)
        109 - CustomCommand (Failed)
        114 - BuildDepends (Failed)
        115 - SimpleInstall (Failed)
        116 - SimpleInstall-Stage2 (Failed)
        132 - ExternalProject (Failed)
        134 - ExternalProjectLocal (Failed)
        153 - Plugin (SEGFAULT)
        156 - SubDir (Failed)
        194 - CMakeCommands.target_link_libraries (Failed)
        225 - IncludeDirectories (Failed)
        275 - RunCMake.CMP0060 (Failed)
        277 - RunCMake.CMP0065 (Failed)
        280 - RunCMake.BuildDepends (Failed)
        282 - RunCMake.Configure (Failed)
        360 - RunCMake.ExternalProject (Failed)
        367 - RunCMake.AutoExportDll (Failed)
        398 - CMake.CheckSourceTree (Failed)
Errors while running CTest

============================================================================*/
#include "cmGlobalFastbuildGenerator.h"

#include "cmComputeLinkInformation.h"
#include "cmCustomCommandGenerator.h"
#include "cmGeneratorTarget.h"
#include "cmGlobalGeneratorFactory.h"
#include "cmGlobalVisualStudioGenerator.h"
#include "cmLocalFastbuildGenerator.h"
#include "cmLocalGenerator.h"
#include "cmMakefile.h"
#include "cmSourceFile.h"
#include "cmTarget.h"
#include <assert.h>
#include <cmsys/Encoding.hxx>

#include "cmFastbuildNormalTargetGenerator.h"
#include "cmFastbuildUtilityTargetGenerator.h"

const char* cmGlobalFastbuildGenerator::FASTBUILD_DOLLAR_TAG =
  "FASTBUILD_DOLLAR_TAG";
#define FASTBUILD_DOLLAR_TAG "FASTBUILD_DOLLAR_TAG"

cmGlobalFastbuildGenerator::Detail::FileContext::FileContext()
{
}

void cmGlobalFastbuildGenerator::Detail::FileContext::setFileName(
  const std::string fileName)
{
  fout.Open(fileName.c_str());
  fout.SetCopyIfDifferent(true);
}

void cmGlobalFastbuildGenerator::Detail::FileContext::close()
{
  // Close file
  fout.Close();
}

void cmGlobalFastbuildGenerator::Detail::FileContext::WriteComment(
  const std::string& comment)
{
  fout << linePrefix << ";" << comment << "\n";
}

void cmGlobalFastbuildGenerator::Detail::FileContext::WriteBlankLine()
{
  fout << "\n";
}

void cmGlobalFastbuildGenerator::Detail::FileContext::WriteHorizontalLine()
{
  fout << ";----------------------------------------------------------------"
          "---------------\n";
}

void cmGlobalFastbuildGenerator::Detail::FileContext::WriteSectionHeader(
  const char* section)
{
  fout << "\n";
  WriteHorizontalLine();
  WriteComment(section);
  WriteHorizontalLine();
}

void cmGlobalFastbuildGenerator::Detail::FileContext::WritePushScope(
  char begin, char end)
{
  fout << linePrefix << begin << "\n";
  linePrefix += "\t";
  closingScope += end;
}

void cmGlobalFastbuildGenerator::Detail::FileContext::WritePushScopeStruct()
{
  WritePushScope('[', ']');
}

void cmGlobalFastbuildGenerator::Detail::FileContext::WritePopScope()
{
  assert(!linePrefix.empty());
  linePrefix.resize(linePrefix.size() - 1);

  fout << linePrefix << closingScope[closingScope.size() - 1] << "\n";

  closingScope.resize(closingScope.size() - 1);
}

void cmGlobalFastbuildGenerator::Detail::FileContext::WriteVariable(
  const std::string& key, const std::string& value,
  const std::string& operation)
{
  fout << linePrefix << "." << key << " " << operation << " " << value << "\n";
}

void cmGlobalFastbuildGenerator::Detail::FileContext::WriteCommand(
  const std::string& command, const std::string& value)
{
  fout << linePrefix << command;
  if (!value.empty()) {
    fout << "(" << value << ")";
  }
  fout << "\n";
}

void cmGlobalFastbuildGenerator::Detail::FileContext::WriteArray(
  const std::string& key, const std::vector<std::string>& values,
  const std::string& operation)
{
  WriteVariable(key, "", operation);
  WritePushScope();
  size_t size = values.size();
  for (size_t index = 0; index < size; ++index) {
    const std::string& value = values[index];
    bool isLast = index == size - 1;

    fout << linePrefix << value;
    if (!isLast) {
      fout << ',';
    }
    fout << "\n";
  }
  WritePopScope();
}

//----------------------------------------------------------------------------

std::string cmGlobalFastbuildGenerator::Detail::Detection::GetLastFolderName(
  const std::string& string)
{
  return string.substr(string.rfind('/'));
}

bool cmGlobalFastbuildGenerator::Detail::Detection::IsExcludedFromAll(
  cmGeneratorTarget* target)
{
  bool excluded = target->GetPropertyAsBool("EXCLUDE_FROM_ALL");
  // FIXME
  //        cmLocalGenerator* lg =
  //        target->GetMakefile()->GetLocalGenerator();
  //        while(lg->GetParent() != 0 && !excluded)
  //        {
  //            excluded =
  //            lg->GetMakefile()->GetPropertyAsBool("EXCLUDE_FROM_ALL");
  //            lg = lg->GetParent();
  //        }

  return excluded;
}

void cmGlobalFastbuildGenerator::Detail::Detection::UnescapeFastbuildVariables(
  std::string& string)
{
  // Unescape the Fastbuild configName symbol with $
  cmSystemTools::ReplaceString(string, "^", "^^");
  cmSystemTools::ReplaceString(string, "$$", "^$");
  cmSystemTools::ReplaceString(string, FASTBUILD_DOLLAR_TAG, "$");
  // cmSystemTools::ReplaceString(string, "$$ConfigName$$", "$ConfigName$");
  // cmSystemTools::ReplaceString(string, "^$ConfigName^$", "$ConfigName$");
}

void cmGlobalFastbuildGenerator::Detail::Detection::ResolveFastbuildVariables(
  std::string& string, const std::string& configName)
{
  // Replace Fastbuild configName with the config name
  cmSystemTools::ReplaceString(string, "$ConfigName$", configName);
}

std::string cmGlobalFastbuildGenerator::Detail::Detection::BuildCommandLine(
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

void cmGlobalFastbuildGenerator::Detail::Detection::DetectOutput(
  cmLocalCommonGenerator* lg, FastbuildTargetNames& targetNamesOut,
  const cmGeneratorTarget* generatorTarget, const std::string& configName)
{
  if (generatorTarget->GetType() == cmState::EXECUTABLE) {
    generatorTarget->GetExecutableNames(
      targetNamesOut.targetNameOut, targetNamesOut.targetNameReal,
      targetNamesOut.targetNameImport, targetNamesOut.targetNamePDB,
      configName);
  } else {
    generatorTarget->GetLibraryNames(
      targetNamesOut.targetNameOut, targetNamesOut.targetNameSO,
      targetNamesOut.targetNameReal, targetNamesOut.targetNameImport,
      targetNamesOut.targetNamePDB, configName);
  }

  if (generatorTarget->HaveWellDefinedOutputFiles()) {
    targetNamesOut.targetOutputDir =
      generatorTarget->GetDirectory(configName) + "/";

    targetNamesOut.targetOutput = generatorTarget->GetFullPath(configName);
    targetNamesOut.targetOutputReal =
      generatorTarget->GetFullPath(configName,
                                   /*implib=*/false,
                                   /*realpath=*/true);
    targetNamesOut.targetOutputImplib =
      generatorTarget->GetFullPath(configName,
                                   /*implib=*/true);
  } else {
    targetNamesOut.targetOutputDir =
      lg->GetMakefile()->GetHomeOutputDirectory();
    if (targetNamesOut.targetOutputDir.empty() ||
        targetNamesOut.targetOutputDir == ".") {
      targetNamesOut.targetOutputDir = generatorTarget->GetName();
    } else {
      targetNamesOut.targetOutputDir += "/";
      targetNamesOut.targetOutputDir += generatorTarget->GetName();
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

  if (generatorTarget->GetType() == cmState::EXECUTABLE ||
      generatorTarget->GetType() == cmState::STATIC_LIBRARY ||
      generatorTarget->GetType() == cmState::SHARED_LIBRARY ||
      generatorTarget->GetType() == cmState::MODULE_LIBRARY) {
    targetNamesOut.targetOutputPDBDir =
      generatorTarget->GetPDBDirectory(configName);
    targetNamesOut.targetOutputPDBDir += "/";
  }
  if (generatorTarget->GetType() <= cmState::OBJECT_LIBRARY) {
    targetNamesOut.targetOutputCompilePDBDir =
      generatorTarget->GetCompilePDBPath(configName);
    if (targetNamesOut.targetOutputCompilePDBDir.empty()) {
      targetNamesOut.targetOutputCompilePDBDir =
        generatorTarget->GetSupportDirectory() + "/";
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

void cmGlobalFastbuildGenerator::Detail::Detection::ComputeLinkCmds(
  std::vector<std::string>& linkCmds, cmLocalCommonGenerator* lg,
  const cmGeneratorTarget* gt, std::string configName)
{
  const std::string& linkLanguage = gt->GetLinkerLanguage(configName);
  cmMakefile* mf = lg->GetMakefile();
  {
    std::string linkCmdVar =
      gt->GetCreateRuleVariable(linkLanguage, configName);
    const char* linkCmd = mf->GetDefinition(linkCmdVar);
    if (linkCmd) {
      cmSystemTools::ExpandListArgument(linkCmd, linkCmds);
      return;
    }
  }

  // If the above failed, then lets try this:
  switch (gt->GetType()) {
    case cmState::STATIC_LIBRARY: {
      // We have archive link commands set. First, delete the existing
      // archive.
      {
        std::string cmakeCommand = lg->ConvertToOutputFormat(
          mf->GetRequiredDefinition("CMAKE_COMMAND"), cmLocalGenerator::SHELL);
        linkCmds.push_back(cmakeCommand + " -E remove $TARGET_FILE");
      }
      // TODO: Use ARCHIVE_APPEND for archives over a certain size.
      {
        std::string linkCmdVar = "CMAKE_";
        linkCmdVar += linkLanguage;
        linkCmdVar += "_ARCHIVE_CREATE";
        const char* linkCmd = mf->GetRequiredDefinition(linkCmdVar);
        cmSystemTools::ExpandListArgument(linkCmd, linkCmds);
      }
      {
        std::string linkCmdVar = "CMAKE_";
        linkCmdVar += linkLanguage;
        linkCmdVar += "_ARCHIVE_FINISH";
        const char* linkCmd = mf->GetRequiredDefinition(linkCmdVar);
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

std::string cmGlobalFastbuildGenerator::Detail::Detection::ComputeDefines(
  cmLocalCommonGenerator* lg, const cmGeneratorTarget* generatorTarget,
  const cmSourceFile* source, const std::string& configName,
  const std::string& language)
{
  std::set<std::string> defines;

  // Add the export symbol definition for shared library objects.
  if (const char* exportMacro = generatorTarget->GetExportMacro()) {
    lg->AppendDefines(defines, exportMacro);
  }

  // Add preprocessor definitions for this target and configuration.
  lg->AddCompileDefinitions(defines, generatorTarget, configName, language);

  if (source) {
    lg->AppendDefines(defines, source->GetProperty("COMPILE_DEFINITIONS"));

    std::string defPropName = "COMPILE_DEFINITIONS_";
    defPropName += cmSystemTools::UpperCase(configName);
    lg->AppendDefines(defines, source->GetProperty(defPropName));
  }

  // Add a definition for the configuration name.
  // NOTE: CMAKE_TEST_REQUIREMENT The following was added specifically to
  // facillitate cmake testing. Doesn't feel right to do this...
  std::string configDefine = "CMAKE_INTDIR=\"";
  configDefine += configName;
  configDefine += "\"";
  lg->AppendDefines(defines, configDefine);

  std::string definesString;
  lg->JoinDefines(defines, definesString, language);

  return definesString;
}

void cmGlobalFastbuildGenerator::Detail::Detection::DetectLinkerLibPaths(
  std::string& linkerLibPath, cmLocalCommonGenerator* lg,
  const cmGeneratorTarget* generatorTarget, const std::string& configName)
{
  cmMakefile* pMakefile = lg->GetMakefile();
  cmComputeLinkInformation* pcli =
    generatorTarget->GetLinkInformation(configName);
  if (!pcli) {
    // No link information, then no linker library paths
    return;
  }
  cmComputeLinkInformation& cli = *pcli;

  std::string libPathFlag =
    pMakefile->GetRequiredDefinition("CMAKE_LIBRARY_PATH_FLAG");
  std::string libPathTerminator =
    pMakefile->GetSafeDefinition("CMAKE_LIBRARY_PATH_TERMINATOR");

  // Append the library search path flags.
  std::vector<std::string> const& libDirs = cli.GetDirectories();
  for (std::vector<std::string>::const_iterator libDir = libDirs.begin();
       libDir != libDirs.end(); ++libDir) {
    std::string libpath = lg->ConvertToOutputForExisting(
      *libDir, cmLocalGenerator::START_OUTPUT, cmLocalGenerator::SHELL);
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

bool cmGlobalFastbuildGenerator::Detail::Detection::DetectBaseLinkerCommand(
  std::string& command, cmLocalFastbuildGenerator* lg,
  const cmGeneratorTarget* gt, const std::string& configName)
{
  const std::string& linkLanguage = gt->GetLinkerLanguage(configName);
  if (linkLanguage.empty()) {
    cmSystemTools::Error("CMake can not determine linker language for "
                         "target: ",
                         gt->GetName().c_str());
    return false;
  }

  cmLocalGenerator::RuleVariables vars;
  vars.RuleLauncher = "RULE_LAUNCH_LINK";
  // FIXME const cast are evil
  vars.CMTarget = (cmGeneratorTarget*)gt;
  vars.Language = linkLanguage.c_str();
  vars.Manifests = "";

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
    gt->GetTargetVersion(major, minor);
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
  ComputeLinkCmds(linkCmds, lg, gt, configName);
  for (std::vector<std::string>::iterator i = linkCmds.begin();
       i != linkCmds.end(); ++i) {
    lg->ExpandRuleVariables(*i, vars);
  }

  command = BuildCommandLine(linkCmds);

  return true;
}

void cmGlobalFastbuildGenerator::Detail::Detection::SplitExecutableAndFlags(
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

void cmGlobalFastbuildGenerator::Detail::Detection::DetectBaseCompileCommand(
  std::string& command, cmLocalFastbuildGenerator* lg,
  const cmGeneratorTarget* generatorTarget, const std::string& language)
{
  cmLocalGenerator::RuleVariables compileObjectVars;
  // FIXME const cast are evil
  compileObjectVars.CMTarget = (cmGeneratorTarget*)generatorTarget;
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
  compileObjectVars.Manifests = "";
  compileObjectVars.Defines = "";
  compileObjectVars.TargetCompilePDB = FASTBUILD_DOLLAR_TAG
    "TargetOutCompilePDBDir" FASTBUILD_DOLLAR_TAG FASTBUILD_DOLLAR_TAG
    "TargetNamePDB" FASTBUILD_DOLLAR_TAG;

  // Rule for compiling object file.
  std::string compileCmdVar = "CMAKE_";
  compileCmdVar += language;
  compileCmdVar += "_COMPILE_OBJECT";
  std::string compileCmd =
    lg->GetMakefile()->GetRequiredDefinition(compileCmdVar);
  std::vector<std::string> compileCmds;
  cmSystemTools::ExpandListArgument(compileCmd, compileCmds);

  for (std::vector<std::string>::iterator i = compileCmds.begin();
       i != compileCmds.end(); ++i) {
    std::string& compileCmdStr = *i;
    lg->ExpandRuleVariables(compileCmdStr, compileObjectVars);
  }

  command = BuildCommandLine(compileCmds);
}

void cmGlobalFastbuildGenerator::Detail::Detection::DetectLanguages(
  std::set<std::string>& languages, const cmGeneratorTarget* generatorTarget)
{
  // Object libraries do not have linker stages
  // nor utilities
  bool hasObjectGroups = generatorTarget->GetType() != cmState::UTILITY &&
    generatorTarget->GetType() != cmState::GLOBAL_TARGET;
  if (!hasObjectGroups) {
    return;
  }

  std::vector<std::string> configs;
  generatorTarget->Makefile->GetConfigurations(configs, false);
  for (std::vector<std::string>::const_iterator iter = configs.begin();
       iter != configs.end(); ++iter) {
    const std::string& configName = *iter;

    std::vector<const cmSourceFile*> sourceFiles;
    generatorTarget->GetObjectSources(sourceFiles, configName);
    for (std::vector<const cmSourceFile*>::const_iterator i =
           sourceFiles.begin();
         i != sourceFiles.end(); ++i) {
      const std::string& lang = (*i)->GetLanguage();
      if (!lang.empty()) {
        languages.insert(lang);
      }
    }
  }
}

void cmGlobalFastbuildGenerator::Detail::Detection::FilterSourceFiles(
  std::vector<cmSourceFile const*>& filteredSourceFiles,
  std::vector<cmSourceFile const*>& sourceFiles, const std::string& language)
{
  for (std::vector<cmSourceFile const*>::const_iterator i =
         sourceFiles.begin();
       i != sourceFiles.end(); ++i) {
    const cmSourceFile* sf = *i;
    if (sf->GetLanguage() == language) {
      filteredSourceFiles.push_back(sf);
    }
  }
}

void cmGlobalFastbuildGenerator::Detail::Detection::DetectCompilerFlags(
  std::string& compileFlags, cmLocalCommonGenerator* lg,
  const cmGeneratorTarget* gt, const cmSourceFile* source,
  const std::string& language, const std::string& configName)
{
  lg->AddLanguageFlags(compileFlags, language, configName);

  lg->AddArchitectureFlags(compileFlags, gt, language, configName);

  // Add shared-library flags if needed.
  lg->AddCMP0018Flags(compileFlags, gt, language, configName);

  lg->AddVisibilityPresetFlags(compileFlags, gt, language);

  std::vector<std::string> includes;
  lg->GetIncludeDirectories(includes, gt, language, configName);

  // Add include directory flags.
  // FIXME const cast are evil
  std::string includeFlags = lg->GetIncludeFlags(
    includes, (cmGeneratorTarget*)gt, language,
    language == "RC" ? true : false, // full include paths for RC
    // needed by cmcldeps
    false, configName);

  lg->AppendFlags(compileFlags, includeFlags);

  // Append old-style preprocessor definition flags.
  lg->AppendFlags(compileFlags, lg->GetMakefile()->GetDefineFlags());

  // Add target-specific flags.
  // FIXME const cast are evil
  lg->AddCompileOptions(compileFlags, (cmGeneratorTarget*)gt, language,
                        configName);

  if (source) {
    lg->AppendFlags(compileFlags, source->GetProperty("COMPILE_FLAGS"));
  }
}

void cmGlobalFastbuildGenerator::Detail::Detection::
  DetectTargetLinkDependencies(const cmGeneratorTarget* generatorTarget,
                               const std::string& configName,
                               std::vector<std::string>& dependencies)
{
  // Static libraries never depend on other targets for linking.
  if (generatorTarget->GetType() == cmState::STATIC_LIBRARY ||
      generatorTarget->GetType() == cmState::OBJECT_LIBRARY) {
    return;
  }

  cmComputeLinkInformation* cli =
    generatorTarget->GetLinkInformation(configName);
  if (!cli) {
    return;
  }

  const std::vector<std::string>& deps = cli->GetDepends();
  std::copy(deps.begin(), deps.end(), std::back_inserter(dependencies));
}

std::string
cmGlobalFastbuildGenerator::Detail::Detection::DetectTargetCompileOutputDir(
  cmLocalCommonGenerator* lg, const cmGeneratorTarget* generatorTarget,
  std::string configName)
{
  std::string result = lg->GetTargetDirectory(generatorTarget) + "/";
  if (!configName.empty()) {
    result = result + configName + "/";
  }
  cmSystemTools::ConvertToOutputSlashes(result);
  return result;
}

void cmGlobalFastbuildGenerator::Detail::Detection::
  DetectTargetObjectDependencies(cmGlobalCommonGenerator* gg,
                                 const cmGeneratorTarget* gt,
                                 const std::string& configName,
                                 std::vector<std::string>& dependencies)
{
  // Iterate over all source files and look for
  // object file dependencies

  std::set<std::string> objectLibs;

  std::vector<cmSourceFile*> sourceFiles;
  gt->GetSourceFiles(sourceFiles, configName);
  for (std::vector<cmSourceFile*>::const_iterator i = sourceFiles.begin();
       i != sourceFiles.end(); ++i) {
    const std::string& objectLib = (*i)->GetObjectLibrary();
    if (!objectLib.empty()) {
      // Find the target this actually is (might be an alias)
      const cmTarget* objectTarget = gg->FindTarget(objectLib);
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
  gt->GetExternalObjects(objFiles, configName);
  for (std::vector<const cmSourceFile*>::const_iterator i = objFiles.begin();
       i != objFiles.end(); ++i) {
    const cmSourceFile* sourceFile = *i;
    if (sourceFile->GetObjectLibrary().empty()) {
      dependencies.push_back(sourceFile->GetFullPath());
    }
  }
}

void cmGlobalFastbuildGenerator::Detail::Detection::DependencySorter::
  TargetHelper::GetOutputs(const cmGeneratorTarget* entry,
                           std::vector<std::string>& outputs)
{
  outputs.push_back(entry->GetName());
}

void cmGlobalFastbuildGenerator::Detail::Detection::DependencySorter::
  TargetHelper::GetInputs(const cmGeneratorTarget* entry,
                          std::vector<std::string>& inputs)
{
  TargetDependSet const& ts = gg->GetTargetDirectDepends(entry);
  for (TargetDependSet::const_iterator iter = ts.begin(); iter != ts.end();
       ++iter) {
    const cmGeneratorTarget* dtarget = *iter;
    inputs.push_back(dtarget->GetName());
  }
}

void cmGlobalFastbuildGenerator::Detail::Detection::DependencySorter::
  CustomCommandHelper::GetOutputs(const cmSourceFile* entry,
                                  std::vector<std::string>& outputs)
{
  const cmCustomCommand* cc = entry->GetCustomCommand();
  cmMakefile* makefile = lg->GetMakefile();

  // We need to generate the command for execution.
  cmCustomCommandGenerator ccg(*cc, configName, lg);

  const std::vector<std::string>& ccOutputs = ccg.GetOutputs();
  const std::vector<std::string>& byproducts = ccg.GetByproducts();
  outputs.insert(outputs.end(), ccOutputs.begin(), ccOutputs.end());
  outputs.insert(outputs.end(), byproducts.begin(), byproducts.end());
}

void cmGlobalFastbuildGenerator::Detail::Detection::DependencySorter::
  CustomCommandHelper::GetInputs(const cmSourceFile* entry,
                                 std::vector<std::string>& inputs)
{
  const cmCustomCommand* cc = entry->GetCustomCommand();
  cmMakefile* makefile = lg->GetMakefile();
  cmCustomCommandGenerator ccg(*cc, configName, lg);

  std::string workingDirectory = ccg.GetWorkingDirectory();
  if (workingDirectory.empty()) {
    workingDirectory = makefile->GetCurrentBinaryDirectory();
    workingDirectory += "/";
  }

  // Take the dependencies listed and split into targets and files.
  const std::vector<std::string>& depends = ccg.GetDepends();
  for (std::vector<std::string>::const_iterator iter = depends.begin();
       iter != depends.end(); ++iter) {
    std::string dep = *iter;

    bool isTarget = gg->FindTarget(dep) != NULL;
    if (!isTarget) {
      if (!cmSystemTools::FileIsFullPath(dep.c_str())) {
        dep = workingDirectory + dep;
      }

      inputs.push_back(dep);
    }
  }
}

void cmGlobalFastbuildGenerator::Detail::Detection::
  ComputeTargetOrderAndDependencies(cmGlobalFastbuildGenerator* gg,
                                    OrderedTargetSet& orderedTargets)
{
  TargetDependSet projectTargets;
  TargetDependSet originalTargets;
  std::map<std::string, std::vector<cmLocalGenerator *> >::const_iterator
    it = gg->GetProjectMap().begin(),
    end = gg->GetProjectMap().end();
  for (; it != end; ++it) {
    const std::vector<cmLocalGenerator*>& generators = it->second;
    cmLocalFastbuildGenerator* root =
      static_cast<cmLocalFastbuildGenerator*>(generators[0]);

    // Given this information, calculate the dependencies:
    // Collect all targets under this root generator and the transitive
    // closure of their dependencies.

    gg->GetTargetSets(projectTargets, originalTargets, root, generators);
  }

  // Iterate over the targets and export their order
  for (TargetDependSet::iterator iter = projectTargets.begin();
       iter != projectTargets.end(); ++iter) {
    const cmTargetDepend& targetDepend = *iter;
    const cmGeneratorTarget* target = targetDepend.operator->();

    orderedTargets.push_back(target);
  }

  DependencySorter::TargetHelper targetHelper = { gg };
  DependencySorter::Sort(targetHelper, orderedTargets);
}

// Iterate over all targets and remove the ones that are
// not needed for generation.
// i.e. the nested global targets
bool cmGlobalFastbuildGenerator::Detail::Detection::RemovalTest::operator()(
  const cmGeneratorTarget* target) const
{
  if (target->GetType() == cmState::GLOBAL_TARGET) {
    // We only want to process global targets that live in the home
    // (i.e. top-level) directory.  CMake creates copies of these targets
    // in every directory, which we don't need.
    cmMakefile* mf = target->Target->GetMakefile();
    if (strcmp(mf->GetCurrentSourceDirectory(), mf->GetHomeDirectory()) != 0) {
      return true;
    }
  }
  return false;
}

void cmGlobalFastbuildGenerator::Detail::Detection::StripNestedGlobalTargets(
  OrderedTargetSet& orderedTargets)
{
  orderedTargets.erase(std::remove_if(orderedTargets.begin(),
                                      orderedTargets.end(), RemovalTest()),
                       orderedTargets.end());
}

bool cmGlobalFastbuildGenerator::Detail::Detection::isConfigDependant(
  const cmCustomCommandGenerator* ccg)
{
  typedef std::vector<std::string> StringVector;
  StringVector outputs = ccg->GetOutputs();
  StringVector byproducts = ccg->GetByproducts();

  std::for_each(outputs.begin(), outputs.end(),
                &Detection::UnescapeFastbuildVariables);
  std::for_each(byproducts.begin(), byproducts.end(),
                &Detection::UnescapeFastbuildVariables);

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

void cmGlobalFastbuildGenerator::Detail::Detection::DetectCompilerExtraFiles(
  const std::string& compilerID, const std::string& version,
  std::vector<std::string>& extraFiles)
{
  // Output a list of files that are relative to $CompilerRoot$
  return;

  if (compilerID == "MSVC") {
    if (version.compare(0, 3, "18.") != std::string::npos) {
      // Using vs2013
      const char* vs2013_extraFiles[13] = {
        "$CompilerRoot$\\c1.dll", "$CompilerRoot$\\c1ast.dll",
        "$CompilerRoot$\\c1xx.dll", "$CompilerRoot$\\c1xxast.dll",
        "$CompilerRoot$\\c2.dll", "$CompilerRoot$\\msobj120.dll",
        "$CompilerRoot$\\mspdb120.dll", "$CompilerRoot$\\mspdbcore.dll",
        "$CompilerRoot$\\mspft120.dll", "$CompilerRoot$\\1033\\clui.dll",
        "$CompilerRoot$\\..\\..\\redist\\x86\\Microsoft.VC120.CRT\\msvcp120."
        "dll",
        "$CompilerRoot$\\..\\..\\redist\\x86\\Microsoft.VC120.CRT\\msvcr120."
        "dll",
        "$CompilerRoot$\\..\\..\\redist\\x86\\Microsoft.VC120."
        "CRT\\vccorlib120.dll"
      };
      extraFiles.insert(extraFiles.end(), &vs2013_extraFiles[0],
                        &vs2013_extraFiles[13]);
    } else if (version.compare(0, 3, "17.") != std::string::npos) {
      // Using vs2012
      const char* vs2012_extraFiles[12] = {
        "$CompilerRoot$\\c1.dll", "$CompilerRoot$\\c1ast.dll",
        "$CompilerRoot$\\c1xx.dll", "$CompilerRoot$\\c1xxast.dll",
        "$CompilerRoot$\\c2.dll", "$CompilerRoot$\\mspft110.dll",
        "$CompilerRoot$\\1033\\clui.dll", "$CompilerRoot$\\..\\.."
                                          "\\redist\\x86\\Microsoft.VC110."
                                          "CRT\\msvcp110.dll",
        "$CompilerRoot$\\..\\..\\redist\\x86\\Microsoft.VC110.CRT\\msvcr110."
        "dll",
        "$CompilerRoot$\\..\\..\\redist\\x86\\Microsoft.VC110."
        "CRT\\vccorlib110.dll",
        "$CompilerRoot$\\..\\..\\..\\Common7\\IDE\\mspdb110.dll",
        "$CompilerRoot$\\..\\..\\..\\Common7\\IDE\\mspdbcore.dll"
      };
      extraFiles.insert(extraFiles.end(), &vs2012_extraFiles[0],
                        &vs2012_extraFiles[12]);
    }
  }
}

//----------------------------------------------------------------------------
class cmGlobalFastbuildGenerator::Detail::Definition
{
  struct FastbuildSettings
  {
    std::string cachePath;
  };

  struct FastbuildCompiler
  {
    std::string executable;
  };
  typedef std::map<std::string, FastbuildCompiler> CompilerMap;

  struct FastbuildConfiguration
  {
    std::string name;
  };
  typedef std::map<std::string, FastbuildConfiguration> ConfigurationMap;

  struct FastbuildStructure
  {
    ConfigurationMap configurations;

    FastbuildSettings settings;
    CompilerMap compilers;
  };
};

//----------------------------------------------------------------------------

std::string cmGlobalFastbuildGenerator::Detail::Generation::Quote(
  const std::string& str, const std::string& quotation)
{
  std::string result = str;
  cmSystemTools::ReplaceString(result, quotation, "^" + quotation);
  return quotation + result + quotation;
}

std::string cmGlobalFastbuildGenerator::Detail::Generation::Join(
  const std::vector<std::string>& elems, const std::string& delim)
{
  std::stringstream stringstream;
  for (std::vector<std::string>::const_iterator iter = elems.begin();
       iter != elems.end(); ++iter) {
    stringstream << (*iter);
    if (iter + 1 != elems.end()) {
      stringstream << delim;
    }
  }

  return stringstream.str();
}

std::vector<std::string> cmGlobalFastbuildGenerator::Detail::Generation::Wrap(
  const std::vector<std::string>& in, const std::string& prefix,
  const std::string& suffix)
{
  std::vector<std::string> result;

  WrapHelper helper = { prefix, suffix };

  std::transform(in.begin(), in.end(), std::back_inserter(result), helper);

  return result;
}

std::string cmGlobalFastbuildGenerator::Detail::Generation::EncodeLiteral(
  const std::string& lit)
{
  std::string result = lit;
  cmSystemTools::ReplaceString(result, "$", "^$");
  return result;
}

void cmGlobalFastbuildGenerator::Detail::Generation::BuildTargetContexts(
  cmGlobalFastbuildGenerator* gg, TargetContextMap& map)
{
  std::map<std::string, std::vector<cmLocalGenerator *> >::const_iterator
    it = gg->GetProjectMap().begin(),
    end = gg->GetProjectMap().end();
  for (; it != end; ++it) {
    const std::vector<cmLocalGenerator*>& generators = it->second;
    cmLocalFastbuildGenerator* root =
      static_cast<cmLocalFastbuildGenerator*>(generators[0]);

    // Build a map of all targets to their local generator
    for (std::vector<cmLocalGenerator*>::const_iterator iter =
           generators.begin();
         iter != generators.end(); ++iter) {
      cmLocalFastbuildGenerator* lg =
        static_cast<cmLocalFastbuildGenerator*>(*iter);

      if (gg->IsExcluded(root, lg)) {
        continue;
      }

      for (std::vector<cmGeneratorTarget*>::const_iterator targetIter =
             lg->GetGeneratorTargets().begin();
           targetIter != lg->GetGeneratorTargets().end(); ++targetIter) {
        cmGeneratorTarget* generatorTarget = *targetIter;
        if (gg->IsRootOnlyTarget(generatorTarget) &&
            lg->GetMakefile() != root->GetMakefile()) {
          continue;
        }

        TargetGenerationContext targetContext = { generatorTarget, root,
                                                  generators, lg };
        map[generatorTarget] = targetContext;
      }
    }
  }
}

void cmGlobalFastbuildGenerator::Detail::Generation::GenerateRootBFF(
  cmGlobalFastbuildGenerator* self)
{

  cmLocalFastbuildGenerator* root =
    static_cast<cmLocalFastbuildGenerator*>(self->GetLocalGenerators()[0]);

  // Calculate filename
  std::string fname = root->GetMakefile()->GetHomeOutputDirectory();
  fname += "/fbuild.bff";

  self->g_fc.setFileName(fname);
  GenerationContext context(self, root, self->g_fc);
  Detection::ComputeTargetOrderAndDependencies(self, context.orderedTargets);
  Detection::StripNestedGlobalTargets(context.orderedTargets);
  BuildTargetContexts(self, context.targetContexts);
  WriteRootBFF(context);

  self->g_fc.close();
  self->FileReplacedDuringGenerate(fname);
}

void cmGlobalFastbuildGenerator::Detail::Generation::WriteRootBFF(
  GenerationContext& context)
{
  context.fc.WriteSectionHeader("Fastbuild makefile - Generated using CMAKE");

  WritePlaceholders(context.fc);
  WriteSettings(context.fc,
                context.self->GetCMakeInstance()->GetHomeOutputDirectory());
  WriteCompilers(context);
  WriteConfigurations(context.fc, context.root->GetMakefile());

  // Sort targets
  WriteTargetDefinitions(context, false);
  WriteAliases(context, false);
  WriteTargetDefinitions(context, true);
  WriteAliases(context, true);
}

void cmGlobalFastbuildGenerator::Detail::Generation::WritePlaceholders(
  FileContext& fileContext)
{
  // Define some placeholder
  fileContext.WriteSectionHeader("Helper variables");

  fileContext.WriteVariable("FB_INPUT_1_PLACEHOLDER", Quote("\"%1\""));
  fileContext.WriteVariable("FB_INPUT_2_PLACEHOLDER", Quote("\"%2\""));
}

void cmGlobalFastbuildGenerator::Detail::Generation::WriteSettings(
  FileContext& fileContext, std::string cacheDir)
{
  fileContext.WriteSectionHeader("Settings");

  fileContext.WriteCommand("Settings");
  fileContext.WritePushScope();

  cacheDir += "\\.fbuild.cache";
  cmSystemTools::ConvertToOutputSlashes(cacheDir);

  fileContext.WriteVariable("CachePath", Quote(cacheDir));
  fileContext.WritePopScope();
}

bool cmGlobalFastbuildGenerator::Detail::Generation::WriteCompilers(
  GenerationContext& context)
{
  cmMakefile* mf = context.root->GetMakefile();

  context.fc.WriteSectionHeader("Compilers");

  // Detect each language used in the definitions
  std::set<std::string> languages;
  for (TargetContextMap::iterator iter = context.targetContexts.begin();
       iter != context.targetContexts.end(); ++iter) {
    TargetGenerationContext& targetContext = iter->second;

    if (targetContext.target->GetType() == cmState::INTERFACE_LIBRARY) {
      continue;
    }

    Detection::DetectLanguages(languages, targetContext.target);
  }

  // Now output a compiler for each of these languages
  typedef std::map<std::string, std::string> StringMap;
  typedef std::map<std::string, CompilerDef> CompilerDefMap;
  CompilerDefMap compilerToDef;
  StringMap languageToCompiler;
  for (std::set<std::string>::iterator iter = languages.begin();
       iter != languages.end(); ++iter) {
    const std::string& language = *iter;

    // Calculate the root location of the compiler
    std::string variableString = "CMAKE_" + language + "_COMPILER";
    std::string compilerLocation = mf->GetSafeDefinition(variableString);
    if (compilerLocation.empty()) {
      return false;
    }

    // Add the language to the compiler's name
    CompilerDef& compilerDef = compilerToDef[compilerLocation];
    if (compilerDef.name.empty()) {
      compilerDef.name = "Compiler";
      compilerDef.path = compilerLocation;
      compilerDef.cmakeCompilerID =
        mf->GetSafeDefinition("CMAKE_" + language + "_COMPILER_ID");
      compilerDef.cmakeCompilerVersion =
        mf->GetSafeDefinition("CMAKE_" + language + "_COMPILER_VERSION");
    }
    compilerDef.name += "-";
    compilerDef.name += language;

    // Now add the language to point to that compiler location
    languageToCompiler[language] = compilerLocation;
  }

  // Now output all the compilers
  for (CompilerDefMap::iterator iter = compilerToDef.begin();
       iter != compilerToDef.end(); ++iter) {
    const CompilerDef& compilerDef = iter->second;

    // Detect the list of extra files used by this compiler
    // for distribution
    std::vector<std::string> extraFiles;
    Detection::DetectCompilerExtraFiles(compilerDef.cmakeCompilerID,
                                        compilerDef.cmakeCompilerVersion,
                                        extraFiles);

    // Strip out the path to the compiler
    std::string compilerPath =
      cmSystemTools::GetFilenamePath(compilerDef.path);
    std::string compilerFile =
      "$CompilerRoot$\\" + cmSystemTools::GetFilenameName(compilerDef.path);

    cmSystemTools::ConvertToOutputSlashes(compilerPath);
    cmSystemTools::ConvertToOutputSlashes(compilerFile);

    // Write out the compiler that has been configured
    context.fc.WriteCommand("Compiler", Quote(compilerDef.name));
    context.fc.WritePushScope();
    context.fc.WriteVariable("CompilerRoot", Quote(compilerPath));
    context.fc.WriteVariable("Executable", Quote(compilerFile));
    context.fc.WriteArray("ExtraFiles", Wrap(extraFiles));

    context.fc.WritePopScope();
  }

  // Now output the compiler names according to language as variables
  for (StringMap::iterator iter = languageToCompiler.begin();
       iter != languageToCompiler.end(); ++iter) {
    const std::string& language = iter->first;
    const std::string& compilerLocation = iter->second;
    const CompilerDef& compilerDef = compilerToDef[compilerLocation];

    // Output a default compiler to absorb the library requirements for a
    // compiler
    if (iter == languageToCompiler.begin()) {
      context.fc.WriteVariable("Compiler_dummy", Quote(compilerDef.name));
    }

    context.fc.WriteVariable("Compiler_" + language, Quote(compilerDef.name));
  }

  return true;
}

void cmGlobalFastbuildGenerator::Detail::Generation::WriteConfigurations(
  FileContext& fileContext, cmMakefile* makefile)
{
  fileContext.WriteSectionHeader("Configurations");

  fileContext.WriteVariable("ConfigBase", "");
  fileContext.WritePushScopeStruct();
  fileContext.WritePopScope();

  // Iterate over all configurations and define them:
  std::vector<std::string> configs;
  makefile->GetConfigurations(configs, false);
  for (std::vector<std::string>::const_iterator iter = configs.begin();
       iter != configs.end(); ++iter) {
    const std::string& configName = *iter;
    fileContext.WriteVariable("config_" + configName, "");
    fileContext.WritePushScopeStruct();

    // Using base config
    fileContext.WriteCommand("Using", ".ConfigBase");

    fileContext.WritePopScope();
  }

  // Write out a list of all configs
  fileContext.WriteArray("all_configs", Wrap(configs, ".config_", ""));
}

void cmGlobalFastbuildGenerator::Detail::Generation::WriteTargetDefinitions(
  GenerationContext& context, bool outputGlobals)
{
  context.fc.WriteSectionHeader((outputGlobals) ? "Global Target Definitions"
                                                : "Target Definitions");

  // Now iterate each target in order
  for (OrderedTargets::iterator targetIter = context.orderedTargets.begin();
       targetIter != context.orderedTargets.end(); ++targetIter) {
    const cmGeneratorTarget* constTarget = (*targetIter);
    if (constTarget->GetType() == cmState::INTERFACE_LIBRARY) {
      continue;
    }

    TargetContextMap::iterator findResult =
      context.targetContexts.find(constTarget);
    if (findResult == context.targetContexts.end()) {
      continue;
    }

    cmGeneratorTarget* target = findResult->second.target;
    cmLocalCommonGenerator* lg = findResult->second.lg;

    if (target->GetType() == cmState::GLOBAL_TARGET) {
      if (!outputGlobals)
        continue;
    } else {
      if (outputGlobals)
        continue;
    }

    switch (target->GetType()) {
      case cmState::EXECUTABLE:
      case cmState::SHARED_LIBRARY:
      case cmState::STATIC_LIBRARY:
      case cmState::MODULE_LIBRARY:
      case cmState::OBJECT_LIBRARY: {
        cmFastbuildNormalTargetGenerator targetGenerator(
          target, context.customCommandAliases);
        targetGenerator.Generate();
        break;
      }
      case cmState::UTILITY:
      case cmState::GLOBAL_TARGET:
        // TODO stuff ?
        break;
      default:
        break;
    }
  }
}

void cmGlobalFastbuildGenerator::Detail::Generation::WriteAliases(
  GenerationContext& context, bool outputGlobals)
{
  context.fc.WriteSectionHeader("Aliases");

  // Write the following aliases:
  // Per Target
  // Per Config
  // All

  typedef std::map<std::string, std::vector<std::string> > TargetListMap;
  TargetListMap perConfig;
  TargetListMap perTarget;

  for (OrderedTargets::iterator targetIter = context.orderedTargets.begin();
       targetIter != context.orderedTargets.end(); ++targetIter) {
    const cmGeneratorTarget* constTarget = (*targetIter);
    if (constTarget->GetType() == cmState::INTERFACE_LIBRARY) {
      continue;
    }

    TargetContextMap::iterator findResult =
      context.targetContexts.find(constTarget);
    if (findResult == context.targetContexts.end()) {
      continue;
    }

    cmGeneratorTarget* target = findResult->second.target;
    const std::string& targetName = target->GetName();

    if (target->GetType() == cmState::GLOBAL_TARGET) {
      if (!outputGlobals)
        continue;
    } else {
      if (outputGlobals)
        continue;
    }

    // Define compile flags
    std::vector<std::string> configs;
    context.root->GetMakefile()->GetConfigurations(configs, false);
    for (std::vector<std::string>::const_iterator iter = configs.begin();
         iter != configs.end(); ++iter) {
      const std::string& configName = *iter;
      std::string aliasName = targetName + "-" + configName;

      perTarget[targetName].push_back(aliasName);

      if (!Detection::IsExcludedFromAll(target)) {
        perConfig[configName].push_back(aliasName);
      }
    }
  }

  if (!outputGlobals) {
    context.fc.WriteComment("Per config");
    std::vector<std::string> aliasTargets;
    for (TargetListMap::iterator iter = perConfig.begin();
         iter != perConfig.end(); ++iter) {
      const std::string& configName = iter->first;
      const std::vector<std::string>& targets = iter->second;

      context.fc.WriteCommand("Alias", Quote(configName));
      context.fc.WritePushScope();
      context.fc.WriteArray("Targets", Wrap(targets, "'", "'"));
      context.fc.WritePopScope();

      aliasTargets.clear();
      aliasTargets.push_back(configName);
      context.fc.WriteCommand("Alias", Quote("ALL_BUILD-" + configName));
      context.fc.WritePushScope();
      context.fc.WriteArray("Targets", Wrap(aliasTargets, "'", "'"));
      context.fc.WritePopScope();
    }
  }

  context.fc.WriteComment("Per targets");
  for (TargetListMap::iterator iter = perTarget.begin();
       iter != perTarget.end(); ++iter) {
    const std::string& targetName = iter->first;
    const std::vector<std::string>& targets = iter->second;

    context.fc.WriteCommand("Alias", "'" + targetName + "'");
    context.fc.WritePushScope();
    context.fc.WriteArray("Targets", Wrap(targets, "'", "'"));
    context.fc.WritePopScope();
  }

  if (!outputGlobals) {
    std::vector<std::string> configs;
    context.root->GetMakefile()->GetConfigurations(configs, false);

    context.fc.WriteComment("All");
    context.fc.WriteCommand("Alias", "'All'");
    context.fc.WritePushScope();
    context.fc.WriteArray("Targets", Wrap(configs, "'", "'"));
    context.fc.WritePopScope();
  }
}

//----------------------------------------------------------------------------
cmGlobalGeneratorFactory* cmGlobalFastbuildGenerator::NewFactory()
{
  return new cmGlobalGeneratorSimpleFactory<cmGlobalFastbuildGenerator>();
}

//----------------------------------------------------------------------------
cmGlobalFastbuildGenerator::cmGlobalFastbuildGenerator(cmake* cm)
  : cmGlobalCommonGenerator(cm)
{
#ifdef _WIN32
  cm->GetState()->SetWindowsShell(true);
#endif
  this->FindMakeProgramFile = "CMakeFastbuildFindMake.cmake";
}

//----------------------------------------------------------------------------
cmGlobalFastbuildGenerator::~cmGlobalFastbuildGenerator()
{
}

//----------------------------------------------------------------------------
cmLocalGenerator* cmGlobalFastbuildGenerator::CreateLocalGenerator(
  cmMakefile* makefile)
{
  return new cmLocalFastbuildGenerator(this, makefile);
}

//----------------------------------------------------------------------------
void cmGlobalFastbuildGenerator::EnableLanguage(
  std::vector<std::string> const& lang, cmMakefile* mf, bool optional)
{
  // Create list of configurations requested by user's cache, if any.
  this->cmGlobalGenerator::EnableLanguage(lang, mf, optional);

  if (!mf->GetDefinition("CMAKE_CONFIGURATION_TYPES")) {
    mf->AddCacheDefinition(
      "CMAKE_CONFIGURATION_TYPES", "Debug;Release;MinSizeRel;RelWithDebInfo",
      "Semicolon separated list of supported configuration types, "
      "only supports Debug, Release, MinSizeRel, and RelWithDebInfo, "
      "anything else will be ignored.",
      cmState::STRING);
  }
}

//----------------------------------------------------------------------------
void cmGlobalFastbuildGenerator::Generate()
{
  // Execute the standard generate process
  cmGlobalGenerator::Generate();

  Detail::Generation::GenerateRootBFF(this);
}

//----------------------------------------------------------------------------
void cmGlobalFastbuildGenerator::GenerateBuildCommand(
  std::vector<std::string>& makeCommand, const std::string& makeProgram,
  const std::string& projectName, const std::string& projectDir,
  const std::string& targetName, const std::string& config, bool /*fast*/,
  bool /*verbose*/, std::vector<std::string> const& /*makeOptions*/)
{
  // A build command for fastbuild looks like this:
  // fbuild.exe [make-options] [-config projectName.bff] <target>-<config>

  // Setup make options
  std::vector<std::string> makeOptionsSelected;

  // Select the caller- or user-preferred make program
  std::string makeProgramSelected = this->SelectMakeProgram(makeProgram);

  // Select the target
  std::string targetSelected = targetName;
  // Note an empty target is a valid target (defaults to ALL anyway)
  if (targetSelected == "clean") {
    makeOptionsSelected.push_back("-clean");
    targetSelected = "";
  }

  // Select the config
  std::string configSelected = config;
  if (configSelected.empty()) {
    configSelected = "Debug";
  }

  // Select fastbuild target
  if (targetSelected.empty()) {
    targetSelected = configSelected;
  } else {
    targetSelected += "-" + configSelected;
  }

  // Hunt the fbuild.bff file in the directory above
  std::string configFile;
  if (!cmSystemTools::FileExists(projectDir + "fbuild.bff")) {
    configFile = cmSystemTools::FileExistsInParentDirectories(
      "fbuild.bff", projectDir.c_str(), "");
  }

  // Build the command
  makeCommand.push_back(makeProgramSelected);

  // Push in the make options
  makeCommand.insert(makeCommand.end(), makeOptionsSelected.begin(),
                     makeOptionsSelected.end());

  /*
  makeCommand.push_back("-config");
  makeCommand.push_back(projectName + ".bff");
  */

  makeCommand.push_back("-showcmds");
  makeCommand.push_back("-ide");

  if (!configFile.empty()) {
    makeCommand.push_back("-config");
    makeCommand.push_back(configFile);
  }

  // Add the target-config to the command
  if (!targetSelected.empty()) {
    makeCommand.push_back(targetSelected);
  }
}

//----------------------------------------------------------------------------
void cmGlobalFastbuildGenerator::AppendDirectoryForConfig(
  const std::string& prefix, const std::string& config,
  const std::string& suffix, std::string& dir)
{
  if (!config.empty()) {
    dir += prefix;
    dir += config;
    dir += suffix;
  }
}

//----------------------------------------------------------------------------
void cmGlobalFastbuildGenerator::ComputeTargetObjectDirectory(
  cmGeneratorTarget* gt) const
{
  cmTarget* target = gt->Target;

  // Compute full path to object file directory for this target.
  std::string dir;
  dir += gt->Makefile->GetCurrentBinaryDirectory();
  dir += "/";
  dir += gt->LocalGenerator->GetTargetDirectory(gt);
  dir += "/";
  gt->ObjectDirectory = dir;
}

//----------------------------------------------------------------------------
const char* cmGlobalFastbuildGenerator::GetCMakeCFGIntDir() const
{
  return FASTBUILD_DOLLAR_TAG "ConfigName" FASTBUILD_DOLLAR_TAG;
}

//----------------------------------------------------------------------------
void cmGlobalFastbuildGenerator::GetTargetSets(
  TargetDependSet& projectTargets, TargetDependSet& originalTargets,
  cmLocalGenerator* root, GeneratorVector const& gv)
{
  cmGlobalGenerator::GetTargetSets(projectTargets, originalTargets, root, gv);
}

void cmGlobalFastbuildGenerator::GetDocumentation(cmDocumentationEntry& entry)
{
  entry.Name = cmGlobalFastbuildGenerator::GetActualName();
  entry.Brief = "Generates build.bff files.";
}

//----------------------------------------------------------------------------
