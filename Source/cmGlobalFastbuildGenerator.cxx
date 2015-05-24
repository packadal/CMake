
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
   - Fix cmake build using fastbuild (currently appears configuration incorrect)
   - Running some of the Cmake generation, the pdb files can't be deleted (shows up errors)
   - Depends upon visual studio generator code to sort dependencies
   - When generating CMAKE from scratch, it sometimes errors with fortran complaints and fails generation?
	 a re-run will succeed.
   - Linker for msvc uses the cmake command to callback. Don't think this is an issue since
	 I think compilation is the part that gets distributed anyway.
	 But it might mean that the cache has trouble calculating deps for obj->lib/exe. 
	 Not sure if Fastbuild supports that anyway yet

  Fastbuild bugs:
   - Defining prebuild dependencies that don't exist, causes the error output when that 
	 target is actually defined. Rather than originally complaining that the target 
	 doesn't exist where the reference is attempted.
   - Parsing strings with double $$ doesn't generate a nice error
   - Undocumented that you can escape a $ with ^$

  Limitations:
   - Only tested/working with MSVC

  Current list of unit tests failing:

	83% tests passed, 63 tests failed out of 372
	43 - OutDir (Failed)
	44 - ObjectLibrary (Failed)
	48 - ExternalOBJ (Failed)
	49 - LoadCommand (Failed)
	50 - LinkDirectory (Failed)
	58 - SourceGroups (Failed)
	59 - Preprocess (Failed)
	63 - EmptyLibrary (Failed)
	67 - AliasTarget (Failed)
	69 - InterfaceLibrary (Failed)
	70 - ConfigSources (Failed)
	77 - Module.CheckTypeSize (Failed)
	78 - Module.ExternalData (Failed)
	79 - Module.GenerateExportHeader (Failed)
	101 - SubProject-Stage2 (Failed)
	102 - Framework (Failed)
	103 - TargetName (Failed)
	105 - CustComDepend (Failed)
	108 - CustomCommand (Failed)
	109 - CustomCommandByproducts (Failed)
	110 - EmptyDepends (Failed)
	111 - CustomCommandWorkingDirectory (Failed)
	112 - OutOfSource (Failed)
	113 - BuildDepends (Failed)
	114 - SimpleInstall (Failed)
	115 - SimpleInstall-Stage2 (Failed)
	126 - LoadedCommandOneConfig (Failed)
	127 - complex (Failed)
	128 - complexOneConfig (Failed)
	131 - ExternalProject (Failed)
	132 - ExternalProjectLocal (Failed)
	133 - ExternalProjectUpdateSetup (Failed)
	134 - ExternalProjectUpdate (Failed)
	139 - TutorialStep5 (Failed)
	140 - TutorialStep6 (Failed)
	141 - TutorialStep7 (Failed)
	143 - wrapping (Failed)
	144 - qtwrapping (Failed)
	148 - Dependency (Failed)
	149 - JumpWithLibOut (Failed)
	150 - JumpNoLibOut (Failed)
	151 - Plugin (Failed)
	157 - PrecompiledHeader (Failed)
	158 - ModuleDefinition (Failed)
	160 - MFC (Failed)
	168 - CTest.BuildCommand.ProjectInSubdir (Failed)
	185 - CTestConfig.Script.Debug (Failed)
	186 - CTestConfig.Dashboard.Debug (Failed)
	187 - CTestConfig.Script.MinSizeRel (Failed)
	188 - CTestConfig.Dashboard.MinSizeRel (Failed)
	189 - CTestConfig.Script.Release (Failed)
	190 - CTestConfig.Dashboard.Release (Failed)
	191 - CTestConfig.Script.RelWithDebInfo (Failed)
	192 - CTestConfig.Dashboard.RelWithDebInfo (Failed)
	198 - CMakeCommands.target_compile_options (Failed)
	225 - IncludeDirectories (Failed)
	239 - CMakeOnly.CheckStructHasMember (Failed)
	276 - RunCMake.Configure (Failed)
	281 - RunCMake.GeneratorExpression (Failed)
	290 - RunCMake.CompileFeatures (Failed)
	332 - RunCMake.File_Generate (Failed)
============================================================================*/
#include "cmGlobalFastbuildGenerator.h"

#include "cmGeneratorTarget.h"
#include "cmGlobalGeneratorFactory.h"
#include "cmLocalFastbuildGenerator.h"
#include "cmMakefile.h"
#include "cmSourceFile.h"
#include "cmTarget.h"
#include "cmGeneratedFileStream.h"
#include "cmLocalGenerator.h"
#include "cmComputeLinkInformation.h"
#include "cmGlobalVisualStudioGenerator.h"
#include <cmsys/Encoding.hxx>
#include <assert.h>

static const char fastbuildGeneratorName[] = "Fastbuild";

class cmGlobalFastbuildGenerator::Factory 
	: public cmGlobalGeneratorFactory
{
public:
	Factory()
	{

	}

	cmGlobalGenerator* CreateGlobalGenerator(const std::string& name) const 
	{
		if (name == (fastbuildGeneratorName))
		{
			return new cmGlobalFastbuildGenerator();
		}
		return NULL;
	}

	void GetDocumentation(cmDocumentationEntry& entry) const 
	{
		entry.Name = fastbuildGeneratorName;
		entry.Brief = "Generates fastbuild project files.";
	}

	void GetGenerators(std::vector<std::string>& names) const 
	{
		names.push_back(fastbuildGeneratorName);
	}
};

//----------------------------------------------------------------------------
class cmGlobalFastbuildGenerator::Detail
{
public:
	class FileContext;
	class Definition;
	class Detection;
	class Generation;
};

//----------------------------------------------------------------------------
class cmGlobalFastbuildGenerator::Detail::FileContext
{
public:
	FileContext(cmGeneratedFileStream & afout)
		: fout(afout)
	{
	}

	void WriteComment(const std::string& comment)
	{
		fout << linePrefix << ";" << comment << "\n";
	}

	void WriteBlankLine()
	{
		fout << "\n";
	}

	void WriteHorizontalLine()
	{
		fout <<
			";-------------------------------------------------------------------------------\n";
	}

	void WriteSectionHeader(const char * section)
	{
		fout << "\n";
		WriteHorizontalLine();
		WriteComment(section);
		WriteHorizontalLine();
	}

	void WritePushScope(char begin = '{', char end = '}')
	{
		fout << linePrefix << begin << "\n";
		linePrefix += "\t";
		closingScope += end;
	}

	void WritePushScopeStruct()
	{
		WritePushScope('[', ']');
	}

	void WritePopScope()
	{
		assert(!linePrefix.empty());
		linePrefix.resize(linePrefix.size() - 1);

		fout << linePrefix << closingScope[closingScope.size() - 1] <<
			"\n";

		closingScope.resize(closingScope.size() - 1);
	}

	void WriteVariable(const std::string& key, const std::string& value,
		const std::string& operation = "=")
	{
		fout << linePrefix << "." <<
			key << " " << operation << " " << value << "\n";
	}

	void WriteCommand(const std::string& command, const std::string& value = std::string())
	{
		fout << linePrefix << command;
		if (!value.empty())
		{
			fout << "(" << value << ")";
		}
		fout << "\n";
	}

	void WriteArray(const std::string& key,
		const std::vector<std::string>& values,
		const std::string& prefix, const std::string& suffix,
		const std::string& operation = "=")
	{
		WriteVariable(key, "", operation);
		WritePushScope();
		int size = values.size();
		for (int index = 0; index < size; ++index)
		{
			const std::string & value = values[index];
			bool isLast = index == size - 1;

			fout << linePrefix << prefix << value << suffix;
			if (!isLast)
			{
				fout << ',';
			}
			fout << "\n";
		}
		WritePopScope();
	}

private:
	cmGeneratedFileStream & fout;
	std::string linePrefix;
	std::string closingScope;
};

//----------------------------------------------------------------------------
class cmGlobalFastbuildGenerator::Detail::Detection
{
public:
	static std::string BuildCommandLine(
		const std::vector<std::string> &cmdLines)
	{
		// If we have no commands but we need to build a command anyway, use ":".
		// This happens when building a POST_BUILD value for link targets that
		// don't use POST_BUILD.
		if (cmdLines.empty())
		{
#ifdef _WIN32
			return "cd .";
#else
			return ":";
#endif
		}

		std::ostringstream cmd;
		for (std::vector<std::string>::const_iterator li = cmdLines.begin();
			li != cmdLines.end(); ++li)
#ifdef _WIN32
		{
			if (li != cmdLines.begin())
			{
				cmd << " && ";
			}
			else if (cmdLines.size() > 1)
			{
				cmd << "cmd.exe /C \"";
			}
			cmd << *li;
		}
		if (cmdLines.size() > 1)
		{
			cmd << "\"";
		}
#else
		{
			if (li != cmdLines.begin())
			{
				cmd << " && ";
			}
			cmd << *li;
		}
#endif
		return cmd.str();
	}

	static void DetectConfigurations(cmGlobalFastbuildGenerator * self,
		cmMakefile* mf,
		std::vector<std::string> & configurations)
	{
		// process the configurations
		const char* configList =
			self->CMakeInstance->GetCacheDefinition("CMAKE_CONFIGURATION_TYPES");
		if (configList)
		{
			std::vector<std::string> argsOut;
			cmSystemTools::ExpandListArgument(configList, argsOut);
			for (std::vector<std::string>::iterator iter = argsOut.begin();
				iter != argsOut.end(); ++iter)
			{
				if (std::find(configurations.begin(), configurations.end(), *iter) == configurations.end())
				{
					configurations.push_back(*iter);
				}
			}
		}

		// default to at least Debug and Release
		if (configurations.size() == 0)
		{
			configurations.push_back("Debug");
			configurations.push_back("Release");
		}

		// Reset the entry to have a semi-colon separated list.
		std::string configs = configurations[0];
		for (unsigned int i = 1; i < configurations.size(); ++i)
		{
			configs += ";";
			configs += configurations[i];
		}

		// Add a cache definition
		mf->AddCacheDefinition(
			"CMAKE_CONFIGURATION_TYPES",
			configs.c_str(),
			"Semicolon separated list of supported configuration types, "
			"only supports Debug, Release, MinSizeRel, and RelWithDebInfo, "
			"anything else will be ignored.",
			cmCacheManager::STRING);
	}

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
	};

	static void DetectOutput(
		FastbuildTargetNames & targetNamesOut,
		cmLocalFastbuildGenerator *lg,
		cmTarget &target,
		const std::string & configName)
	{
		if (target.GetType() == cmTarget::EXECUTABLE)
		{
			target.GetExecutableNames(
				targetNamesOut.targetNameOut,
				targetNamesOut.targetNameReal,
				targetNamesOut.targetNameImport,
				targetNamesOut.targetNamePDB,
				configName);
		}
		else
		{
			target.GetLibraryNames(
				targetNamesOut.targetNameOut,
				targetNamesOut.targetNameSO,
				targetNamesOut.targetNameReal,
				targetNamesOut.targetNameImport,
				targetNamesOut.targetNamePDB,
				configName);
		}
		
		if (target.HaveWellDefinedOutputFiles())
		{
			targetNamesOut.targetOutputDir = target.GetDirectory(configName) + "/";

			targetNamesOut.targetOutput = target.GetFullPath(configName);
			targetNamesOut.targetOutputReal = target.GetFullPath(configName,
				/*implib=*/false,
				/*realpath=*/true);
			targetNamesOut.targetOutputImplib = target.GetFullPath(configName,
				/*implib=*/true);
		}
		else
		{
			targetNamesOut.targetOutputDir = target.GetMakefile()->GetStartOutputDirectory();
			if (targetNamesOut.targetOutputDir.empty() || 
				targetNamesOut.targetOutputDir == ".")
			{
				targetNamesOut.targetOutputDir = target.GetName();
			}
			else 
			{
				targetNamesOut.targetOutputDir += "/";
				targetNamesOut.targetOutputDir += target.GetName();
			}
			targetNamesOut.targetOutputDir += "/";
			targetNamesOut.targetOutputDir += configName;
			targetNamesOut.targetOutputDir += "/";

			targetNamesOut.targetOutput = targetNamesOut.targetOutputDir + "/" +
				targetNamesOut.targetNameOut;
			targetNamesOut.targetOutputImplib = targetNamesOut.targetOutputDir + "/" +
				targetNamesOut.targetNameImport;
			targetNamesOut.targetOutputReal = targetNamesOut.targetOutputDir + "/" +
				targetNamesOut.targetNameReal;
		}

		// Make sure all obey the correct slashes
		cmSystemTools::ConvertToOutputSlashes(targetNamesOut.targetOutput);
		cmSystemTools::ConvertToOutputSlashes(targetNamesOut.targetOutputImplib);
		cmSystemTools::ConvertToOutputSlashes(targetNamesOut.targetOutputReal);
		cmSystemTools::ConvertToOutputSlashes(targetNamesOut.targetOutputDir);
	}

	static void ComputeLinkCmds(std::vector<std::string> & linkCmds,
		cmLocalFastbuildGenerator *lg,
		cmTarget &target,
		cmGeneratorTarget *gt,
		std::string configName)
	{
		const std::string& linkLanguage = target.GetLinkerLanguage(configName);
		cmMakefile* mf = lg->GetMakefile();
		{
			std::string linkCmdVar = 
				gt->GetCreateRuleVariable(linkLanguage, configName);
			const char *linkCmd = mf->GetDefinition(linkCmdVar);
			if (linkCmd)
			{
				cmSystemTools::ExpandListArgument(linkCmd, linkCmds);
				return;
			}
		}

		// If the above failed, then lets try this:
		switch (target.GetType()) 
		{
			case cmTarget::STATIC_LIBRARY: 
			{
				// We have archive link commands set. First, delete the existing archive.
				{
					std::string cmakeCommand = lg->ConvertToOutputFormat(
						mf->GetRequiredDefinition("CMAKE_COMMAND"),
						cmLocalGenerator::SHELL);
					linkCmds.push_back(cmakeCommand + " -E remove $TARGET_FILE");
				}
				// TODO: Use ARCHIVE_APPEND for archives over a certain size.
				{
					std::string linkCmdVar = "CMAKE_";
					linkCmdVar += linkLanguage;
					linkCmdVar += "_ARCHIVE_CREATE";
					const char *linkCmd = mf->GetRequiredDefinition(linkCmdVar);
					cmSystemTools::ExpandListArgument(linkCmd, linkCmds);
				}
				{
					std::string linkCmdVar = "CMAKE_";
					linkCmdVar += linkLanguage;
					linkCmdVar += "_ARCHIVE_FINISH";
					const char *linkCmd = mf->GetRequiredDefinition(linkCmdVar);
					cmSystemTools::ExpandListArgument(linkCmd, linkCmds);
				}
				return;
			}
			case cmTarget::SHARED_LIBRARY:
			case cmTarget::MODULE_LIBRARY:
			case cmTarget::EXECUTABLE:
				break;
			default:
				assert(0 && "Unexpected target type");
		}
		return;
	}

	static std::string ComputeDefines(
		cmLocalFastbuildGenerator *lg,
		cmTarget &target,
		const cmSourceFile* source,
		const std::string& configName,
		const std::string& language)
	{
		std::set<std::string> defines;

		// Add the export symbol definition for shared library objects.
		if(const char* exportMacro = target.GetExportMacro())
		{
			lg->AppendDefines(defines, exportMacro);
		}

		// Add preprocessor definitions for this target and configuration.
		lg->AddCompileDefinitions(defines, &target,
			configName, language);

		if (source)
		{
			lg->AppendDefines(defines,
				source->GetProperty("COMPILE_DEFINITIONS"));

			std::string defPropName = "COMPILE_DEFINITIONS_";
			defPropName += cmSystemTools::UpperCase(configName);
			lg->AppendDefines(defines,
				source->GetProperty(defPropName));
		}

		// Add a definition for the configuration name.
		// NOTE: CMAKE_TEST_REQUIREMENT The following was added specifically to 
		// facillitate cmake testing. Doesn't feel right to do this...
		std::string configDefine = "CMAKE_INTDIR=\"";
		configDefine += configName;
		configDefine += "\"";
		lg->AppendDefines(defines, configDefine);

		std::string definesString;
		lg->JoinDefines(defines, definesString,
			language);

		return definesString;
	}

	static bool DetectBaseLinkerCommand(std::string & command,
		cmLocalFastbuildGenerator *lg,
		cmTarget &target,
		cmGeneratorTarget *gt,
		const std::string & configName)
	{
		const std::string& linkLanguage = target.GetLinkerLanguage(configName);
		if (linkLanguage.empty()) {
			cmSystemTools::Error("CMake can not determine linker language for "
				"target: ",
				target.GetName().c_str());
			return false;
		}

		cmTarget::TargetType targetType = target.GetType();

		cmLocalGenerator::RuleVariables vars;
		vars.RuleLauncher = "RULE_LAUNCH_LINK";
		vars.CMTarget = &target;
		vars.Language = linkLanguage.c_str();

		std::string responseFlag;
		vars.Objects = "%1";
		vars.LinkLibraries = "";
		
		vars.ObjectDir = "$TargetOutDir$";
		vars.Target = "%2";

		vars.TargetSOName = "$TargetOutSO$";
		vars.TargetPDB = "$TargetOutDir$$TargetNamePDB$";

		// Setup the target version.
		std::string targetVersionMajor;
		std::string targetVersionMinor;
		{
			std::ostringstream majorStream;
			std::ostringstream minorStream;
			int major;
			int minor;
			target.GetTargetVersion(major, minor);
			majorStream << major;
			minorStream << minor;
			targetVersionMajor = majorStream.str();
			targetVersionMinor = minorStream.str();
		}
		vars.TargetVersionMajor = targetVersionMajor.c_str();
		vars.TargetVersionMinor = targetVersionMinor.c_str();

		vars.Defines = "$CompileDefineFlags$";
		vars.Flags = "$CompileFlags$";
		vars.LinkFlags = "$LinkFlags$";

		// Rule for linking library/executable.
		std::vector<std::string> linkCmds;
		ComputeLinkCmds(linkCmds, lg, target, gt, configName);
		for (std::vector<std::string>::iterator i = linkCmds.begin();
			i != linkCmds.end();
			++i)
		{
			lg->ExpandRuleVariables(*i, vars);
		}
		
		command = BuildCommandLine(linkCmds);

		return true;
	}

	static void SplitExecutableAndFlags(const std::string & command,
		std::string & executable, std::string & options)
	{
		// Remove the command from the front
		std::vector<std::string> args = cmSystemTools::ParseArguments(command.c_str());

		// Join the args together and remove 0 from the front
		std::stringstream argSet;
		std::copy(args.begin() + 1, args.end(), std::ostream_iterator<std::string>(argSet, " "));
		
		executable = args[0];
		options = argSet.str();
	}

	static void DetectBaseCompileCommand(std::string & command,
		cmLocalFastbuildGenerator *lg,
		cmTarget &target,
		const std::string & language)
	{
		cmLocalGenerator::RuleVariables compileObjectVars;
		compileObjectVars.CMTarget = &target;
		compileObjectVars.Language = language.c_str();
		compileObjectVars.Source = "%1";
		compileObjectVars.Object = "%2";
		compileObjectVars.ObjectDir = "$TargetOutputDir$";
		compileObjectVars.ObjectFileDir = "";
		compileObjectVars.Flags = "";
		compileObjectVars.Defines = "";
		compileObjectVars.TargetCompilePDB = "$TargetNamePDB$";

		// Rule for compiling object file.
		std::string compileCmdVar = "CMAKE_";
		compileCmdVar += language;
		compileCmdVar += "_COMPILE_OBJECT";
		std::string compileCmd = lg->GetMakefile()->GetRequiredDefinition(compileCmdVar);
		std::vector<std::string> compileCmds;
		cmSystemTools::ExpandListArgument(compileCmd, compileCmds);

		for (std::vector<std::string>::iterator i = compileCmds.begin();
			i != compileCmds.end(); ++i)
		{
			std::string & compileCmd = *i;
			lg->ExpandRuleVariables(compileCmd, compileObjectVars);
		}

		command = BuildCommandLine(compileCmds);
	}

	static std::string DetectCompileRule(cmLocalFastbuildGenerator *lg,
		cmTarget &target,
		const std::string& lang)
	{
		cmLocalGenerator::RuleVariables vars;
		vars.RuleLauncher = "RULE_LAUNCH_COMPILE";
		vars.CMTarget = &target;
		vars.Language = lang.c_str();
		vars.Source = "$in";
		vars.Object = "$out";
		vars.Defines = "$DEFINES";
		vars.TargetPDB = "$TARGET_PDB";
		vars.TargetCompilePDB = "$TARGET_COMPILE_PDB";
		vars.ObjectDir = "$OBJECT_DIR";
		vars.ObjectFileDir = "$OBJECT_FILE_DIR";
		vars.Flags = "$FLAGS";

		cmMakefile* mf = lg->GetMakefile();

		// Rule for compiling object file.
		const std::string cmdVar = std::string("CMAKE_") + lang + "_COMPILE_OBJECT";
		std::string compileCmd = mf->GetRequiredDefinition(cmdVar);
		std::vector<std::string> compileCmds;
		cmSystemTools::ExpandListArgument(compileCmd, compileCmds);

		for (std::vector<std::string>::iterator i = compileCmds.begin();
			i != compileCmds.end(); ++i)
		{
			lg->ExpandRuleVariables(*i, vars);
		}

		std::string cmdLine =
			BuildCommandLine(compileCmds);

		return cmdLine;
	}

	static void DetectLanguages(std::set<std::string> & languages,
		cmGlobalFastbuildGenerator * self,
		cmLocalFastbuildGenerator *lg,
		cmTarget &target)
	{
		cmGeneratorTarget *gt = self->GetGeneratorTarget(&target);

		for (std::vector<std::string>::iterator iter = self->Configurations.begin();
			iter != self->Configurations.end(); ++iter)
		{
			std::string & configName = *iter;

			std::vector<cmSourceFile*> sourceFiles;
			gt->GetSourceFiles(sourceFiles, configName);
			for (std::vector<cmSourceFile*>::const_iterator
				i = sourceFiles.begin(); i != sourceFiles.end(); ++i)
			{
				const std::string& lang = (*i)->GetLanguage();
				if (!lang.empty())
				{
					languages.insert(lang);
				}
			}
		}
	}

	static void FilterSourceFiles(std::vector<cmSourceFile const*> & filteredSourceFiles,
		std::vector<cmSourceFile const*> & sourceFiles, const std::string & language)
	{
		for (std::vector<cmSourceFile const*>::const_iterator
			i = sourceFiles.begin(); i != sourceFiles.end(); ++i)
		{
			const cmSourceFile* sf = *i;
			if (sf->GetLanguage() == language)
			{
				filteredSourceFiles.push_back(sf);
			}
		}
	}

	static void DetectCompilerFlags(std::string & compileFlags,
		cmLocalFastbuildGenerator *lg,
		cmGeneratorTarget *gt,
		cmTarget &target,
		const cmSourceFile* source,
		const std::string& language,
		const std::string& configName)
	{
		lg->AddLanguageFlags(compileFlags, 
			language, 
			configName);

		lg->AddArchitectureFlags(compileFlags,
			gt,
			language,
			configName);

		// Add shared-library flags if needed.
		lg->AddCMP0018Flags(compileFlags, 
			&target,
			language,
			configName);

		lg->AddVisibilityPresetFlags(compileFlags, &target,
			language);

		std::vector<std::string> includes;
		lg->GetIncludeDirectories(includes,
			gt,
			language,
			configName);

		// Add include directory flags.
		std::string includeFlags = lg->GetIncludeFlags(includes, gt,
			language,
			language == "RC" ? true : false,  // full include paths for RC
			// needed by cmcldeps
			false,
			configName);
		
		lg->AppendFlags(compileFlags, includeFlags);

		// Append old-style preprocessor definition flags.
		lg->AppendFlags(compileFlags,
			lg->GetMakefile()->GetDefineFlags());

		// Add target-specific flags.
		lg->AddCompileOptions(compileFlags, 
			&target,
			language,
			configName);

		if (source)
		{
			lg->AppendFlags(compileFlags, source->GetProperty("COMPILE_FLAGS"));
		}
	}

	static void DetectTargetCompileDependencies(
		cmGlobalFastbuildGenerator* gg,
		cmTarget& target, 
		std::vector<std::string>& dependencies)
	{
		if (target.GetType() == cmTarget::GLOBAL_TARGET)
		{
			// Global targets only depend on other utilities, which may not appear in
			// the TargetDepends set (e.g. "all").
			std::set<std::string> const& utils = target.GetUtilities();
			std::copy(utils.begin(), utils.end(), std::back_inserter(dependencies));
		}
		else 
		{
			cmTargetDependSet const& targetDeps =
				gg->GetTargetDirectDepends(target);
			for (cmTargetDependSet::const_iterator i = targetDeps.begin();
				i != targetDeps.end(); ++i)
			{
				const cmTargetDepend& depTarget = *i;
				if (depTarget->GetType() == cmTarget::INTERFACE_LIBRARY)
				{
					continue;
				}
				dependencies.push_back(depTarget->GetName());
			}
		}
	}

	static void DetectTargetLinkDependencies(
		cmGlobalFastbuildGenerator* gg,
		cmTarget& target, 
		const std::string& configName,
		std::vector<std::string>& dependencies)
	{
		// Static libraries never depend on other targets for linking.
		if (target.GetType() == cmTarget::STATIC_LIBRARY ||
			target.GetType() == cmTarget::OBJECT_LIBRARY)
		{
			return;
		}

		cmComputeLinkInformation* cli =
			target.GetLinkInformation(configName);
		if(!cli)
		{
			return;
		}

		const std::vector<std::string> &deps = cli->GetDepends();
		std::copy(deps.begin(), deps.end(), std::back_inserter(dependencies));
	}

	static std::string DetectTargetCompileOutputDir(
		cmLocalFastbuildGenerator* lg,
		const cmTarget& target)
	{
		std::string result = lg->GetTargetDirectory(target) + "/";
		cmSystemTools::ConvertToOutputSlashes(result);
		return result;
	}

	static void DetectTargetObjectDependencies(
		cmGlobalFastbuildGenerator* gg,
		cmTarget& target, 
		const std::string& configName,
		std::vector<std::string>& dependencies)
	{
		// Iterate over all source files and look for 
		// object file dependencies
		cmGeneratorTarget *gt = gg->GetGeneratorTarget(&target);

		std::set<std::string> objectLibs;

		std::vector<cmSourceFile*> sourceFiles;
		gt->GetSourceFiles(sourceFiles, configName);
		for (std::vector<cmSourceFile*>::const_iterator
			i = sourceFiles.begin(); i != sourceFiles.end(); ++i)
		{
			const std::string& objectLib = (*i)->GetObjectLibrary();
			if (!objectLib.empty())
			{
				// Find the target this actually is (might be an alias)
				const cmTarget* objectTarget = gg->FindTarget(objectLib);
				if (objectTarget)
				{
					objectLibs.insert(objectTarget->GetName() + "-" + configName);
				}
			}
		}

		std::copy(objectLibs.begin(), objectLibs.end(),
			std::back_inserter(dependencies) );
	}

	struct TargetCompare
	{
		bool operator()(cmTarget const* l, cmTarget const* r) const
		{
			// Make sure ALL_BUILD is first so it is the default active project.
			if(r->GetName() == "ALL_BUILD")
			{
				return false;
			}
			if(l->GetName() == "ALL_BUILD")
			{
				return true;
			}

			// Now look to see if either depends on the other recursively
			

			// Fall back to ordering by name
			return strcmp(l->GetName().c_str(), r->GetName().c_str()) < 0;
		}
	};

	typedef std::vector<const cmTarget*> OrderedTargetSet;

	static void ComputeTargetOrderAndDependencies(
		cmGlobalFastbuildGenerator* gg,
		OrderedTargetSet& orderedTargets)
	{
		TargetDependSet projectTargets;
		TargetDependSet originalTargets;
		std::map<std::string, std::vector<cmLocalGenerator*> >::iterator it;
		for(it = gg->ProjectMap.begin(); it!= gg->ProjectMap.end(); ++it)
		{
			std::vector<cmLocalGenerator*>& generators = it->second;
			cmLocalFastbuildGenerator* root =
				static_cast<cmLocalFastbuildGenerator*>(generators[0]);
			
			// Given this information, calculate the dependencies:
			// Collect all targets under this root generator and the transitive
			// closure of their dependencies.
			
			gg->GetTargetSets(projectTargets, originalTargets, root, generators);
		}

		// Iterate over the targets and export their order
		for (TargetDependSet::iterator iter = projectTargets.begin();
			iter != projectTargets.end();
			++iter)
		{
			const cmTargetDepend& targetDepend = *iter;
			const cmTarget& target = *targetDepend;

			orderedTargets.push_back(&target);
		}

		struct SortByDependency
		{
			SortByDependency(cmGlobalFastbuildGenerator* agg)
				: gg(agg)
			{}

			cmGlobalFastbuildGenerator* gg;
			bool operator()(const cmTarget* l, const cmTarget* r) const
			{
				// Test for dependencies
				if (aHasDependencyOnB(r, l))
				{
					return true;
				}

				return false;
			}

			bool aHasDependencyOnB(const cmTarget* candidate, const cmTarget* test) const
			{
				TargetDependSet const& ts = gg->GetTargetDirectDepends(*candidate);
				if (ts.find(test) != ts.end())
				{
					return true;
				}

				for(TargetDependSet::const_iterator i = ts.begin(); i != ts.end(); ++i)
				{
					const cmTarget * dtarget = *i;
					if (aHasDependencyOnB(dtarget, test))
					{
						return true;
					}
				}

				return false;
			}
		};

		// Now I have an array of all targets.
		// Attempt to sort it
		std::_Insertion_sort(orderedTargets.begin(), orderedTargets.end(), 
			SortByDependency(gg));

		// Validate
		SortByDependency lessThan(gg);
		std::vector<const cmTarget*> newOt;

		// Build the reverse graph, 
		// each target, and the set of things that depend upon it
		typedef std::map<const cmTarget*, std::vector<const cmTarget*>> DepMap;
		DepMap forwardDeps;
		DepMap reverseDeps;
		for (int i = 0; i < ((int)orderedTargets.size()); ++i)
		{
			const cmTarget* target = orderedTargets[i];
			std::vector<const cmTarget*>& fwdDeps = forwardDeps[target];

			TargetDependSet const& ts = gg->GetTargetDirectDepends(*target);
			for(TargetDependSet::const_iterator iter = ts.begin(); iter != ts.end(); ++iter)
			{
				const cmTarget * dtarget = *iter;
				fwdDeps.push_back(dtarget);
				reverseDeps[dtarget].push_back(target);
			}
		}
		orderedTargets.clear();

		// Now iterate over each target with its list of dependencies.
		// And dump out ones that have 0 dependencies.
		bool written = true;
		while (!forwardDeps.empty() && written)
		{
			written = false;
			for (DepMap::iterator iter = forwardDeps.begin();
				iter != forwardDeps.end(); ++iter)
			{
				std::vector<const cmTarget*>& fwdDeps = iter->second;
				const cmTarget* target = iter->first;
				if (!fwdDeps.empty())
				{
					// Looking for empty dependency lists.
					// Those are the next to be written out
					continue;
				}

				// dependency list is empty,
				// add it to the output list
				written = true;
				orderedTargets.push_back(target);

				// Use reverse dependencies to determine 
				// what forward dep lists to adjust
				std::vector<const cmTarget*>& revDeps = reverseDeps[target];
				for (unsigned int i = 0; i < revDeps.size(); ++i)
				{
					const cmTarget* revDep = revDeps[i];
					
					// Fetch the list of deps on that target
					std::vector<const cmTarget*>& revDepFwdDeps = 
						forwardDeps[revDep];
					// remove the one we just added from the list
					revDepFwdDeps.erase(
						std::remove(revDepFwdDeps.begin(), revDepFwdDeps.end(), target),
						revDepFwdDeps.end());
				}

				// Remove it from forward deps so not
				// considered again
				forwardDeps.erase(target);

				// Must break now as we've invalidated
				// our forward deps iterator
				break;
			}
		}
		// Make sure we managed to find a place
		// to insert every dependency.
		// If this fires, then there is most likely
		// a cycle in the graph...
		assert(forwardDeps.empty());

		for (int i = 0; i < ((int)orderedTargets.size() - 1); ++i)
		{
			for (int j = i + 1; j < ((int)orderedTargets.size()); ++j)
			{
				// i does not need to be < j.
				// but j must not be less than i
				assert(!lessThan(orderedTargets[j], orderedTargets[i]));
			}
		}
	}

	static void StripNestedGlobalTargets( OrderedTargetSet& orderedTargets )
	{
		// Iterate over all targets and remove the ones that are 
		// not needed for generation.
		// i.e. the nested global targets
		struct RemovalTest
		{
			bool operator()(const cmTarget* target) const
			{
				if (target->GetType() == cmTarget::UTILITY ||
					target->GetType() == cmTarget::GLOBAL_TARGET)
				{
					// Temporarily disable all utilities and global targets.
					// no good way to define dependancies for these things
					// in fbuild yet. And they're not 100% necessary
					return true;
				}

				if (target->GetType() == cmTarget::GLOBAL_TARGET)
				{
					// We only want to process global targets that live in the home
					// (i.e. top-level) directory.  CMake creates copies of these targets
					// in every directory, which we don't need.
					cmMakefile *mf = target->GetMakefile();
					if (strcmp(mf->GetStartDirectory(), mf->GetHomeDirectory()) != 0)
					{
						return true;
					}
				}
				return false;
			}
		};

		orderedTargets.erase(
			std::remove_if(orderedTargets.begin(), orderedTargets.end(), RemovalTest()),
			orderedTargets.end());
	}

private:

};

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
class cmGlobalFastbuildGenerator::Detail::Generation
{
public:
	struct TargetGenerationContext
	{
		cmTarget* target;
		cmLocalFastbuildGenerator* root;
		std::vector<cmLocalGenerator*> generators;
		cmLocalFastbuildGenerator* lg;
	};
	typedef std::map<const cmTarget*, TargetGenerationContext> TargetContextMap;
	typedef Detection::OrderedTargetSet OrderedTargets;

	struct GenerationContext
	{
		cmGlobalFastbuildGenerator * self;
		cmLocalFastbuildGenerator* root;
		FileContext& fc;
		OrderedTargets orderedTargets;
		TargetContextMap targetContexts;
	};

	static std::string Quote(const std::string& str, const std::string& quotation = "'")
	{
		return quotation + str + quotation;
	}

	static std::string EncodeLiteral(const std::string &lit)
	{
		std::string result = lit;
		cmSystemTools::ReplaceString(result, "$", "^$");
		return result;
	}

	static void EnsureDirectoryExists(const std::string& path,
		GenerationContext& context)
	{
		if (cmSystemTools::FileIsFullPath(path.c_str()))
		{
			cmSystemTools::MakeDirectory(path.c_str());
		}
		else
		{
			const std::string fullPath = std::string(
				context.self->GetCMakeInstance()->GetHomeOutputDirectory())
				+ "/" + path;
			cmSystemTools::MakeDirectory(fullPath.c_str());
		}
	}
	
	static void BuildTargetContexts(cmGlobalFastbuildGenerator * gg,
		TargetContextMap& map)
	{
		std::map<std::string, std::vector<cmLocalGenerator*> >::iterator it;
		for(it = gg->ProjectMap.begin(); it!= gg->ProjectMap.end(); ++it)
		{
			std::vector<cmLocalGenerator*>& generators = it->second;
			cmLocalFastbuildGenerator* root =
				static_cast<cmLocalFastbuildGenerator*>(generators[0]);

			// Build a map of all targets to their local generator
			for (std::vector<cmLocalGenerator*>::iterator iter = generators.begin();
				iter != generators.end(); ++iter)
			{
				cmLocalFastbuildGenerator *lg = static_cast<cmLocalFastbuildGenerator*>(*iter);

				if(gg->IsExcluded(root, lg))
				{
					continue;
				}

				cmTargets &tgts = lg->GetMakefile()->GetTargets();
				for (cmTargets::iterator targetIter = tgts.begin(); 
					targetIter != tgts.end();
					++targetIter)
				{
					cmTarget &target = (targetIter->second);

					if(gg->IsRootOnlyTarget(&target) &&
						target.GetMakefile() != root->GetMakefile())
					{
						continue;
					}

					TargetGenerationContext targetContext =
						{ &target, root, generators, lg };
					map[&target] = targetContext;
				}
			}
		}
	}

	static void GenerateRootBFF(cmGlobalFastbuildGenerator * self)
	{

		cmLocalFastbuildGenerator* root = 
			static_cast<cmLocalFastbuildGenerator*>(self->GetLocalGenerators()[0]);

		// Calculate filename
		std::string fname = root->GetMakefile()->GetStartOutputDirectory();
		fname += "/";
		//fname += root->GetMakefile()->GetProjectName();
		fname += "fbuild";
		fname += ".bff";
		
		// Open file
		cmGeneratedFileStream fout(fname.c_str());
		fout.SetCopyIfDifferent(true);
		if(!fout)
		{
			return;
		}

		FileContext fc(fout);
		GenerationContext context =
			{ self, root, fc };
		Detection::ComputeTargetOrderAndDependencies( context.self, context.orderedTargets );
		Detection::StripNestedGlobalTargets( context.orderedTargets );
		BuildTargetContexts( context.self, context.targetContexts );
		WriteRootBFF(context);
		
		// Close file
		if (fout.Close())
		{
			self->FileReplacedDuringGenerate(fname);
		}
	}

	static void WriteRootBFF(GenerationContext& context)
	{
		context.fc.WriteSectionHeader("Fastbuild makefile - Generated using CMAKE");

		WriteSettings( context );
		WriteCompilers( context );
		WriteConfigurations( context );

		// Sort targets

		WriteTargetDefinitions( context );
		WriteAliases( context );
	}

	static void WriteSettings( GenerationContext& context )
	{
		context.fc.WriteSectionHeader("Settings");

		context.fc.WriteCommand("Settings");
		context.fc.WritePushScope();

		std::string cacheDir =
			context.self->GetCMakeInstance()->GetHomeOutputDirectory();
		cacheDir += "\\.fbuild.cache";
		cmSystemTools::ConvertToOutputSlashes(cacheDir);

		context.fc.WriteVariable("CachePath", Quote(cacheDir));
		context.fc.WritePopScope();
	}

	static bool WriteCompilers( GenerationContext& context )
	{
		cmMakefile *mf = context.root->GetMakefile();

		context.fc.WriteSectionHeader("Compilers");

		// Detect each language used in the definitions
		std::set<std::string> languages;
		for (TargetContextMap::iterator iter = context.targetContexts.begin();
			iter != context.targetContexts.end(); ++iter)
		{
			TargetGenerationContext& targetContext = iter->second;

			if (targetContext.target->GetType() == cmTarget::INTERFACE_LIBRARY)
			{
				continue;
			}

			Detection::DetectLanguages(languages, context.self,
				targetContext.lg, *targetContext.target);
		}

		// Now output a compiler for each of these languages
		typedef std::map<std::string, std::string> StringMap;
		StringMap compilerToCompilerName;
		StringMap languageToCompiler;
		for (std::set<std::string>::iterator iter = languages.begin();
			iter != languages.end();
			++iter)
		{
			const std::string & language = *iter;

			// Calculate the root location of the compiler
			std::string variableString = "CMAKE_"+language+"_COMPILER";
			std::string compilerLocation = mf->GetSafeDefinition(variableString);
			if (compilerLocation.empty())
			{
				return false;
			}

			// Add the language to the compiler's name
			std::string& compilerName = compilerToCompilerName[compilerLocation];
			if (compilerName.empty())
			{
				compilerName = "Compiler";
			}
			compilerName += "-";
			compilerName += language;

			// Now add the language to point to that compiler location
			languageToCompiler[language] = compilerLocation;
		}

		// Now output all the compilers
		for (StringMap::iterator iter = compilerToCompilerName.begin();
			iter != compilerToCompilerName.end();
			++iter)
		{
			const std::string& compilerLocation = iter->first;
			const std::string& compilerName = iter->second;

			// Strip out the path to the compiler
			std::string compilerPath = 
				cmSystemTools::GetFilenamePath( compilerLocation );
			std::string compilerFile = "$CompilerRoot$\\" +
				cmSystemTools::GetFilenameName( compilerLocation );

			cmSystemTools::ConvertToOutputSlashes( compilerPath );
			cmSystemTools::ConvertToOutputSlashes( compilerFile );

			// Write out the compiler that has been configured
			context.fc.WriteCommand("Compiler", Quote(compilerName));
			context.fc.WritePushScope();
			context.fc.WriteVariable("CompilerRoot", Quote(compilerPath));
			context.fc.WriteVariable("Executable", Quote(compilerFile));
			context.fc.WritePopScope();
		}

		// Now output the compiler names according to language as variables
		for (StringMap::iterator iter = languageToCompiler.begin();
			iter != languageToCompiler.end();
			++iter)
		{
			const std::string& language = iter->first;
			const std::string& compilerLocation = iter->second;
			const std::string& compilerName = compilerToCompilerName[compilerLocation];

			// Output a default compiler to absorb the library requirements for a compiler
			if (iter == languageToCompiler.begin())
			{
				context.fc.WriteVariable("Compiler_dummy", Quote(compilerName));
			}

			context.fc.WriteVariable("Compiler_"+language, Quote(compilerName));
		}
		
		return true;
	}
	
	static void WriteConfigurations(GenerationContext& context)
	{
		context.fc.WriteSectionHeader("Configurations");

		context.fc.WriteVariable("ConfigBase", "");
		context.fc.WritePushScopeStruct();
		context.fc.WritePopScope();

		// Iterate over all configurations and define them:
		for (std::vector<std::string>::iterator iter = context.self->Configurations.begin();
			iter != context.self->Configurations.end(); ++iter)
		{
			std::string & configName = *iter;
			context.fc.WriteVariable("config_" + configName, "");
			context.fc.WritePushScopeStruct();

			// Using base config
			context.fc.WriteCommand("Using", ".ConfigBase");

			context.fc.WritePopScope();
		}

		// Write out a list of all configs
		context.fc.WriteArray("all_configs", context.self->Configurations,
			".config_", "");
	}

	static void WriteTargetDefinition(GenerationContext& context,
		cmLocalFastbuildGenerator *lg, cmTarget &target)
	{
		// Detection of the link command as follows:
		std::string linkCommand = "Library";
		switch (target.GetType())
		{
			case cmTarget::INTERFACE_LIBRARY:
				// We don't write out interface libraries.
				return;
			case cmTarget::EXECUTABLE:
			{
				linkCommand = "Executable";
				break;
			}
			case cmTarget::SHARED_LIBRARY:
			{
				linkCommand = "DLL";
				break;
			}
			case cmTarget::STATIC_LIBRARY:
			case cmTarget::MODULE_LIBRARY:
			case cmTarget::OBJECT_LIBRARY:
			{
				// No changes required
				break;
			}
			case cmTarget::UTILITY:
			case cmTarget::GLOBAL_TARGET:
			case cmTarget::UNKNOWN_LIBRARY:
			{
				// Ignoring this target generation...
				// Still generate a valid target by the name,
				// but don't make it do anything
				linkCommand = "Alias";
				return;
			}
		}

		std::string targetName = target.GetName();
		cmGeneratorTarget *gt = context.self->GetGeneratorTarget(&target);

		context.fc.WriteComment("Target definition: "+targetName);
		context.fc.WritePushScope();

		// Iterate over each configuration
		// This time to define linker settings for each config
		for (std::vector<std::string>::iterator iter = context.self->Configurations.begin();
			iter != context.self->Configurations.end(); ++iter)
		{
			std::string configName = *iter;

			context.fc.WriteVariable("BaseConfig_" + configName, "");
			context.fc.WritePushScopeStruct();

			context.fc.WriteCommand("Using", ".ConfigBase");

			context.fc.WriteVariable("ConfigName", Quote(configName));

			context.fc.WriteBlankLine();
			context.fc.WriteComment("General output details:");
			// Write out the output paths for the outcome of this target
			{
				Detection::FastbuildTargetNames targetNames;
				Detection::DetectOutput(targetNames, lg, target, configName);

				context.fc.WriteVariable("TargetNameOut", Quote(targetNames.targetNameOut));
				context.fc.WriteVariable("TargetNameImport", Quote(targetNames.targetNameImport));
				context.fc.WriteVariable("TargetNamePDB", Quote(targetNames.targetNamePDB));
				context.fc.WriteVariable("TargetNameSO", Quote(targetNames.targetNameSO));
				context.fc.WriteVariable("TargetNameReal", Quote(targetNames.targetNameReal));

				// TODO: Remove this if these variables aren't used... 
				// They've been added for testing
				context.fc.WriteVariable("TargetOutput", Quote(targetNames.targetOutput));
				context.fc.WriteVariable("TargetOutputImplib", Quote(targetNames.targetOutputImplib));
				context.fc.WriteVariable("TargetOutputReal", Quote(targetNames.targetOutputReal));
				context.fc.WriteVariable("TargetOutDir", Quote(targetNames.targetOutputDir));

				if (target.GetType() != cmTarget::OBJECT_LIBRARY)
				{
					// on Windows the output dir is already needed at compile time
					// ensure the directory exists (OutDir test)
					EnsureDirectoryExists(targetNames.targetOutputDir, context);
				}
			}

			// Write the dependency list in here too
			// So all dependant libraries are built before this one is
			// This is incase this library depends on code generated from previous ones
			{
				std::vector<std::string> dependencies;
				Detection::DetectTargetCompileDependencies( context.self, target, dependencies );

				context.fc.WriteArray("PreBuildDependencies", dependencies,
					"'", "-"+configName+"'");
			}

			context.fc.WritePopScope();
		}

		// Figure out the list of languages in use by this object
		std::vector<std::string> objectGroups;
		std::set<std::string> languages;
		Detection::DetectLanguages(languages, context.self, lg, target);

		// Write the object list definitions for each language
		// stored in this target
		for (std::set<std::string>::iterator langIter = languages.begin();
			langIter != languages.end(); ++langIter)
		{
			const std::string & objectGroupLanguage = *langIter;
			std::string ruleObjectGroupName = "ObjectGroup_" + objectGroupLanguage;
			objectGroups.push_back(ruleObjectGroupName);

			context.fc.WriteVariable(ruleObjectGroupName, "");
			context.fc.WritePushScopeStruct();

			// Iterating over all configurations
			for (std::vector<std::string>::iterator iter = context.self->Configurations.begin();
				iter != context.self->Configurations.end(); ++iter)
			{
				std::string configName = *iter;
				context.fc.WriteVariable("ObjectConfig_" + configName, "");
				context.fc.WritePushScopeStruct();

				context.fc.WriteCommand("Using", ".BaseConfig_" + configName);

				struct CompileCommand
				{
					std::string defines;
					std::string flags;
					std::vector<std::string> sourceFiles;
				};
				std::map<std::string,CompileCommand> commandPermutations; 

				// Source files
				context.fc.WriteBlankLine();
				context.fc.WriteComment("Source files:");
				{
					// get a list of source files
					std::vector<cmSourceFile const*> objectSources;
					gt->GetObjectSources(objectSources, configName);

					std::vector<cmSourceFile const*> filteredObjectSources;
					Detection::FilterSourceFiles(filteredObjectSources, objectSources,
						objectGroupLanguage);

					for (std::vector<cmSourceFile const*>::iterator sourceIter = filteredObjectSources.begin();
						sourceIter != filteredObjectSources.end(); ++sourceIter)
					{
						cmSourceFile const *srcFile = *sourceIter;
						std::string sourceFile = srcFile->GetFullPath();

						// Detect flags and defines
						std::string compilerFlags;
						Detection::DetectCompilerFlags(compilerFlags, 
							lg, gt, target, srcFile, objectGroupLanguage, configName);
						std::string compileDefines = 
							Detection::ComputeDefines(lg, target, srcFile, configName, objectGroupLanguage);
						
						std::string configKey = compilerFlags + "{|}" + compileDefines;
						CompileCommand& command = commandPermutations[configKey];
						command.sourceFiles.push_back(sourceFile);
						command.flags = compilerFlags;
						command.defines = compileDefines;
					}
				}

				context.fc.WriteBlankLine();
				context.fc.WriteComment("Compiler options:");
				{
					// Tie together the variables
					std::string targetCompileOutDirectory = 
						Detection::DetectTargetCompileOutputDir(lg, target);
					context.fc.WriteVariable("CompilerOutputPath", Quote(targetCompileOutDirectory));

					std::string compileObjectCmd = Detection::DetectCompileRule(lg, target, objectGroupLanguage);
					//context.fc.WriteVariable("CompilerRuleCmd", Quote( compileObjectCmd ));
				}

				// Compiler options
				{
					// Remove the command from the front and leave the flags behind
					std::string compileCmd;
					Detection::DetectBaseCompileCommand(compileCmd,
						lg, target, objectGroupLanguage);

					std::string executable;
					std::string flags;
					Detection::SplitExecutableAndFlags(compileCmd, executable, flags);

					context.fc.WriteVariable("CompilerCmdBaseFlags", Quote(flags));

					std::string compilerName = ".Compiler_" + objectGroupLanguage;
					context.fc.WriteVariable("Compiler", compilerName);
				}

				// Iterate over all subObjectGroups
				std::string objectGroupRuleName = targetName + "-" + ruleObjectGroupName + "-" + configName;
				std::vector<std::string> configObjectGroups;
				int groupNameCount = 1;
				for (std::map<std::string, CompileCommand>::iterator groupIter = commandPermutations.begin();
					groupIter != commandPermutations.end();
					++groupIter)
				{
					const CompileCommand& command = groupIter->second;
					std::stringstream ruleName;
					ruleName << objectGroupRuleName << "-" << (groupNameCount++);
					configObjectGroups.push_back(ruleName.str());

					context.fc.WriteCommand("ObjectList", Quote(ruleName.str()));
					context.fc.WritePushScope();

					context.fc.WriteArray("CompilerInputFiles", command.sourceFiles, "'", "'");

					// Unity source files:
					context.fc.WriteVariable("UnityInputFiles", ".CompilerInputFiles");

					context.fc.WriteVariable("CompileDefineFlags", Quote( command.defines ));
					context.fc.WriteVariable("CompileFlags", Quote( command.flags ));
					context.fc.WriteVariable("CompilerOptions", Quote("$CompileFlags$ $CompileDefineFlags$ $CompilerCmdBaseFlags$"));

					context.fc.WritePopScope();

				}

				// Write an alias for this object group to group them all together
				context.fc.WriteCommand("Alias", Quote(objectGroupRuleName));
				context.fc.WritePushScope();
				context.fc.WriteArray("Targets", configObjectGroups, "'", "'");
				context.fc.WritePopScope();

				context.fc.WritePopScope();
			}
			context.fc.WritePopScope();
		}

		// Object libraries do not have linker stages
		bool hasLinkerStage = 
			target.GetType() != cmTarget::OBJECT_LIBRARY;

		// Iterate over each configuration
		// This time to define linker settings for each config
		for (std::vector<std::string>::iterator iter = context.self->Configurations.begin();
			iter != context.self->Configurations.end(); ++iter)
		{
			std::string configName = *iter;

			std::string linkRuleName = targetName + "-" + configName + "-link";

			if (hasLinkerStage)
			{

				context.fc.WriteVariable("LinkerConfig_" + configName, "");
				context.fc.WritePushScopeStruct();

				context.fc.WriteCommand("Using", ".BaseConfig_" + configName);

				context.fc.WriteBlankLine();
				context.fc.WriteComment("Linker options:");
				// Linker options
				{
					std::string linkLibs;
					std::string targetFlags;
					std::string linkFlags;
					std::string frameworkPath;
					std::string linkPath;

					lg->GetTargetFlags(
						linkLibs,
						targetFlags,
						linkFlags,
						frameworkPath,
						linkPath,
						gt,
						false);

					context.fc.WriteVariable("LinkLibs", "'" + linkLibs + "'");
					context.fc.WriteVariable("LinkFlags", "'" + linkFlags + "'");

					// Remove the command from the front and leave the flags behind
					std::string linkCmd;
					if (!Detection::DetectBaseLinkerCommand(linkCmd,
						lg, target, gt, configName))
					{
						return;
					}

					std::string executable;
					std::string flags;
					Detection::SplitExecutableAndFlags(linkCmd, executable, flags);

					context.fc.WriteVariable("Linker", Quote(executable));
					context.fc.WriteVariable("BaseLinkerOptions", Quote(flags));

					context.fc.WriteVariable("LinkerOutput", "'$TargetOutput$'");
					context.fc.WriteVariable("LinkerOptions", "'$BaseLinkerOptions$ $LinkLibs$'");

					context.fc.WriteArray("Libraries", objectGroups, "'" + targetName + "-", "-" + configName + "'");

					// Now detect the extra dependencies for linking
					{
						std::vector<std::string> dependencies;
						Detection::DetectTargetLinkDependencies( context.self, target, configName, dependencies );
						Detection::DetectTargetObjectDependencies( context.self, target, configName, dependencies );

						context.fc.WriteArray("Libraries", dependencies,
							"'", "'", "+");
					}
				
					context.fc.WriteCommand(linkCommand, Quote(linkRuleName));
					context.fc.WritePushScope();
					if (linkCommand == "Library")
					{
						context.fc.WriteComment("Convert the linker options to work with libraries");

						// Push dummy definitions for compilation variables
						// These variables are required by the Library command
						context.fc.WriteVariable("Compiler", ".Compiler_dummy");
						context.fc.WriteVariable("CompilerOptions", "'%1 %2'");
						context.fc.WriteVariable("CompilerOutputPath", "'/dummy/'");
					
						// These variables are required by the Library command as well
						// we just need to transfer the values in the linker variables
						// to these locations
						context.fc.WriteVariable("Librarian","'$Linker$'");
						context.fc.WriteVariable("LibrarianOptions","'$LinkerOptions$'");
						context.fc.WriteVariable("LibrarianOutput","'$LinkerOutput$'");

						context.fc.WriteVariable("LibrarianAdditionalInputs", ".Libraries");
					}
					context.fc.WritePopScope();
				}
				context.fc.WritePopScope();
			}

			// Output a list of aliases
			context.fc.WriteCommand("Alias", Quote(targetName + "-" + configName));
			context.fc.WritePushScope();
			context.fc.WriteArray("Targets", objectGroups, "'" + targetName + "-", "-" + configName + "'");

			if (hasLinkerStage)
			{
				context.fc.WriteVariable("Targets", "{'" + linkRuleName + "'}", "+");
			}
			context.fc.WritePopScope();
		}

		context.fc.WritePopScope();
	}

	static void WriteTargetDefinitions(GenerationContext& context)
	{
		context.fc.WriteSectionHeader("Target Definitions");

		// Now iterate each target in order
		for (OrderedTargets::iterator targetIter = context.orderedTargets.begin(); 
			targetIter != context.orderedTargets.end();
			++targetIter)
		{
			const cmTarget* constTarget = (*targetIter);
			if(constTarget->GetType() == cmTarget::INTERFACE_LIBRARY)
			{
				continue;
			}

			TargetContextMap::iterator findResult = context.targetContexts.find(constTarget);
			if (findResult == context.targetContexts.end())
			{
				continue;
			}

			cmTarget* target = findResult->second.target;
			cmLocalFastbuildGenerator* lg = findResult->second.lg;

			switch (target->GetType())
			{
				case cmTarget::EXECUTABLE:
				case cmTarget::SHARED_LIBRARY:
				case cmTarget::STATIC_LIBRARY:
				case cmTarget::MODULE_LIBRARY:
				case cmTarget::OBJECT_LIBRARY:
					WriteTargetDefinition(context, lg, *target);
					break;
				case cmTarget::UTILITY:
					// TODO
					break;
				case cmTarget::GLOBAL_TARGET:
					// TODO
					break;
				default:
					break;
			}
		}
	}
	
	static void WriteAliases(GenerationContext& context)
	{
		context.fc.WriteSectionHeader("Aliases");

		// Write the following aliases:
		// Per Target
		// Per Config
		// All

		typedef std::map<std::string, std::vector<std::string>> TargetListMap;
		TargetListMap perConfig;
		TargetListMap perTarget;

		for (OrderedTargets::iterator targetIter = context.orderedTargets.begin(); 
			targetIter != context.orderedTargets.end();
			++targetIter)
		{
			const cmTarget* constTarget = (*targetIter);
			if(constTarget->GetType() == cmTarget::INTERFACE_LIBRARY)
			{
				continue;
			}

			TargetContextMap::iterator findResult = context.targetContexts.find(constTarget);
			if (findResult == context.targetContexts.end())
			{
				continue;
			}

			cmTarget* target = findResult->second.target;
			cmLocalFastbuildGenerator* lg = findResult->second.lg;
			const std::string & targetName = target->GetName();

			// Define compile flags
			for (std::vector<std::string>::iterator iter = context.self->Configurations.begin();
				iter != context.self->Configurations.end(); ++iter)
			{
				std::string & configName = *iter;
				std::string aliasName = targetName + "-" + configName;

				perTarget[targetName].push_back(aliasName);
				perConfig[configName].push_back(aliasName);
			}
		}

		context.fc.WriteComment("Per config");
		for (TargetListMap::iterator iter = perConfig.begin();
			iter != perConfig.end(); ++iter)
		{
			const std::string & configName = iter->first;
			const std::vector<std::string> & targets = iter->second;

			context.fc.WriteCommand("Alias", "'" + configName + "'");
			context.fc.WritePushScope();
			context.fc.WriteArray("Targets", targets, "'", "'");
			context.fc.WritePopScope();
		}

		context.fc.WriteComment("Per targets");
		for (TargetListMap::iterator iter = perTarget.begin();
			iter != perTarget.end(); ++iter)
		{
			const std::string & targetName = iter->first;
			const std::vector<std::string> & targets = iter->second;

			context.fc.WriteCommand("Alias", "'" + targetName + "'");
			context.fc.WritePushScope();
			context.fc.WriteArray("Targets", targets, "'", "'");
			context.fc.WritePopScope();
		}

		context.fc.WriteComment("All");
		context.fc.WriteCommand("Alias", "'All'");
		context.fc.WritePushScope();
		context.fc.WriteArray("Targets", context.self->Configurations, "'", "'");
		context.fc.WritePopScope();
	}
};

//----------------------------------------------------------------------------
cmGlobalGeneratorFactory* cmGlobalFastbuildGenerator::NewFactory()
{
	return new Factory();
}

//----------------------------------------------------------------------------
cmGlobalFastbuildGenerator::cmGlobalFastbuildGenerator()
{
	this->FindMakeProgramFile = "CMakeFastbuildFindMake.cmake";
}

//----------------------------------------------------------------------------
cmGlobalFastbuildGenerator::~cmGlobalFastbuildGenerator()
{

}

//----------------------------------------------------------------------------
std::string cmGlobalFastbuildGenerator::GetName() const 
{
	return fastbuildGeneratorName; 
}

//----------------------------------------------------------------------------
cmLocalGenerator *cmGlobalFastbuildGenerator::CreateLocalGenerator()
{
	cmLocalGenerator * lg = new cmLocalFastbuildGenerator();
	lg->SetGlobalGenerator(this);
	return lg;
}

//----------------------------------------------------------------------------
void cmGlobalFastbuildGenerator::EnableLanguage(
	std::vector<std::string>const &  lang,
	cmMakefile *mf, bool optional)
{
  // Create list of configurations requested by user's cache, if any.
  this->cmGlobalGenerator::EnableLanguage(lang, mf, optional);
  Detail::Detection::DetectConfigurations(this, mf, this->Configurations);
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
  std::vector<std::string>& makeCommand,
  const std::string& makeProgram,
  const std::string& projectName,
  const std::string& projectDir,
  const std::string& targetName,
  const std::string& config,
  bool fast, bool verbose,
  std::vector<std::string> const& makeOptions)
{
	// A build command for fastbuild looks like this:
	// fbuild.exe [make-options] [-config projectName.bff] <target>-<config>

	// Setup make options
	std::vector<std::string> makeOptionsSelected;

	// Select the caller- or user-preferred make program
	std::string makeProgramSelected =
		this->SelectMakeProgram(makeProgram);

	// Select the target
	std::string targetSelected = targetName;
	// Note an empty target is a valid target (defaults to ALL anyway)
	if (targetSelected == "clean")
	{
		makeOptionsSelected.push_back("-clean");
		targetSelected = "";
	}
  
	// Select the config
	std::string configSelected = config;
	if (configSelected.empty())
	{
		configSelected = "Debug";
	}

	// Select fastbuild target
	if (targetSelected.empty())
	{
		targetSelected = configSelected;
	}
	else
	{
		targetSelected += "-" + configSelected;
	}

	// Build the command
	makeCommand.push_back(makeProgramSelected);
	
	// Push in the make options
	makeCommand.insert(makeCommand.end(), makeOptionsSelected.begin(), makeOptionsSelected.end());

	/*
	makeCommand.push_back("-config");
	makeCommand.push_back(projectName + ".bff");
	*/

	makeCommand.push_back("-showcmds");
	makeCommand.push_back("-ide");

	// Add the target-config to the command
	if (!targetSelected.empty())
	{
		makeCommand.push_back(targetSelected);
	}
}

//----------------------------------------------------------------------------
void cmGlobalFastbuildGenerator::AppendDirectoryForConfig(
	const std::string& prefix,
	const std::string& config,
	const std::string& suffix,
	std::string& dir)
{
	if(!config.empty())
	{
		dir += prefix;
		dir += config;
		dir += suffix;
	}
}

//----------------------------------------------------------------------------