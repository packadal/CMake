
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
	struct FastbuildFileContext
	{
		cmGeneratedFileStream & fout;
		cmGlobalFastbuildGenerator * self;
		cmLocalGenerator* root;
		std::vector<cmLocalGenerator*>& generators;
		std::string linePrefix;
	};

	struct FastbuildVariable
	{
		std::string key;
		std::string value;
	};
	typedef std::vector<FastbuildVariable> FastbuildVariables;

	static void GenerateRootBFF(cmGlobalFastbuildGenerator * self,
		cmLocalGenerator* root, std::vector<cmLocalGenerator*>& generators)
	{
		// Debug info:
		std::cout << "======== GLOBAL Fastbuild Gen ========\n";

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

		FastbuildFileContext context = {
			fout, self, root, generators, ""
			};
		WriteRootBFF(context);
		
		// Close file
		if (fout.Close())
		{
			self->FileReplacedDuringGenerate(fname);
		}
	}

	static void WriteRootBFF(FastbuildFileContext& context)
	{
		WriteSectionHeader( context, "Fastbuild makefile - Generated using CMAKE" );

		WriteSettings( context );
		WriteCompilers( context );
		WriteConfigurations( context );

		// Sort targets

		WriteTargetDefinitions( context );
		WriteTargets( context );
		WriteAliases( context );
	}

	static void WriteComment(FastbuildFileContext& context, const char * comment)
	{
		context.fout << ";" << comment << "\n";
	}

	static void WriteLine(FastbuildFileContext& context)
	{
		context.fout << 
			";-------------------------------------------------------------------------------\n";
	}

	static void WriteSectionHeader( FastbuildFileContext& context, const char * section )
	{
		context.fout << "\n";
		WriteLine( context );
		WriteComment( context, section );
		WriteLine( context );
	}

	static void WritePushScope( FastbuildFileContext& context )
	{
		context.fout << context.linePrefix << "{\n";
		context.linePrefix += "\t";
	}

	static void WritePopScope( FastbuildFileContext& context )
	{
		assert( !context.linePrefix.empty() );
		context.linePrefix.resize( context.linePrefix.size() - 1 );

		context.fout << context.linePrefix << "}\n";
	}

	static void WriteVariable( FastbuildFileContext& context, const std::string& key, const std::string& value )
	{
		context.fout << context.linePrefix << 
			key << " = " << value << "\n";
	}

	static void WriteCommand( FastbuildFileContext& context, const std::string& command, const std::string& value = std::string())
	{
		context.fout << context.linePrefix << 
			command;
		if (!value.empty())
		{
			context.fout << "(" << value << ")";
		}
		context.fout << "\n";
	}

	static void WriteStruct( FastbuildFileContext& context, 
		const char * structName,
		const FastbuildVariables& variables )
	{
		
	}
	
	static void WriteSettings( FastbuildFileContext& context )
	{
		WriteSectionHeader( context, "Settings" );

		WriteCommand( context, "Settings" );
		WritePushScope( context );
		WriteVariable( context, "CachePath", "\"C:\\.fbuild.cache\"");
		WritePopScope( context );
	}

	static bool WriteCompilers( FastbuildFileContext& context )
	{
		cmMakefile *mf = context.root->GetMakefile();

		WriteSectionHeader( context, "Compilers" );

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
		WriteVariable( context, "CompilerRoot", "'" + cxxCompilerPath + "'" );

		WriteCommand( context, "Compiler", "'Compiler-default'");
		WritePushScope( context );
		WriteVariable( context, "Executable", "'" + cxxCompilerFile + "'" );
		WritePopScope( context );

		return true;
	}
	
	static void WriteConfigurations(FastbuildFileContext& context)
	{
		WriteSectionHeader( context, "Configurations" );

		WriteVariable( context, "ConfigBase", "" );
		WritePushScope( context );
		WriteVariable( context, "Compiler", "'Compiler-default'" );
		WriteVariable( context, "Librarian", "'$CompilerRoot$\\lib.exe'" );
		WriteVariable( context, "Linker", "'$CompilerRoot$\\link.exe'" );
		WritePopScope( context );

		// Iterate over all configurations and define them:
		/*
		for(std::vector<std::string>::iterator iter = context.self->Configurations.begin();
			iter != context.self->Configurations.end(); ++iter)
		{
			std::string & configName = *iter;
			/*
			std::vector<std::string

			WriteStruct( context, "config-" + configName, 
				)
			context.fout << "\t\t" << *i << "|" << this->GetPlatformName()
			<< " = "  << *i << "|" << this->GetPlatformName() << "\n";
		}
		*/
	}

	static void WriteTargetDefinitions(FastbuildFileContext& context)
	{
		WriteSectionHeader( context, "Target Definitions" );

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
				if(target.GetType() == cmTarget::INTERFACE_LIBRARY)
				{
					continue;
				}
				
				std::string targetName = "TargetDef_" + target.GetName();
				
				WriteVariable( context, targetName, "" );
				WritePushScope( context );

				cmGeneratorTarget *gt = context.self->GetGeneratorTarget(&target);
				// get a list of source files
				std::vector<cmSourceFile const*> objectSources;
				gt->GetObjectSources(objectSources, "");
				
				for (std::vector<cmSourceFile const*>::iterator sourceIter = objectSources.begin();
					sourceIter != objectSources.end(); ++sourceIter)
				{
					cmSourceFile const *srcFile = *sourceIter;
					std::string sourceFile = srcFile->GetFullPath();
					context.fout << sourceFile << "\n";
				}
				
				// Get include directories
				/*
				for(std::vector<std::string>::iterator i = configs->begin();
					i != configs->end(); ++i)
				{
				std::vector<std::string> includes;
				this->LocalGenerator->GetIncludeDirectories(includes,
				this->GeneratorTarget,
				"C", i->c_str());
				*/
				WritePopScope( context );
			}
		}
		

	}

	static void WriteTargets(FastbuildFileContext& context)
	{
		WriteSectionHeader( context, "Targets" );
	}

	static void WriteAliases(FastbuildFileContext& context)
	{
		WriteSectionHeader( context, "Aliases" );
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

	// Add the target-config to the command
	if (!targetSelected.empty())
	{
		std::string realTarget = targetSelected + "-" + configSelected;
		makeCommand.push_back(realTarget);
	}
}

//----------------------------------------------------------------------------