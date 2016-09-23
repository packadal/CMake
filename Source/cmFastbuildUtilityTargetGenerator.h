#ifndef CMFASTBUILDUTILITYTARGETGENERATOR_H
#define CMFASTBUILDUTILITYTARGETGENERATOR_H

#include <cmFastbuildTargetGenerator.h>

class cmFastbuildUtilityTargetGenerator : public cmFastbuildTargetGenerator
{
public:
  cmFastbuildUtilityTargetGenerator(cmGeneratorTarget* gt);

  virtual void Generate();
};

#endif // CMFASTBUILDUTILITYTARGETGENERATOR_H
