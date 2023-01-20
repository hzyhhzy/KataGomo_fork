#include "../program/playsettings.h"

PlaySettings::PlaySettings()
  : initGamesWithPolicy(false),
    policyInitAvgMoveNum(0.0),
    startPosesPolicyInitAvgMoveNum(0.0),
   sidePositionProb(0.0),
   policyInitAreaTemperature(1.0),
   cheapSearchProb(0),cheapSearchVisits(0),cheapSearchTargetWeight(0.0f),
   reduceVisits(false),reduceVisitsThreshold(100.0),reduceVisitsThresholdLookback(1),reducedVisitsMin(0),reducedVisitsWeight(1.0f),
   policySurpriseDataWeight(0.0),valueSurpriseDataWeight(0.0),scaleDataWeight(1.0),
   recordTreePositions(false),recordTreeThreshold(0),recordTreeTargetWeight(0.0f),
   noResolveTargetWeights(false),
   allowResignation(false),resignThreshold(0.0),resignConsecTurns(1),
   forSelfPlay(false),
    normalAsymmetricPlayoutProb(0.0),
    maxAsymmetricRatio(2.0),
   recordTimePerMove(false)
{}
PlaySettings::~PlaySettings()
{}

PlaySettings PlaySettings::loadForMatch(ConfigParser& cfg) {
  PlaySettings playSettings;
  playSettings.allowResignation = cfg.getBool("allowResignation");
  playSettings.resignThreshold = cfg.getDouble("resignThreshold",-1.0,0.0); //Threshold on [-1,1], regardless of winLossUtilityFactor
  playSettings.resignConsecTurns = cfg.getInt("resignConsecTurns",1,100);
  playSettings.initGamesWithPolicy =  cfg.contains("initGamesWithPolicy") ? cfg.getBool("initGamesWithPolicy") : false;
  if(playSettings.initGamesWithPolicy) {
    playSettings.policyInitAvgMoveNum = cfg.getDouble("policyInitAvgMoveNum", 0.0, 100.0);
    playSettings.startPosesPolicyInitAvgMoveNum =
      cfg.contains("startPosesPolicyInitAvgMoveNum") ? cfg.getDouble("startPosesPolicyInitAvgMoveNum", 0.0, 100.0) : 0.0;
    playSettings.policyInitAreaTemperature = cfg.contains("policyInitAreaTemperature") ? cfg.getDouble("policyInitAreaTemperature",0.1,5.0) : 1.0;
  }
  playSettings.recordTimePerMove = true;
  return playSettings;
}

PlaySettings PlaySettings::loadForGatekeeper(ConfigParser& cfg) {
  PlaySettings playSettings;
  playSettings.allowResignation = cfg.getBool("allowResignation");
  playSettings.resignThreshold = cfg.getDouble("resignThreshold",-1.0,0.0); //Threshold on [-1,1], regardless of winLossUtilityFactor
  playSettings.resignConsecTurns = cfg.getInt("resignConsecTurns",1,100);
  return playSettings;
}

PlaySettings PlaySettings::loadForSelfplay(ConfigParser& cfg) {
  PlaySettings playSettings;
  playSettings.initGamesWithPolicy = cfg.getBool("initGamesWithPolicy");
  playSettings.policyInitAvgMoveNum =
    cfg.contains("policyInitAvgMoveNum") ? cfg.getDouble("policyInitAvgMoveNum", 0.0, 100.0) : 12.0;
  playSettings.startPosesPolicyInitAvgMoveNum =
    cfg.contains("startPosesPolicyInitAvgMoveNum") ? cfg.getDouble("startPosesPolicyInitAvgMoveNum", 0.0, 100.0) : 0.0;
  playSettings.sidePositionProb =
    //forkSidePositionProb is the legacy name, included for backward compatibility
    (cfg.contains("forkSidePositionProb") && !cfg.contains("sidePositionProb")) ?
    cfg.getDouble("forkSidePositionProb",0.0,1.0) : cfg.getDouble("sidePositionProb",0.0,1.0);

  playSettings.policyInitAreaTemperature = cfg.contains("policyInitAreaTemperature") ? cfg.getDouble("policyInitAreaTemperature",0.1,5.0) : 1.0;


  playSettings.cheapSearchProb = cfg.getDouble("cheapSearchProb",0.0,1.0);
  playSettings.cheapSearchVisits = cfg.getInt("cheapSearchVisits",1,10000000);
  playSettings.cheapSearchTargetWeight = cfg.getFloat("cheapSearchTargetWeight",0.0f,1.0f);
  playSettings.reduceVisits = cfg.getBool("reduceVisits");
  playSettings.reduceVisitsThreshold = cfg.getDouble("reduceVisitsThreshold",0.0,0.999999);
  playSettings.reduceVisitsThresholdLookback = cfg.getInt("reduceVisitsThresholdLookback",0,1000);
  playSettings.reducedVisitsMin = cfg.getInt("reducedVisitsMin",1,10000000);
  playSettings.reducedVisitsWeight = cfg.getFloat("reducedVisitsWeight",0.0f,1.0f);
  playSettings.policySurpriseDataWeight = cfg.getDouble("policySurpriseDataWeight",0.0,1.0);
  playSettings.valueSurpriseDataWeight = cfg.getDouble("valueSurpriseDataWeight",0.0,1.0);
  playSettings.scaleDataWeight = cfg.contains("scaleDataWeight") ? cfg.getDouble("scaleDataWeight",0.01,10.0) : 1.0;
  playSettings.normalAsymmetricPlayoutProb = cfg.getDouble("normalAsymmetricPlayoutProb",0.0,1.0);
  playSettings.maxAsymmetricRatio = cfg.getDouble("maxAsymmetricRatio",1.0,100.0);
  playSettings.forSelfPlay = true;

  if(playSettings.policySurpriseDataWeight + playSettings.valueSurpriseDataWeight > 1.0)
    throw StringError("policySurpriseDataWeight + valueSurpriseDataWeight > 1.0");

  return playSettings;
}
