%module cmake

%{
  #include <string>

  #include <cmProperty.h>
  #include <cmPropertyMap.h>


  #include "cmStandardIncludes.h"
  #include "cmObject.h"

  #include "cmSystemTools.h"

  #include "cmListFileCache.h"

  #include "cmLocalGenerator.h"
  #include "cmGlobalGenerator.h"

  #include "cmTarget.h"
  #include "cmMakefile.h"

  #include "cmState.h"
  #include "cmCacheManager.h"
  #include "cmake.h"

  #include "cmPolicies.h"
  #include "cmCommand.h"
  #include "cmProjectCommand.h"
  #include "cmExecutionStatus.h"

  void progressCallback(const char* message, float progress, void *)
  {
      std::cout << "[" << progress << " %] " << message << std::endl;
  }

%}

%include <stl.i>
%include <std_string.i>
%include <std_set.i>
%include <std_map.i>
%include "typemaps.i"

namespace std {
  %template(StringSet)       set<string>;
  %template(StringVector)    vector<string>;
  %template(LocalGeneratorVector)    vector<cmLocalGenerator*>;
  %template(PropertyMapBase)    map<string, cmProperty>;
}

%ignore cmCommand::Disallowed(cmPolicies::PolicyID pol, const char* e);

%ignore cmMakefile::RemoveFunctionBlocker(cmFunctionBlocker* fb, const cmListFileFunction& lff);

%include <cmProperty.h>
%include <cmPropertyMap.h>


%include "cmStandardIncludes.h"
%include "cmObject.h"

%ignore cmSystemTools::ExpandRegistryValues(std::string& source, KeyWOW64 view = KeyWOW64_Default);
%include "cmSystemTools.h"

%include "cmListFileCache.h"
%include "cmLocalGenerator.h"
%include "cmGlobalGenerator.h"

//cmStringRange and cmBacktraceRange do not work out of the box, ignore functions that use them for now
%ignore cmTarget::GetIncludeDirectoriesEntries;
%ignore cmTarget::GetIncludeDirectoriesBacktraces;
%ignore cmTarget::GetCompileOptionsEntries;
%ignore cmTarget::GetCompileOptionsBacktraces;
%ignore cmTarget::GetCompileFeaturesEntries;
%ignore cmTarget::GetCompileFeaturesBacktraces;
%ignore cmTarget::GetCompileDefinitionsEntries;
%ignore cmTarget::GetCompileDefinitionsBacktraces;
%ignore cmTarget::GetSourceEntries;
%ignore cmTarget::GetSourceBacktraces;
%ignore cmTarget::GetLinkImplementationEntries;
%ignore cmTarget::GetLinkImplementationBacktraces;
%include "cmTarget.h"

%ignore cmMakefile::GetIncludeDirectoriesEntries;
%ignore cmMakefile::GetIncludeDirectoriesBacktraces;
%ignore cmMakefile::GetCompileOptionsEntries;
%ignore cmMakefile::GetCompileOptionsBacktraces;
%ignore cmMakefile::GetCompileDefinitionsEntries;
%ignore cmMakefile::GetCompileDefinitionsBacktraces;
%include "cmMakefile.h"

%include "cmState.h"
%ignore cmCacheManager::NewIterator();
%ignore cmCacheManager::GetCacheIterator(const char *key=0);
%include "cmCacheManager.h"
%include "cmake.h"

%constant void progressCallback(const char* message, float progress, void *);

%include "cmCommand.h"
%include "cmProjectCommand.h"
%include "cmExecutionStatus.h"

// high-level interface
%pythoncode "cmakemodule.py"
