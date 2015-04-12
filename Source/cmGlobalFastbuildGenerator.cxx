
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

	void WriteComment(const char * comment)
	{
		fout << ";" << comment << "\n";
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

	static void DetectBaseLinkerCommand(std::string & command,
		cmLocalFastbuildGenerator *lg,
		cmTarget &target,
		const std::string & configName)
	{
		const std::string& linkLanguage = target.GetLinkerLanguage(configName);
	}

	static void DetectBaseCompileCommand(std::string & command,
		cmLocalFastbuildGenerator *lg,
		cmTarget &target,
		const std::string & configName,
		const std::string & language)
	{
		cmLocalGenerator::RuleVariables compileObjectVars;
		compileObjectVars.CMTarget = &target;
		compileObjectVars.Language = language.c_str();
		compileObjectVars.Source = "%1";
		compileObjectVars.Object = "%2";
		compileObjectVars.ObjectDir = "";
		compileObjectVars.ObjectFileDir = "";
		compileObjectVars.Flags = "";
		compileObjectVars.Defines = "";

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

			// Remove the command from the front
			std::vector<std::string> args = cmSystemTools::ParseArguments(compileCmd.c_str());

			// Join the args together and remove 0 from the front
			std::stringstream argSet;
			std::copy(args.begin() + 1, args.end(), std::ostream_iterator<std::string>(argSet, " "));
			compileCmd = argSet.str();
		}

		// Remove the command from the front and leave the flags behind
		//context.fc.WriteVariable("CompilerOptions", "'" + compileCmds[0] + "'");
	}

private:

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
		//context.fc.WriteVariable("CachePath", "\"C:\\.fbuild.cache\"");
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
		context.fc.WriteVariable("CompilerRoot", "'" + cxxCompilerPath + "'");

		context.fc.WriteCommand("Compiler", "'Compiler-default'");
		context.fc.WritePushScope();
		context.fc.WriteVariable("Executable", "'" + cxxCompilerFile + "'");
		context.fc.WritePopScope();

		return true;
	}
	
	static void WriteConfigurations(GenerationContext& context)
	{
		context.fc.WriteSectionHeader("Configurations");

		context.fc.WriteVariable("ConfigBase", "");
		context.fc.WritePushScopeStruct();
		context.fc.WriteVariable("Compiler", "'Compiler-default'");
		context.fc.WriteVariable("Librarian", "'$CompilerRoot$\\lib.exe'");
		context.fc.WriteVariable("Linker", "'$CompilerRoot$\\link.exe'");
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

		context.fc.WriteVariable("TargetDef_" + targetName, "");
		context.fc.WritePushScopeStruct();

		cmGeneratorTarget *gt = context.self->GetGeneratorTarget(&target);
		// get a list of source files
		std::vector<cmSourceFile const*> objectSources;
		gt->GetObjectSources(objectSources, "");

		std::vector<std::string> sourceFiles;
		for (std::vector<cmSourceFile const*>::iterator sourceIter = objectSources.begin();
			sourceIter != objectSources.end(); ++sourceIter)
		{
			cmSourceFile const *srcFile = *sourceIter;
			std::string sourceFile = srcFile->GetFullPath();
			sourceFiles.push_back(sourceFile);
		}
		context.fc.WriteArray("CompilerInputFiles", sourceFiles, "'", "'");

		// Write the dependencies of this target
		context.fc.WriteArray("PreBuildDependencies", std::vector<std::string>(), "'", "'");
		context.fc.WriteArray("Libraries", std::vector<std::string>(), "'", "'");

		// Define compile flags
		for (std::vector<std::string>::iterator iter = context.self->Configurations.begin();
			iter != context.self->Configurations.end(); ++iter)
		{
			std::string configName = *iter;
			context.fc.WriteVariable(configName + "Config", "");
			context.fc.WritePushScopeStruct();

			context.fc.WriteCommand("Using", ".ConfigBase");

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
				gt, "C", configName);
			std::string includeFlags = lg->GetIncludeFlags(
				includes,
				gt,
				"C",
				false,
				false,
				configName);

			context.fc.WriteVariable("IncludeFlags", "'" + linkPath + "'");

			std::string compilerOutputPath = targetName + "/" + configName + "/";
			std::string librarianOutput = compilerOutputPath + targetName + ".lib";
			std::string linkerOutput = compilerOutputPath + targetName + ".exe";
			cmSystemTools::ConvertToOutputSlashes(compilerOutputPath);
			cmSystemTools::ConvertToOutputSlashes(librarianOutput);
			cmSystemTools::ConvertToOutputSlashes(linkerOutput);

			// Tie together the variables
			context.fc.WriteVariable("CompilerOptions", "'$CompilerFlags$ $IncludeFlags$'");
			context.fc.WriteVariable("CompilerOutputPath", "'" + compilerOutputPath + "'");
			context.fc.WriteVariable("LibrarianOutput", "'" + librarianOutput + "'");
			context.fc.WriteVariable("LinkerOutput", "'" + linkerOutput + "'");
			context.fc.WriteVariable("LinkerOptions", "'%1 %2 $LinkFlags$'");

			context.fc.WritePopScope();

			if (target.GetType() != cmTarget::OBJECT_LIBRARY)
			{
				// on Windows the output dir is already needed at compile time
				// ensure the directory exists (OutDir test)
				EnsureDirectoryExists(target.GetDirectory(configName), context);
			}
		}

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

		// Select command type to use
		std::string commandNameA = "Library";
		std::string commandNameB = "";
		std::string targetNameBase = targetName + "-" + configName;
		std::string targetNameA = targetNameBase;
		std::string targetNameB = targetNameBase;

		switch (target.GetType())
		{
			case cmTarget::EXECUTABLE:
			{
				commandNameA = "ObjectList";
				commandNameB = "Executable";
				targetNameA += "-" + commandNameA;
				targetNameB += "-" + commandNameB;
				break;
			}
			case cmTarget::SHARED_LIBRARY:
			{
				commandNameA = "ObjectList";
				commandNameB = "DLL";
				targetNameA += "-" + commandNameA;
				targetNameB += "-" + commandNameB;
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
				commandNameA = "Alias";
				return;
			}
			}

		// Write fastbuild target definition 
		context.fc.WriteCommand(commandNameA, "'" + targetNameA + "'");
		context.fc.WritePushScope();

		context.fc.WriteCommand("Using", ".TargetDef_" + targetName);
		context.fc.WriteCommand("Using", "." + configName + "Config");

		context.fc.WritePopScope();

		// Write the second target definition
		if (!commandNameB.empty())
		{
			context.fc.WriteCommand(commandNameB, "'" + targetNameB + "'");
			context.fc.WritePushScope();

			context.fc.WriteCommand("Using", ".TargetDef_" + targetName);
			context.fc.WriteCommand("Using", "." + configName + "Config");

			std::vector<std::string> deps;
			deps.push_back(targetNameA);
			context.fc.WriteArray("PreBuildDependencies", deps, "'", "'");
			context.fc.WriteArray("Libraries", deps, "'", "'");

			context.fc.WritePopScope();

			context.fc.WriteCommand("Alias", "'" + targetNameBase + "'");
			context.fc.WritePushScope();
			context.fc.WriteVariable("Targets", "{ '" + targetNameA + "', '" + targetNameB + "' }");
			context.fc.WritePopScope();
		}
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

					WriteTarget(context, lg, target, configName);
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