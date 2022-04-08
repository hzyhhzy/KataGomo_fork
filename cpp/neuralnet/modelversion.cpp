#include "../neuralnet/nninputs.h"
#include "../neuralnet/modelversion.h"


//Supported model versions:
//98 = V97(V7OLD) features, no VCF, before 2021.1
//8 = V7 features, has VCF, "gomoku" branch on GitHub, before 2022.2
//10 = V10 features, support multi rules in single net, "GomDev" branch on GitHub, before 2022.4
//101 = V101 features, VCN+firstPassWin+maxmoves, "GomDevVCN" branch on GitHub

static void fail(int modelVersion) {
  throw StringError("NNModelVersion: Model version not currently implemented or supported: " + Global::intToString(modelVersion));
}

static_assert(NNModelVersion::oldestModelVersionImplemented == 8, "");
static_assert(NNModelVersion::oldestInputsVersionImplemented == 8, "");
static_assert(NNModelVersion::latestModelVersionImplemented == 101, "");
static_assert(NNModelVersion::latestInputsVersionImplemented == 101, "");

int NNModelVersion::getInputsVersion(int modelVersion) {
  if(modelVersion == 98)
    return 97;
  else if(modelVersion == 8)
    return 7;
  else if(modelVersion == 10)
    return 10;
  else if(modelVersion == 101)
    return 101;

  fail(modelVersion);
  return -1;
}

int NNModelVersion::getNumSpatialFeatures(int modelVersion) {
  if(modelVersion == 98)
    return NNInputs::NUM_FEATURES_SPATIAL_V7OLD;
  else if(modelVersion == 8)
    return NNInputs::NUM_FEATURES_SPATIAL_V7;
  else if(modelVersion == 10)
    return NNInputs::NUM_FEATURES_SPATIAL_V10;
  else if(modelVersion == 101)
    return NNInputs::NUM_FEATURES_SPATIAL_V101;

  fail(modelVersion);
  return -1;
}

int NNModelVersion::getNumGlobalFeatures(int modelVersion) {
  if(modelVersion == 98)
    return NNInputs::NUM_FEATURES_GLOBAL_V7OLD;
  else if(modelVersion == 8)
    return NNInputs::NUM_FEATURES_GLOBAL_V7;
  else if(modelVersion == 10)
    return NNInputs::NUM_FEATURES_GLOBAL_V10;
  else if(modelVersion == 101)
    return NNInputs::NUM_FEATURES_GLOBAL_V101;

  fail(modelVersion);
  return -1;
}
