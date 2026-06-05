#include "../search/search.h"

#include "../search/searchnode.h"

//------------------------
#include "../core/using.h"
//------------------------

static void failToroidalMirrorSearch() {
  assert(false);
  throw StringError("Anti-mirror/mirror search is disabled for toroidal boards");
}

// Updates mirroringPla, mirrorAdvantage, mirrorCenterSymmetryError
void Search::updateMirroring() {
  mirroringPla = C_EMPTY;
  mirrorAdvantage = 0.0;
  mirrorCenterSymmetryError = 1e10;

  if(searchParams.antiMirror)
    failToroidalMirrorSearch();
}

bool Search::isMirroringSinceSearchStart(const BoardHistory& threadHistory, int skipRecent) const {
  (void)threadHistory;
  (void)skipRecent;
  failToroidalMirrorSearch();
  return false;
}

void Search::maybeApplyAntiMirrorPolicy(
  float& nnPolicyProb,
  const Loc moveLoc,
  const float* policyProbs,
  const Player movePla,
  const SearchThread* thread
) const {
  (void)nnPolicyProb;
  (void)moveLoc;
  (void)policyProbs;
  (void)movePla;
  (void)thread;
  failToroidalMirrorSearch();
}

void Search::maybeApplyAntiMirrorForcedExplore(
  double& childUtility,
  const double parentUtility,
  const Loc moveLoc,
  const float* policyProbs,
  const double thisChildWeight,
  const double totalChildWeight,
  const Player movePla,
  const SearchThread* thread,
  const SearchNode& parent
) const {
  (void)childUtility;
  (void)parentUtility;
  (void)moveLoc;
  (void)policyProbs;
  (void)thisChildWeight;
  (void)totalChildWeight;
  (void)movePla;
  (void)thread;
  (void)parent;
  failToroidalMirrorSearch();
}

void Search::hackNNOutputForMirror(std::shared_ptr<NNOutput>& result) const {
  (void)result;
  failToroidalMirrorSearch();
}
