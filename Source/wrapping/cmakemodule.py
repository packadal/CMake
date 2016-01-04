#!/usr/bin/env python
# -*- coding: utf-8 -*-

cacheManager = None
cmakeInstance = None
projects = []

def init(generatorName, sourceDirectory, buildDirectory):
    ###############################################################################
    #####                    initializing core components                     #####
    ###############################################################################

    cmSystemTools_FindCMakeResources('');
    #hack because the cmake root cannot be found when using python
    cmSystemTools_SetCMakeRoot("/media/dev/src/CMake-FBuild")

    global cmakeInstance
    cmakeInstance = cmake()
    #cmakeInstance.SetDebugOutputOn(True)
    #cmakeInstance.SetTrace(True)

    #dummy callback implemented in C++ that forwards to std::cout
    cmakeInstance.SetProgressCallback(progressCallback)

    #required dirs to know where to look for sources and where to write output
    cmakeInstance.SetHomeDirectory(sourceDirectory)
    cmakeInstance.SetHomeOutputDirectory(buildDirectory)

    # initialize the cache
    global cacheManager
    cacheManager = cmCacheManager()
    cacheManager.SaveCache(cmakeInstance.GetHomeOutputDirectory())
    cmakeInstance.LoadCache()
    cmakeInstance.PreLoadCMakeFiles()

    ###############################################################################
    #####                        Configure                                    #####
    ###############################################################################

    # create the generator
    globalGenerator = cmakeInstance.CreateGlobalGenerator(generatorName)
    cmakeInstance.SetGlobalGenerator(globalGenerator)
#    globalGenerator.SetCMakeInstance(cmakeInstance)

    # add cache variables
    # this is done in cmake::Configure but we can't call it from python
    # as it checks for the existence of a CMakeLists.txt
    cacheManager.AddCacheEntry("CMAKE_GENERATOR", globalGenerator.GetName(), "Name of generator.", cmState.INTERNAL)

    cacheManager.AddCacheEntry("CMAKE_HOME_DIRECTORY",
       cmakeInstance.GetHomeDirectory(),
       "Start directory with the top level CMakeLists.txt file for this "
       "project",
       cmState.INTERNAL)

    cacheManager.AddCacheEntry("CMAKE_GENERATOR_PLATFORM",
                                     cmakeInstance.GetGeneratorPlatform(),
                                     "Name of generator platform.",
                                     cmState.INTERNAL)

    cmakeInstance.CleanupCommandsAndMacros()
    mf = cmMakefile(globalGenerator, cmakeInstance.GetCurrentSnapshot())
    globalGenerator.EnableLanguage(["C", "CXX"], mf, False)
    globalGenerator.Configure()
    globalGenerator.CreateGenerationObjects()

###############################################################################
#####                        GENERATE !!                                  #####
###############################################################################
def generate():

    global projects
    global cmakeInstance
    global cacheManager

    print cmakeInstance.GetGlobalGenerator().GetLocalGenerators()
    for localGenerator in cmakeInstance.GetGlobalGenerator().GetLocalGenerators():
        for project in projects:
            projectCommand = cmProjectCommand()
            projectCommand.SetMakefile(localGenerator.GetMakefile())
            status = cmExecutionStatus()
            projectCommandParameters = [project._name, "LANGUAGES"]

            projectCommandParameters += project._languages
            projectCommand.InitialPass(projectCommandParameters, status)

            for target in project.targets:
                print target
                if isinstance(target, SharedLibrary):
                    localGenerator.GetMakefile().AddLibrary(target._name, target._targetType, target._sourceFiles)

    cmakeInstance.Generate()

###############################################################################
#####                      Base Project class                             #####
###############################################################################
class Project:
    def __init__(self, name, languages=["CXX", "C"]):
        self._name = name
        self._languages = languages
        self._targets = []

        global projects
        projects.append(self)

    @property
    def targets(self):
        self._targets


###############################################################################
#####                      Base Target class                              #####
###############################################################################
class Target(object):
    def __init__(self, name, sourceFiles):
        self._name = name
        self._sourceFiles = sourceFiles
        self._properties = {}
        self._targetType = cmState.UNKNOWN_LIBRARY

###############################################################################
#####                  Shared Library Specialization                      #####
###############################################################################
class SharedLibrary(Target):
    def __init__(self, name, sourceFiles):
        super(SharedLibrary, self).__init__(name, sourceFiles)
        self._targetType = cmState.SHARED_LIBRARY

    def SetProperty(self, name, value):
        self._properties[name] = value

    #testLibrary.SetProperty("OUTPUT_NAME", "plop")
    #for propertyName, propertyValue in testLibrary.GetProperties().iteritems():
    #    print (propertyName + ": " + propertyValue.GetValue())

