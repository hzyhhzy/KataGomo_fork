#include "../game/rules.h"

#include "../external/nlohmann_json/json.hpp"

#include <sstream>

using namespace std;
using json = nlohmann::json;

Rules::Rules() {
  sixWinRule = SIXWINRULE_ALWAYS;
  wallBlock = false;
  VCNRule = VCNRULE_NOVC;
  firstPassWin = false;
  maxMoves = 0;
}

Rules::Rules(int sixWinRule, bool wallBlock, int VCNRule, bool firstPassWin, int maxMoves)
  : sixWinRule(sixWinRule), wallBlock(wallBlock), VCNRule(VCNRule), firstPassWin(firstPassWin), maxMoves(maxMoves) {}

Rules::~Rules() {}

bool Rules::operator==(const Rules& other) const {
  return sixWinRule == other.sixWinRule && wallBlock == other.wallBlock && VCNRule == other.VCNRule &&
         firstPassWin == other.firstPassWin && maxMoves == other.maxMoves;
}

bool Rules::operator!=(const Rules& other) const {
  return !(*this == other);
}

Rules Rules::getTrompTaylorish() {
  Rules rules = Rules();
  return rules;
}

set<string> Rules::SixWinRuleStrings() {
  return {"SIXWINRULE_ALWAYS", "SIXWINRULE_NEVER", "SIXWINRULE_CARO"};
}
set<string> Rules::VCNRuleStrings() {
  return {"NOVC", "VC1B", "VC2B", "VC3B", "VC4B", "VCTB", "VCFB", "VC1W", "VC2W", "VC3W", "VC4W", "VCTW", "VCFW"};
}

int Rules::parseSixWinRule(string s) {
  s = Global::toUpper(s);
  if(s == "SIXWINRULE_ALWAYS")
    return Rules::SIXWINRULE_ALWAYS;
  else if(s == "SIXWINRULE_NEVER")
    return Rules::SIXWINRULE_NEVER;
  else if(s == "SIXWINRULE_CARO")
    return Rules::SIXWINRULE_CARO;
  else
    throw IOError("Rules::parseSixWinRule: Invalid six win rule: " + s);
}

string Rules::writeSixWinRule(int sixWinRule) {
  if(sixWinRule == Rules::SIXWINRULE_ALWAYS)
    return string("SIXWINRULE_ALWAYS");
  if(sixWinRule == Rules::SIXWINRULE_NEVER)
    return string("SIXWINRULE_NEVER");
  if(sixWinRule == Rules::SIXWINRULE_CARO)
    return string("SIXWINRULE_CARO");
  return string("UNKNOWN");
}

int Rules::parseVCNRule(string s) {
  s = Global::toLower(s);
  if(s == "novc")
    return Rules::VCNRULE_NOVC;
  else if(s == "vc1b")
    return Rules::VCNRULE_VC1_B;
  else if(s == "vc2b")
    return Rules::VCNRULE_VC2_B;
  else if(s == "vc3b")
    return Rules::VCNRULE_VC3_B;
  else if(s == "vctb")
    return Rules::VCNRULE_VC3_B;
  else if(s == "vc4b")
    return Rules::VCNRULE_VC4_B;
  else if(s == "vcfb")
    return Rules::VCNRULE_VC4_B;
  else if(s == "vc1w")
    return Rules::VCNRULE_VC1_W;
  else if(s == "vc2w")
    return Rules::VCNRULE_VC2_W;
  else if(s == "vc3w")
    return Rules::VCNRULE_VC3_W;
  else if(s == "vctw")
    return Rules::VCNRULE_VC3_W;
  else if(s == "vc4w")
    return Rules::VCNRULE_VC4_W;
  else if(s == "vcfw")
    return Rules::VCNRULE_VC4_W;
  else
    throw IOError("Rules::parseVCNRule: Invalid VCN rule: " + s);
}

string Rules::writeVCNRule(int VCNRule) {
  if(VCNRule == Rules::VCNRULE_NOVC)
    return string("NOVC");
  if(VCNRule == Rules::VCNRULE_VC1_B)
    return string("VC1B");
  if(VCNRule == Rules::VCNRULE_VC2_B)
    return string("VC2B");
  if(VCNRule == Rules::VCNRULE_VC3_B)
    return string("VCTB");
  if(VCNRule == Rules::VCNRULE_VC4_B)
    return string("VCFB");
  if(VCNRule == Rules::VCNRULE_VC1_W)
    return string("VC1W");
  if(VCNRule == Rules::VCNRULE_VC2_W)
    return string("VC2W");
  if(VCNRule == Rules::VCNRULE_VC3_W)
    return string("VCTW");
  if(VCNRule == Rules::VCNRULE_VC4_W)
    return string("VCFW");
  return string("UNKNOWN");
}

Color Rules::vcSide() const {
  static_assert(VCNRULE_VC1_W == VCNRULE_VC1_B + 10, "Ensure VCNRule%10==N, VCNRule/10+1==color");
  if(VCNRule == VCNRULE_NOVC)
    return C_EMPTY;
  return 1 + VCNRule / 10;
}

int Rules::vcLevel() const {
  static_assert(VCNRULE_VC1_W == VCNRULE_VC1_B + 10, "Ensure VCNRule%10==N, VCNRule/10+1==color");
  if(VCNRule == VCNRULE_NOVC)
    return -1;
  return VCNRule % 10;
}

ostream& operator<<(ostream& out, const Rules& rules) {
  out << "sixwinrule" << Rules::writeSixWinRule(rules.sixWinRule);
  out << "wallblock" << rules.wallBlock;
  out << "vcnrule" << Rules::writeVCNRule(rules.VCNRule);
  out << "firstpasswin" << rules.firstPassWin;
  out << "maxmoves" << rules.maxMoves;
  return out;
}

string Rules::toString() const {
  ostringstream out;
  out << (*this);
  return out.str();
}


json Rules::toJson() const {
  json ret;
  ret["sixwinrule"] = writeSixWinRule(sixWinRule);
  ret["vcnrule"] = writeVCNRule(VCNRule);
  ret["firstpasswin"] = firstPassWin;
  ret["maxmoves"] = maxMoves;
  return ret;
}


string Rules::toJsonString() const {
  return toJson().dump();
}



Rules Rules::updateRules(const string& k, const string& v, Rules oldRules) {
  Rules rules = oldRules;
  string key = Global::toLower(Global::trim(k));
  string value = Global::trim(Global::toUpper(v));
  if(key == "sixwinrule")
    rules.sixWinRule = Rules::parseSixWinRule(value);
  else if(key == "wallblock") {
    rules.wallBlock = Global::stringToBool(value);
  }
  else if(key == "vcnrule") {
    rules.firstPassWin = false;
    rules.maxMoves = 0;
    rules.VCNRule = Rules::parseVCNRule(value);
  } else if(key == "firstpasswin") {
    rules.VCNRule = VCNRULE_NOVC;
    rules.maxMoves = 0;
    rules.firstPassWin = Global::stringToBool(value);
  } else if(key == "maxmoves") {
    rules.firstPassWin = false;
    rules.VCNRule = VCNRULE_NOVC;
    rules.maxMoves = Global::stringToInt(value);
  } else
    throw IOError("Unknown rules option: " + key);
  return rules;
}

static Rules parseRulesHelper(const string& sOrig) {
  Rules rules;
  string lowercased = Global::trim(Global::toLower(sOrig));
  if(sOrig.length() > 0 && sOrig[0] == '{') {
    // Default if not specified
    rules = Rules::getTrompTaylorish();
    try {
      json input = json::parse(sOrig);
      string s;
      for(json::iterator iter = input.begin(); iter != input.end(); ++iter) {
        string key = iter.key();
        
        Rules::updateRules(key, iter.value(), rules);
        
      }
    } catch(nlohmann::detail::exception&) {
      throw IOError("Could not parse rules: " + sOrig);
    }
  }

  // This is more of a legacy internal format, not recommended for users to provide
  else {
    throw IOError("Could not parse rules: " + sOrig);
  }

  return rules;
}

Rules Rules::parseRules(const string& sOrig) {
  return parseRulesHelper(sOrig);
}

bool Rules::tryParseRules(const string& sOrig, Rules& buf) {
  Rules rules;
  try {
    rules = parseRulesHelper(sOrig);
  } catch(const StringError&) {
    return false;
  }
  buf = rules;
  return true;
}

string Rules::toStringMaybeNice() const {
  return toString();
}

const Hash128 Rules::ZOBRIST_SIXWIN_RULE_HASH[3] = {
  Hash128(0x72eeccc72c82a5e7ULL, 0x0d1265e413623e2bULL),  // Based on sha256 hash of Rules::TAX_NONE
  Hash128(0x125bfe48a41042d5ULL, 0x061866b5f2b98a79ULL),  // Based on sha256 hash of Rules::TAX_SEKI
  Hash128(0xa384ece9d8ee713cULL, 0xfdc9f3b5d1f3732bULL),  // Based on sha256 hash of Rules::TAX_ALL
};
const Hash128 Rules::ZOBRIST_WALLBLOCK_HASH = Hash128(0x37b8f9b3011b420bULL, 0x706d097a80e19a64ULL);

const Hash128 Rules::ZOBRIST_FIRSTPASSWIN_HASH = Hash128(0x082b14fef06c9716ULL, 0x98f5e636a9351303ULL);

const Hash128 Rules::ZOBRIST_VCNRULE_HASH_BASE = Hash128(0x0dbdfa4e0ec7459cULL, 0xcc360848cf5d7c49ULL);

const Hash128 Rules::ZOBRIST_MAXMOVES_HASH_BASE = Hash128(0x8aba00580c378fe8ULL, 0x7f6c1210e74fb440ULL);

const Hash128 Rules::ZOBRIST_PASSNUM_B_HASH_BASE = Hash128(0x5a881a894f189de8ULL, 0x80adfc5ab8789990ULL);

const Hash128 Rules::ZOBRIST_PASSNUM_W_HASH_BASE = Hash128(0x0d9c957db399f5b2ULL, 0xbf7a532d567346b6ULL);
