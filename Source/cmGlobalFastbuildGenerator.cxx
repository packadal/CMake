
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

	struct FileContext
	{
		cmGeneratedFileStream & fout;
		std::string linePrefix;
		std::string closingScope;
	};

	struct GenerationContext
	{
		cmGlobalFastbuildGenerator * self;
		cmLocalGenerator* root;
		std::vector<cmLocalGenerator*>& generators;
		FileContext fc;
	};

	static void GenerateConfigurations( cmGlobalFastbuildGenerator * self,
		cmMakefile* mf)
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
				if (std::find(self->Configurations.begin(), self->Configurations.end(), *iter) == self->Configurations.end())
				{
					self->Configurations.push_back(*iter);
				}
			}
		}

		// default to at least Debug and Release
		if(self->Configurations.size() == 0)
		{
			self->Configurations.push_back("Debug");
			self->Configurations.push_back("Release");
		}

		// Reset the entry to have a semi-colon separated list.
		std::string configs = self->Configurations[0];
		for(unsigned int i=1; i < self->Configurations.size(); ++i)
		{
			configs += ";";
			configs += self->Configurations[i];
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
		GenerationContext& context,
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

		if (target.GetType() != cmTarget::OBJECT_LIBRARY)
		{
			// on Windows the output dir is already needed at compile time
			// ensure the directory exists (OutDir test)
			EnsureDirectoryExists(target.GetDirectory(configName), context);
		}
	}

	static void DetectBaseLinkerCommand(std::string & command,
		GenerationContext& context,
		cmLocalFastbuildGenerator *lg, 
		cmTarget &target,
		const std::string & configName)
	{
		const std::string& linkLanguage = target.GetLinkerLanguage(configName);
	}

	static void DetectBaseCompileCommand(std::string & command,
		GenerationContext& context,
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
		WriteVariable(context.fc, "CompilerOptions", "'" + compileCmds[0] + "'");
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

		FileContext fc = 
			{ fout };
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
		WriteSectionHeader(context.fc, "Fastbuild makefile - Generated using CMAKE");

		WriteSettings( context );
		WriteCompilers( context );
		WriteConfigurations( context );

		// Sort targets

		WriteTargetDefinitions( context );
		WriteTargets( context );
		WriteAliases( context );
	}

	static void WriteComment(FileContext& context, const char * comment)
	{
		context.fout << ";" << comment << "\n";
	}

	static void WriteLine(FileContext& context)
	{
		context.fout << 
			";-------------------------------------------------------------------------------\n";
	}

	static void WriteSectionHeader(FileContext& context, const char * section)
	{
		context.fout << "\n";
		WriteLine( context );
		WriteComment( context, section );
		WriteLine( context );
	}

	static void WritePushScope(FileContext& context, char begin = '{', char end = '}')
	{
		context.fout << context.linePrefix << begin << "\n";
		context.linePrefix += "\t";
		context.closingScope += end;
	}

	static void WritePushScopeStruct(FileContext& context)
	{
		WritePushScope( context, '[', ']' );
	}

	static void WritePopScope(FileContext& context)
	{
		assert( !context.linePrefix.empty() );
		context.linePrefix.resize( context.linePrefix.size() - 1 );
		
		context.fout << context.linePrefix << context.closingScope[context.closingScope.size() - 1] << 
			"\n";

		context.closingScope.resize( context.closingScope.size() - 1 );
	}

	static void WriteVariable(FileContext& context, const std::string& key, const std::string& value,
		const std::string& operation = "=")
	{
		context.fout << context.linePrefix << "." <<
			key << " " << operation << " " << value << "\n";
	}

	static void WriteCommand(FileContext& context, const std::string& command, const std::string& value = std::string())
	{
		context.fout << context.linePrefix << 
			command;
		if (!value.empty())
		{
			context.fout << "(" << value << ")";
		}
		context.fout << "\n";
	}

	static void WriteArray(FileContext& context, const std::string& key,
		const std::vector<std::string>& values, const std::string& prefix, const std::string& suffix )
	{
		WriteVariable( context, key, "");
		WritePushScope( context );
		int size = values.size();
		for(int index = 0; index < size; ++index)
		{
			const std::string & value = values[index];
			bool isLast = index == size - 1;
			
			context.fout << context.linePrefix << prefix << value << suffix;
			if (!isLast)
			{
				context.fout << ',';
			}
			context.fout << "\n";
		}
		WritePopScope( context );
	}
	
	static void WriteSettings( GenerationContext& context )
	{
		WriteSectionHeader( context.fc, "Settings" );

		WriteCommand(context.fc, "Settings");
		WritePushScope(context.fc);
		//WriteVariable( context, "CachePath", "\"C:\\.fbuild.cache\"");
		WritePopScope(context.fc);
	}

	static bool WriteCompilers( GenerationContext& context )
	{
		cmMakefile *mf = context.root->GetMakefile();

		WriteSectionHeader(context.fc, "Compilers");

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
		WriteVariable(context.fc, "CompilerRoot", "'" + cxxCompilerPath + "'");

		WriteCommand(context.fc, "Compiler", "'Compiler-default'");
		WritePushScope(context.fc);
		WriteVariable(context.fc, "Executable", "'" + cxxCompilerFile + "'");
		WritePopScope(context.fc);

		return true;
	}
	
	static void WriteConfigurations(GenerationContext& context)
	{
		WriteSectionHeader(context.fc, "Configurations");

		WriteVariable(context.fc, "ConfigBase", "");
		WritePushScopeStruct(context.fc);
		WriteVariable(context.fc, "Compiler", "'Compiler-default'");
		WriteVariable(context.fc, "Librarian", "'$CompilerRoot$\\lib.exe'");
		WriteVariable(context.fc, "Linker", "'$CompilerRoot$\\link.exe'");
		WritePopScope(context.fc);

		// Iterate over all configurations and define them:
		for(std::vector<std::string>::iterator iter = context.self->Configurations.begin();
			iter != context.self->Configurations.end(); ++iter)
		{
			std::string & configName = *iter;
			WriteVariable(context.fc, "config_" + configName, "");
			WritePushScopeStruct(context.fc);

			// Using base config
			WriteCommand(context.fc, "Using", ".ConfigBase");

			WritePopScope(context.fc);
		}

		// Write out a list of all configs
		WriteArray(context.fc, "all_configs", context.self->Configurations,
			".config_", "");
	}

	static void WriteTargetDefinition(GenerationContext& context,
		cmLocalFastbuildGenerator *lg, cmTarget &target )
	{
		if(target.GetType() == cmTarget::INTERFACE_LIBRARY)
		{
			return;
		}
		
		std::string targetName = target.GetName();
		
		WriteVariable(context.fc, "TargetDef_" + targetName, "");
		WritePushScopeStruct(context.fc);

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
		WriteArray(context.fc, "CompilerInputFiles", sourceFiles, "'", "'");

		// Write the dependencies of this target
		WriteArray(context.fc, "PreBuildDependencies", std::vector<std::string>(), "'", "'");
		WriteArray(context.fc, "Libraries", std::vector<std::string>(), "'", "'");

		// Define compile flags
		for(std::vector<std::string>::iterator iter = context.self->Configurations.begin();
			iter != context.self->Configurations.end(); ++iter)
		{
			std::string configName = *iter;
			WriteVariable(context.fc, configName + "Config", "");
			WritePushScopeStruct(context.fc);

			WriteCommand(context.fc, "Using", ".ConfigBase");
		
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

			WriteVariable(context.fc, "LibFlags", "'" + libflags + "'");
			WriteVariable(context.fc, "LinkLibs", "'" + linkLibs + "'");
			WriteVariable(context.fc, "CompilerFlags", "'" + flags + "'");
			WriteVariable(context.fc, "LinkFlags", "'" + linkFlags + "'");
			WriteVariable(context.fc, "FrameworkPath", "'" + frameworkPath + "'");
			WriteVariable(context.fc, "LinkPath", "'" + linkPath + "'");

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
			
			WriteVariable(context.fc, "IncludeFlags", "'" + linkPath + "'");

			std::string compilerOutputPath = targetName + "/" + configName + "/";
			std::string librarianOutput = compilerOutputPath + targetName + ".lib";
			std::string linkerOutput = compilerOutputPath + targetName + ".exe";
			cmSystemTools::ConvertToOutputSlashes( compilerOutputPath );
			cmSystemTools::ConvertToOutputSlashes( librarianOutput );
			cmSystemTools::ConvertToOutputSlashes( linkerOutput );

			// Tie together the variables
			WriteVariable(context.fc, "CompilerOptions", "'$CompilerFlags$ $IncludeFlags$'");
			WriteVariable(context.fc, "CompilerOutputPath", "'" + compilerOutputPath + "'");
			WriteVariable(context.fc, "LibrarianOutput", "'" + librarianOutput + "'");
			WriteVariable(context.fc, "LinkerOutput", "'" + linkerOutput + "'");
			WriteVariable(context.fc, "LinkerOptions", "'%1 %2 $LinkFlags$'");

			WritePopScope(context.fc);
		}

		WritePopScope(context.fc);
	}

	static void WriteTargetDefinitions(GenerationContext& context)
	{
		WriteSectionHeader(context.fc, "Target Definitions");

		// Iterate over each of the targets
		for (std::vector<cmLocalGenerator*>::iterator iter = context.generators.begin();
			iter != context.generators.end(); ++iter)
		{
			cmLocalFastbuildGenerator *lg = static_cast<cmLocalFastbuildGenerator*>(*iter);

			cmTargets &tgts = lg->GetMakefile()->GetTargets();
			for(cmTargets::iterator targetIter = tgts.begin(); targetIter != tgts.end(); 
				++targetIter)
			{
				cmTarget &target = targetIter->second;
				WriteTargetDefinition( context, lg, target );
			}
		}
	}

	static void WriteTarget(GenerationContext& context,
		cmLocalGenerator *lg, cmTarget &target, const std::string& configName )
	{
		std::string targetName = target.GetName();

		// Select command type to use
		std::string commandNameA = "Library";
		std::string commandNameB = "";
		std::string targetNameBase = targetName + "-" + configName;
		std::string targetNameA = targetNameBase;
		std::string targetNameB = targetNameBase;

		switch( target.GetType() )
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
		WriteCommand(context.fc, commandNameA, "'" + targetNameA + "'");
		WritePushScope(context.fc);
		
		WriteCommand(context.fc, "Using", ".TargetDef_" + targetName);
		WriteCommand(context.fc, "Using", "." + configName + "Config");

		WritePopScope(context.fc);

		// Write the second target definition
		if (!commandNameB.empty())
		{
			WriteCommand(context.fc, commandNameB, "'" + targetNameB + "'");
			WritePushScope(context.fc);
			
			WriteCommand(context.fc, "Using", ".TargetDef_" + targetName);
			WriteCommand(context.fc, "Using", "." + configName + "Config");

			std::vector<std::string> deps;
			deps.push_back(targetNameA);
			WriteArray(context.fc, "PreBuildDependencies", deps, "'", "'");
			WriteArray(context.fc, "Libraries", deps, "'", "'");

			WritePopScope(context.fc);

			WriteCommand(context.fc, "Alias", "'" + targetNameBase + "'");
			WritePushScope(context.fc);
			WriteVariable(context.fc, "Targets", "{ '" + targetNameA + "', '" + targetNameB + "' }");
			WritePopScope(context.fc);
		}
	}

	static void WriteTargets(GenerationContext& context)
	{
		WriteSectionHeader(context.fc, "Targets");

		// Iterate over each of the targets
		for (std::vector<cmLocalGenerator*>::iterator iter = context.generators.begin();
			iter != context.generators.end(); ++iter)
		{
			cmLocalGenerator *lg = *iter;

			cmTargets &tgts = lg->GetMakefile()->GetTargets();
			for(cmTargets::iterator targetIter = tgts.begin(); targetIter != tgts.end(); 
				++targetIter)
			{
				cmTarget &target = targetIter->second;
				for(std::vector<std::string>::iterator iter = context.self->Configurations.begin();
					iter != context.self->Configurations.end(); ++iter)
				{
					std::string &configName = *iter;

					WriteTarget( context, lg, target, configName );
				}
			}
		}
	}

	static void WriteAliases(GenerationContext& context)
	{
		WriteSectionHeader(context.fc, "Aliases");

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
			for(cmTargets::iterator targetIter = tgts.begin(); targetIter != tgts.end(); 
				++targetIter)
			{
				cmTarget &target = targetIter->second;
				const std::string & targetName = target.GetName();
				
				// Define compile flags
				for(std::vector<std::string>::iterator iter = context.self->Configurations.begin();
					iter != context.self->Configurations.end(); ++iter)
				{
					std::string & configName = *iter;
					std::string aliasName = targetName + "-" + configName;

					perTarget[targetName].push_back(aliasName);
					perConfig[configName].push_back(aliasName);
				}
			}
		}

		WriteComment(context.fc, "Per config");
		for (TargetListMap::iterator iter = perConfig.begin();
			iter != perConfig.end(); ++iter)
		{
			const std::string & configName = iter->first;
			const std::vector<std::string> & targets = iter->second;

			WriteCommand(context.fc, "Alias", "'" + configName + "'");
			WritePushScope(context.fc);
			WriteArray(context.fc, "Targets", targets, "'", "'");
			WritePopScope(context.fc);
		}

		WriteComment(context.fc, "Per targets");
		for (TargetListMap::iterator iter = perTarget.begin();
			iter != perTarget.end(); ++iter)
		{
			const std::string & targetName = iter->first;
			const std::vector<std::string> & targets = iter->second;

			WriteCommand(context.fc, "Alias", "'" + targetName + "'");
			WritePushScope(context.fc);
			WriteArray(context.fc, "Targets", targets, "'", "'");
			WritePopScope(context.fc);
		}

		WriteComment(context.fc, "All");
		WriteCommand(context.fc, "Alias", "'All'");
		WritePushScope(context.fc);
		WriteArray(context.fc, "Targets", context.self->Configurations, "'", "'");
		WritePopScope(context.fc);
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
  Detail::GenerateConfigurations( this, mf );
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
		Detail::GenerateRootBFF( this, it->second[0], it->second );
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