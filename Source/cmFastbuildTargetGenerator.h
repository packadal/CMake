#ifndef CMFASTBUILDTARGETGENERATOR_H
#define CMFASTBUILDTARGETGENERATOR_H

#include <cmCommonTargetGenerator.h>

class cmFastbuildTargetGenerator : public cmCommonTargetGenerator
{
public:
  cmFastbuildTargetGenerator(cmGeneratorTarget* gt);

  virtual void Generate() = 0;

  virtual void AddIncludeFlags(std::string& flags, std::string const& lang);

  std::string ConvertToFastbuildPath(const std::string& path);
};

#endif // CMFASTBUILDTARGETGENERATOR_H
