#include "../neuralnet/nninputs.h"
#include "../neuralnet/modelversion.h"

//Old model versions, no longer supported:
//0 = V1 features, with old head architecture using crelus (no longer supported)
//1 = V1 features, with new head architecture, no crelus
//2 = V2 features, no internal architecture change.

//Supported model versions:
//3 = V3 features, many architecture changes for new selfplay loop, including multiple board sizes
//4 = V3 features, scorebelief head
//5 = V4 features, changed current territory feature to just indicate pass-alive
//6 = V5 features, disable fancy features
//7 = V6 features, support new rules configurations
//8 = V7 features, unbalanced training, button go, lead and variance time
//9 = V7 features, shortterm value error
//10 = V7 features, shortterm value error done more properly
//11 = V7 features, supports mish activations by desc actually reading the activations

static void fail(int modelVersion) {
  throw StringError("NNModelVersion: Model version not currently implemented or supported: " + Global::intToString(modelVersion));
}

static_assert(NNModelVersion::oldestModelVersionImplemented == 8, "");
static_assert(NNModelVersion::oldestInputsVersionImplemented == 7, "");
static_assert(NNModelVersion::latestModelVersionImplemented == 11, "");
static_assert(NNModelVersion::latestInputsVersionImplemented == 7, "");

int NNModelVersion::getInputsVersion(int modelVersion) {
  if(modelVersion >= 8 && modelVersion <= 11)
    return 7;

  fail(modelVersion);
  return -1;
}

int NNModelVersion::getNumSpatialFeatures(int modelVersion) {
  if(modelVersion >= 8 && modelVersion <= 11)
    return NNInputs::NUM_FEATURES_SPATIAL_V7;

  fail(modelVersion);
  return -1;
}

int NNModelVersion::getNumGlobalFeatures(int modelVersion) {
  if(modelVersion >= 8 && modelVersion <= 11)
    return NNInputs::NUM_FEATURES_GLOBAL_V7;

  fail(modelVersion);
  return -1;
}
