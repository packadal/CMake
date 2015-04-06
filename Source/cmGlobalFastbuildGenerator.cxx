
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
		WriteSectionHeader( fout, "Fastbuild makefile - Generated using CMAKE" );

		WriteSettings( fout );
		WriteCompilers( self, fout, root );
		WriteConfigurations( self, fout, root );

		// Sort targets

		WriteTargetDefinitions( self, fout, root, generators );
		WriteTargets( fout );
		WriteAliases( fout );
	}

	static void WriteComment(FastbuildFileContext& context, const char * comment)
	{
		fout << ";" << comment << "\n";
	}

	static void WriteLine(FastbuildFileContext& context)
	{
		WriteComment(fout, 
			"-------------------------------------------------------------------------------");
	}

	static void WriteSectionHeader(cFastbuildFileContext& context, const char * section)
	{
		fout << "\n";
		WriteLine( fout );
		WriteComment( fout, section );
		WriteLine( fout );
	}

	static void WriteFastbuildVariable( FastbuildFileContext& context )
	{
		
	}

	static void WriteFastbuildStruct( FastbuildFileContext& context, 
		const char * structName,
		const FastbuildVariables& variables )
	{
		
	}
	
	static void WriteSettings( FastbuildFileContext& context )
	{
		WriteSectionHeader( context, "Settings" );

		fout << "Settings\n";
		fout << "{\n";
		fout << "\t.CachePath = \"C:\\.fbuild.cache\"\n";
		fout << "}\n";
	}

	static bool WriteCompilers( FastbuildFileContext& context )
	{
		cmMakefile *mf = root->GetMakefile();

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
		std::string cxxCompilerFile = "$Root$\\" +
			cmSystemTools::GetFilenameName( cxxCompilerLocation );

		cmSystemTools::ConvertToOutputSlashes( cxxCompilerPath );
		cmSystemTools::ConvertToOutputSlashes( cxxCompilerFile );

		// Write out the compiler that has been configured
		fout << "Compiler( 'Compiler-default' )\n";
		fout << "{\n";
		fout << "\t.Root = \'" << cxxCompilerPath << "\'\n";
		fout << "\t.Executable = \'" << cxxCompilerFile << "\'\n";
		fout << "}\n";

		return true;
	}
	
	static void WriteConfigurations(FastbuildFileContext& context)
	{
		WriteSectionHeader( context, "Configurations" );

		// Iterate over all configurations and define them:
		for(std::vector<std::string>::iterator iter = self->Configurations.begin();
			iter != self->Configurations.end(); ++iter)
		{
			std::string & configName = *iter;
			std::vector<std::string

			WriteFastbuildStruct( fout, "config-" + configName, 
				)
			fout << "\t\t" << *i << "|" << this->GetPlatformName()
			<< " = "  << *i << "|" << this->GetPlatformName() << "\n";
		}
	}

	static void WriteTargetDefinitions(FastbuildFileContext& context)
	{
		WriteSectionHeader( context, "Target Definitions" );

		// Iterate over each of the targets
		for (std::vector<cmLocalGenerator*>::iterator iter = generators.begin();
			iter != generators.end(); ++iter)
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
				
				fout << target.GetName() << "\n";
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