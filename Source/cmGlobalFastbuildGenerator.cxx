
/*============================================================================
  CMake - Cross Platform Makefile Generator
  Copyright 2000-2009 Kitware, Inc., Insight Software Consortium

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
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
		const std::string& prefix, const std::string& suffix)
	{
		WriteVariable(key, "");
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

	static void DetectBaseLinkerCommand(std::string & command,
		cmLocalFastbuildGenerator *lg,
		cmTarget &target,
		cmGeneratorTarget *gt,
		const std::string & configName)
	{
		const std::string& linkLanguage = target.GetLinkerLanguage(configName);

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
		vars.TargetPDB = "$TargetNamePDB$";

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

		vars.Flags = "";
		vars.LinkFlags = "";

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

	static void DetectLanguages(std::set<std::string> & languages,
		cmGlobalFastbuildGenerator * self,
		cmLocalFastbuildGenerator *lg,
		cmGeneratorTarget *gt,
		cmTarget &target)
	{
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
	struct GenerationContext
	{
		cmGlobalFastbuildGenerator * self;
		cmLocalGenerator* root;
		std::vector<cmLocalGenerator*>& generators;
		FileContext& fc;
	};

	static std::string Quote(const std::string& str, const std::string& quotation = "'")
	{
		return quotation + str + quotation;
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

	static void GenerateRootBFF(cmGlobalFastbuildGenerator * self,
		cmLocalGenerator* root, std::vector<cmLocalGenerator*>& generators)
	{
		// Calculate filename
		std::string fname = root->GetMakefile()->GetStartOutputDirectory();
		fname += "/";
		fname += root->GetMakefile()->GetProjectName();
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
			{ self, root, generators, fc };
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
		WriteTargets( context );
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

		// Calculate the root location of the compiler
		std::string cxxCompilerLocation = mf->GetDefinition("CMAKE_CXX_COMPILER") ?
            mf->GetSafeDefinition("CMAKE_CXX_COMPILER") :
            mf->GetSafeDefinition("CMAKE_C_COMPILER");
		if (cxxCompilerLocation.empty())
		{
			return false;
		}

		// Strip out the path to the compiler
		std::string cxxCompilerPath = 
			cmSystemTools::GetFilenamePath( cxxCompilerLocation );
		std::string cxxCompilerFile = "$CompilerRoot$\\" +
			cmSystemTools::GetFilenameName( cxxCompilerLocation );

		cmSystemTools::ConvertToOutputSlashes( cxxCompilerPath );
		cmSystemTools::ConvertToOutputSlashes( cxxCompilerFile );

		// Write out the compiler that has been configured
		context.fc.WriteVariable("CompilerRoot", Quote(cxxCompilerPath));

		context.fc.WriteCommand("Compiler", "'Compiler-default'");
		context.fc.WritePushScope();
		context.fc.WriteVariable("Executable", Quote(cxxCompilerFile));
		context.fc.WritePopScope();

		return true;
	}
	
	static void WriteConfigurations(GenerationContext& context)
	{
		context.fc.WriteSectionHeader("Configurations");

		context.fc.WriteVariable("ConfigBase", "");
		context.fc.WritePushScopeStruct();
		context.fc.WriteVariable("Compiler", "'Compiler-default'");
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
		if (target.GetType() == cmTarget::INTERFACE_LIBRARY)
		{
			return;
		}

		std::string targetName = target.GetName();
		cmGeneratorTarget *gt = context.self->GetGeneratorTarget(&target);

		std::string ruleTargetName = "TargetDef_" + targetName;
		context.fc.WriteVariable(ruleTargetName, "");
		context.fc.WritePushScopeStruct();

		// Write the dependencies of this target
		context.fc.WriteArray("PreBuildDependencies", std::vector<std::string>(), "'", "'");
		context.fc.WriteArray("Libraries", std::vector<std::string>(), "'", "'");

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

				std::string targetOutDir = target.GetDirectory(configName) +
					"/" + targetName + "/" + configName + "/";
				cmSystemTools::ConvertToOutputSlashes(targetOutDir);

				context.fc.WriteVariable("TargetOutDir", Quote(targetOutDir));

				if (target.GetType() != cmTarget::OBJECT_LIBRARY)
				{
					// on Windows the output dir is already needed at compile time
					// ensure the directory exists (OutDir test)
					EnsureDirectoryExists(targetOutDir, context);
				}
			}

			context.fc.WritePopScope();
		}

		// Figure out the list of languages in use by this object
		std::vector<std::string> objectGroups;
		std::set<std::string> languages;
		Detection::DetectLanguages(languages, context.self, lg, gt, target);

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

				// Compiler options
				{
					// Remove the command from the front and leave the flags behind
					std::string compileCmd;
					Detection::DetectBaseCompileCommand(compileCmd,
						lg, target, objectGroupLanguage);

					std::string executable;
					std::string flags;
					Detection::SplitExecutableAndFlags(compileCmd, executable, flags);

					// Define the compiler
					/*
					std::string compilerName = "Compiler-" + targetName + "-" + ruleObjectGroupName + "-" + configName;
					context.fc.WriteCommand("Compiler", Quote(compilerName));
					context.fc.WritePushScope();
					context.fc.WriteVariable("Executable", Quote(executable));
					context.fc.WritePopScope();

					context.fc.WriteVariable("Compiler", Quote(compilerName));
					*/
					
					context.fc.WriteVariable("Compiler", "'Compiler-default'");
					context.fc.WriteVariable("BaseCompilerOptions", Quote(flags));
				}

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

					std::vector<std::string> sourceFiles;
					for (std::vector<cmSourceFile const*>::iterator sourceIter = objectSources.begin();
						sourceIter != objectSources.end(); ++sourceIter)
					{
						cmSourceFile const *srcFile = *sourceIter;
						std::string sourceFile = srcFile->GetFullPath();
						sourceFiles.push_back(sourceFile);
					}
					context.fc.WriteArray("CompilerInputFiles", sourceFiles, "'", "'");

					// Unity source files:
					context.fc.WriteVariable("UnityInputFiles", ".CompilerInputFiles");
				}

				context.fc.WriteBlankLine();
				context.fc.WriteComment("Compiler options:");
				{
					std::string libflags;
					lg->GetStaticLibraryFlags(libflags, configName, &target);

					std::string linkLibs;
					std::string flags;
					std::string linkFlags;
					std::string frameworkPath;
					std::string linkPath;

					lg->GetTargetFlags(
						linkLibs,
						flags,
						linkFlags,
						frameworkPath,
						linkPath,
						gt,
						false);

					context.fc.WriteVariable("LibFlags", "'" + libflags + "'");
					context.fc.WriteVariable("LinkLibs", "'" + linkLibs + "'");
					context.fc.WriteVariable("CompilerFlags", "'" + flags + "'");
					context.fc.WriteVariable("LinkFlags", "'" + linkFlags + "'");
					context.fc.WriteVariable("FrameworkPath", "'" + frameworkPath + "'");
					context.fc.WriteVariable("LinkPath", "'" + linkPath + "'");

					std::vector<std::string> includes;
					lg->GetIncludeDirectories(includes,
						gt, objectGroupLanguage, configName);
					std::string includeFlags = lg->GetIncludeFlags(
						includes,
						gt,
						objectGroupLanguage,
						false,
						false,
						configName);

					context.fc.WriteVariable("IncludeFlags", "'" + linkPath + "'");

					// Tie together the variables
					context.fc.WriteVariable("CompilerOptions", "'$BaseCompilerOptions$ $CompilerFlags$ $IncludeFlags$'");
					context.fc.WriteVariable("CompilerOutputPath", "'$TargetOutDir$'");
				}

				std::string objectGroupRuleName = targetName + "-" + ruleObjectGroupName + "-" + configName;
				context.fc.WriteCommand("ObjectList", Quote(objectGroupRuleName));
				context.fc.WritePushScope();
				context.fc.WritePopScope();

				context.fc.WritePopScope();
			}
		}
		context.fc.WritePopScope();

		// Iterate over each configuration
		// This time to define linker settings for each config
		for (std::vector<std::string>::iterator iter = context.self->Configurations.begin();
			iter != context.self->Configurations.end(); ++iter)
		{
			std::string configName = *iter;
			std::string linkRuleName = targetName + "-" + configName + "-link";

			context.fc.WriteVariable("LinkerConfig_" + configName, "");
			context.fc.WritePushScopeStruct();

			context.fc.WriteCommand("Using", ".BaseConfig_" + configName);

			context.fc.WriteBlankLine();
			context.fc.WriteComment("Linker options:");
			// Linker options
			{
				// Remove the command from the front and leave the flags behind
				std::string linkCmd;
				Detection::DetectBaseLinkerCommand(linkCmd,
					lg, target, gt, configName);

				std::string executable;
				std::string flags;
				Detection::SplitExecutableAndFlags(linkCmd, executable, flags);

				context.fc.WriteVariable("Linker", Quote(executable));
				context.fc.WriteVariable("BaseLinkerOptions", Quote(flags));

				context.fc.WriteVariable("LibrarianOutput", "'$TargetOutDir$$TargetNameOut$'");
				context.fc.WriteVariable("LinkerOutput", "'$TargetOutDir$$TargetNameOut$'");
				context.fc.WriteVariable("LinkerOptions", "'$BaseLinkerOptions$'");

				context.fc.WriteArray("Libraries", objectGroups, "'" + targetName + "-", "-" + configName + "'");

				context.fc.WriteCommand("Executable", Quote(linkRuleName));
				context.fc.WritePushScope();
				context.fc.WritePopScope();
			}

			context.fc.WritePopScope();

			// Output a list of aliases
			context.fc.WriteCommand("Alias", Quote(targetName + "-" + configName));
			context.fc.WritePushScope();
			context.fc.WriteArray("Targets", objectGroups, "'" + targetName + "-", "-" + configName + "'");
			context.fc.WriteVariable("Targets", "{'" + linkRuleName + "'}", "+");
			context.fc.WritePopScope();
		}

		// Output the list of all objectList definitions
		context.fc.WriteArray("ObjectGroups", objectGroups, ".", "");

		context.fc.WritePopScope();
	}

	static void WriteTargetDefinitions(GenerationContext& context)
	{
		context.fc.WriteSectionHeader("Target Definitions");

		// Iterate over each of the targets
		for (std::vector<cmLocalGenerator*>::iterator iter = context.generators.begin();
			iter != context.generators.end(); ++iter)
		{
			cmLocalFastbuildGenerator *lg = static_cast<cmLocalFastbuildGenerator*>(*iter);

			cmTargets &tgts = lg->GetMakefile()->GetTargets();
			for (cmTargets::iterator targetIter = tgts.begin(); targetIter != tgts.end();
				++targetIter)
			{
				cmTarget &target = targetIter->second;
				WriteTargetDefinition(context, lg, target);
			}
		}
	}

	static void WriteTarget(GenerationContext& context,
		cmLocalGenerator *lg, cmTarget &target, const std::string& configName)
	{
		std::string targetName = target.GetName();
		std::string targetDef = "TargetDef_" + targetName;
		std::string targetNameBase = targetName + "-" + configName;
		std::string targetNameCompileBase = targetNameBase + "-Compile";
		std::string targetNameLink = targetNameBase + "-Link";
		std::vector<std::string> deps;
		deps.push_back(targetNameCompileBase);

		// Always writing two targets, then combining them using an alias.
		// First is the object list compilation definiton.
		// The second links the compiled objects to send to the output
		
		// Detection of the second command as follows:
		std::string linkCommand = "Library";
		switch (target.GetType())
		{
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
			case cmTarget::INTERFACE_LIBRARY:
			case cmTarget::UNKNOWN_LIBRARY:
			{
				// Ignoring this target generation...
				// Still generate a valid target by the name,
				// but don't make it do anything
				linkCommand = "Alias";
				return;
			}
		}

		// Write fastbuild target definition 
		context.fc.WriteComment("Defining target" + targetName + "-" + configName);
		context.fc.WritePushScope(); // Scope push
		
		context.fc.WriteVariable("TargetList", "{}");

		context.fc.WriteCommand("Using", "." + targetDef);

		context.fc.WriteCommand("ForEach", ".ObjectGroup in .ObjectGroups");
		context.fc.WritePushScope();

		context.fc.WriteCommand("Using", ".ObjectGroup");
		context.fc.WriteCommand("Using", "." + configName + "Config");
	
		std::string objectListName = targetNameCompileBase + "-$.ConfigName$";
		context.fc.WriteCommand("ObjectList", Quote(objectListName));
		
		context.fc.WriteVariable("TargetList", "{Quote(objectListName)}", "+");

		context.fc.WritePopScope();
		
		// Write the second target definition
		context.fc.WriteCommand(linkCommand, "'" + targetNameLink + "'");
		context.fc.WritePushScope();

		context.fc.WriteCommand("Using", "." + configName + "LinkerConfig");

		context.fc.WriteArray("PreBuildDependencies", deps, "'", "'");
		context.fc.WriteArray("Libraries", deps, "'", "'");

		context.fc.WritePopScope();

		// Write an alias to combine the two above
		context.fc.WriteCommand("Alias", "'" + targetNameBase + "'");
		context.fc.WritePushScope();
		context.fc.WriteVariable("Targets", "{ '" + targetNameBase + "-Compile', '" + targetNameBase + "-Link' }");
		context.fc.WritePopScope();

		context.fc.WritePopScope(); // Scope push
	}

	static void WriteTargets(GenerationContext& context)
	{
		context.fc.WriteSectionHeader("Targets");

		// Iterate over each of the targets
		for (std::vector<cmLocalGenerator*>::iterator iter = context.generators.begin();
			iter != context.generators.end(); ++iter)
		{
			cmLocalGenerator *lg = *iter;

			cmTargets &tgts = lg->GetMakefile()->GetTargets();
			for (cmTargets::iterator targetIter = tgts.begin(); targetIter != tgts.end();
				++targetIter)
			{
				cmTarget &target = targetIter->second;
				for (std::vector<std::string>::iterator iter = context.self->Configurations.begin();
					iter != context.self->Configurations.end(); ++iter)
				{
					std::string &configName = *iter;

					//WriteTarget(context, lg, target, configName);
				}
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

		// Iterate over each of the targets
		for (std::vector<cmLocalGenerator*>::iterator iter = context.generators.begin();
			iter != context.generators.end(); ++iter)
		{
			cmLocalGenerator *lg = *iter;

			cmTargets &tgts = lg->GetMakefile()->GetTargets();
			for (cmTargets::iterator targetIter = tgts.begin(); targetIter != tgts.end();
				++targetIter)
			{
				cmTarget &target = targetIter->second;
				const std::string & targetName = target.GetName();

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

	// Now execute the extra fastbuild process on the project map
	std::map<std::string, std::vector<cmLocalGenerator*> >::iterator it;
	for(it = this->ProjectMap.begin(); it!= this->ProjectMap.end(); ++it)
	{
		Detail::Generation::GenerateRootBFF(this, it->second[0], it->second);
    }
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
	if(targetSelected.empty())
    {
		targetSelected = "all";// VS uses "ALL_BUILD"; might be useful
    }
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

	// Build the command
	makeCommand.push_back(makeProgramSelected);
	
	// Push in the make options
	makeCommand.insert(makeCommand.end(), makeOptionsSelected.begin(), makeOptionsSelected.end());
	makeCommand.push_back("-config");
	makeCommand.push_back(projectName + ".bff");

	makeCommand.push_back("-showcmds");

	// Add the target-config to the command
	if (!targetSelected.empty())
	{
		std::string realTarget = targetSelected + "-" + configSelected;
		makeCommand.push_back(realTarget);
	}
}

//----------------------------------------------------------------------------