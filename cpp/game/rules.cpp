#include "../game/rules.h"

#include "../external/nlohmann_json/json.hpp"

#include <sstream>

using namespace std;
using json = nlohmann::json;

Rules::Rules() {
  //Defaults if not set - closest match to TT rules
  koRule = KO_SIMPLE;
  multiStoneSuicideLegal = true;
  blackCapturesToWin = 1;
  whiteCapturesToWin = 1;
  komi = 7.5f;
}

Rules::Rules(
  int kRule,
  bool suic,
  int capB,
  int capW,
  float km
)
  :koRule(kRule), multiStoneSuicideLegal(suic), 
   blackCapturesToWin(capB), whiteCapturesToWin(capW),
   komi(km)
{}

Rules::~Rules() {
}

bool Rules::operator==(const Rules& other) const {
  return
    koRule == other.koRule &&
    multiStoneSuicideLegal == other.multiStoneSuicideLegal &&
    blackCapturesToWin==other.blackCapturesToWin &&
    whiteCapturesToWin==other.whiteCapturesToWin &&
    komi == other.komi;
}

bool Rules::operator!=(const Rules& other) const {
  return !(*this == other);
}


bool Rules::gameResultWillBeInteger() const {
  bool komiIsInteger = ((int)komi) == komi;
  return komiIsInteger;
}

Rules Rules::getTrompTaylorish() {
  Rules rules;
  rules.koRule = KO_SIMPLE;
  rules.multiStoneSuicideLegal = true;
  rules.komi = 7.5f;
  rules.blackCapturesToWin = 1;
  rules.whiteCapturesToWin = 1;
  return rules;
}


bool Rules::komiIsIntOrHalfInt(float komi) {
  return std::isfinite(komi) && komi * 2 == (int)(komi * 2);
}

set<string> Rules::koRuleStrings() {
  return {"SIMPLE"};
}

int Rules::parseKoRule(const string& s) {
  if(s == "SIMPLE") return Rules::KO_SIMPLE;
  else throw IOError("Rules::parseKoRule: Invalid ko rule: " + s);
}

string Rules::writeKoRule(int koRule) {
  if(koRule == Rules::KO_SIMPLE) return string("SIMPLE");
  return string("UNKNOWN");
}
ostream& operator<<(ostream& out, const Rules& rules) {
  out << "ko" << Rules::writeKoRule(rules.koRule)
      << "sui" << rules.multiStoneSuicideLegal;
  out << "capb" << rules.blackCapturesToWin;
  out << "capw" << rules.whiteCapturesToWin;
  out << "komi" << rules.komi;
  return out;
}

string Rules::toString() const {
  ostringstream out;
  out << (*this);
  return out.str();
}


json Rules::toJson() const {
  json ret;
  ret["ko"] = writeKoRule(koRule);
  ret["suicide"] = multiStoneSuicideLegal;
  ret["capb"] = blackCapturesToWin;
  ret["capw"] = whiteCapturesToWin;
  ret["komi"] = komi;
  return ret;
}

Hash128 Rules::getRuleHashExceptKomi() const {
  Hash128 hash = Rules::ZOBRIST_KO_RULE_HASH[koRule];
  if(multiStoneSuicideLegal)
    hash ^= Rules::ZOBRIST_MULTI_STONE_SUICIDE_HASH;
  assert(blackCapturesToWin > 0 && whiteCapturesToWin > 0);
  hash ^= Hash128(
    Hash::rrmxmx(blackCapturesToWin * ZOBRIST_BLACK_CAPTURENUM_RULE_HASH.hash0),
    Hash::rrmxmx(blackCapturesToWin * ZOBRIST_BLACK_CAPTURENUM_RULE_HASH.hash1));
  hash ^= Hash128(
    Hash::rrmxmx(whiteCapturesToWin * ZOBRIST_WHITE_CAPTURENUM_RULE_HASH.hash0),
    Hash::rrmxmx(whiteCapturesToWin * ZOBRIST_WHITE_CAPTURENUM_RULE_HASH.hash1));

  return hash;
}


string Rules::toJsonString() const {
  return toJson().dump();
}


Rules Rules::updateRules(const string& k, const string& v, Rules oldRules) {
  Rules rules = oldRules;
  string key = Global::trim(k);
  string value = Global::trim(Global::toUpper(v));
  if(key == "ko") rules.koRule = Rules::parseKoRule(value);
  else if(key == "suicide")
    rules.multiStoneSuicideLegal = Global::stringToBool(value);
  else if(key == "capb" || key == "capw" || key == "cap") {
    int v = Global::stringToInt(value);
    if(v <= 0 || v > MAX_CAPTURE_TO_WIN)
      throw IOError("Bad cap rules option: " + key + ". value should between 1 and" + to_string(MAX_CAPTURE_TO_WIN));
    if(key == "capb")
      rules.blackCapturesToWin = v;
    else if(key == "capw")
      rules.whiteCapturesToWin = v;
    else {
      rules.blackCapturesToWin = v;
      rules.whiteCapturesToWin = v;
    }
  }
  else throw IOError("Unknown rules option: " + key);
  return rules;
}

static Rules parseRulesHelper(const string& sOrig, bool allowKomi) {
  Rules rules = Rules();
  string lowercased = Global::trim(Global::toLower(sOrig));
  if(lowercased == "chinese") {
    rules.koRule = Rules::KO_SIMPLE;
    rules.multiStoneSuicideLegal = false;
    rules.komi = 7.5;
  }
  else if(lowercased == "tromp-taylor" || lowercased == "tromp_taylor" || lowercased == "tromp taylor" || lowercased == "tromptaylor") {
    rules.koRule = Rules::KO_SIMPLE;
    rules.multiStoneSuicideLegal = true;
    rules.komi = 7.5;
  }
  else if(sOrig.length() > 0 && sOrig[0] == '{') {
    //Default if not specified
    rules = Rules::getTrompTaylorish();
    try {
      json input = json::parse(sOrig);
      string s;
      for(json::iterator iter = input.begin(); iter != input.end(); ++iter) {
        string key = iter.key();
        if(key == "ko")
          rules.koRule = Rules::parseKoRule(iter.value().get<string>());
        else if(key == "suicide")
          rules.multiStoneSuicideLegal = iter.value().get<bool>();
        else if(key == "komi") {
          if(!allowKomi)
            throw IOError("Unknown rules option: " + key);
          rules.komi = iter.value().get<float>();
          if(rules.komi < Rules::MIN_USER_KOMI || rules.komi > Rules::MAX_USER_KOMI || !Rules::komiIsIntOrHalfInt(rules.komi))
            throw IOError("Komi value is not a half-integer or is too extreme");
        } 
        else if(key == "capb" || key == "capw" || key == "cap") {
          int v = iter.value().get<int>();
          if(v <= 0 || v > MAX_CAPTURE_TO_WIN)
            throw IOError(
              "Bad cap rules option: " + key + ". value should between 1 and" + to_string(MAX_CAPTURE_TO_WIN));
          if(key == "capb")
            rules.blackCapturesToWin = v;
          else if(key == "capw")
            rules.whiteCapturesToWin = v;
          else {
            rules.blackCapturesToWin = v;
            rules.whiteCapturesToWin = v;
          }
        }
        else
          throw IOError("Unknown rules option: " + key);
      }
    }
    catch(nlohmann::detail::exception&) {
      throw IOError("Could not parse rules: " + sOrig);
    }
  }

  //This is more of a legacy internal format, not recommended for users to provide
  else {
    auto startsWithAndStrip = [](string& str, const string& prefix) {
      bool matches = str.length() >= prefix.length() && str.substr(0,prefix.length()) == prefix;
      if(matches)
        str = str.substr(prefix.length());
      str = Global::trim(str);
      return matches;
    };

    //Default if not specified
    rules = Rules::getTrompTaylorish();

    string s = sOrig;
    s = Global::trim(s);

    //But don't allow the empty string
    if(s.length() <= 0)
      throw IOError("Could not parse rules: " + sOrig);

    while(true) {
      if(s.length() <= 0)
        break;

      if(startsWithAndStrip(s,"komi")) {
        if(!allowKomi)
          throw IOError("Could not parse rules: " + sOrig);
        int endIdx = 0;
        while(endIdx < s.length() && !Global::isAlpha(s[endIdx]) && !Global::isWhitespace(s[endIdx]))
          endIdx++;
        float komi;
        bool suc = Global::tryStringToFloat(s.substr(0,endIdx),komi);
        if(!suc)
          throw IOError("Could not parse rules: " + sOrig);
        if(!std::isfinite(komi) || komi > 1e5 || komi < -1e5)
          throw IOError("Could not parse rules: " + sOrig);
        rules.komi = komi;
        s = s.substr(endIdx);
        s = Global::trim(s);
        continue;
      }
      if(startsWithAndStrip(s, "capb")) {
        int endIdx = 0;
        while(endIdx < s.length() && !Global::isAlpha(s[endIdx]) && !Global::isWhitespace(s[endIdx]))
          endIdx++;
        int v;
        bool suc = Global::tryStringToInt(s.substr(0, endIdx), v);
        if(!suc)
          throw IOError("Could not parse rules: " + sOrig);
        if(v <= 0 || v > MAX_CAPTURE_TO_WIN)
          throw IOError(
            "Bad cap rules option. value should between 1 and" + to_string(MAX_CAPTURE_TO_WIN));
        rules.blackCapturesToWin = v;
        s = s.substr(endIdx);
        s = Global::trim(s);
        continue;
      }
      if(startsWithAndStrip(s, "capw")) {
        int endIdx = 0;
        while(endIdx < s.length() && !Global::isAlpha(s[endIdx]) && !Global::isWhitespace(s[endIdx]))
          endIdx++;
        int v;
        bool suc = Global::tryStringToInt(s.substr(0, endIdx), v);
        if(!suc)
          throw IOError("Could not parse rules: " + sOrig);
        if(v <= 0 || v > MAX_CAPTURE_TO_WIN)
          throw IOError("Bad cap rules option. value should between 1 and" + to_string(MAX_CAPTURE_TO_WIN));
        rules.whiteCapturesToWin = v;
        s = s.substr(endIdx);
        s = Global::trim(s);
        continue;
      }
      if(startsWithAndStrip(s,"ko")) {
        if(startsWithAndStrip(s,"SIMPLE")) rules.koRule = Rules::KO_SIMPLE;
        else throw IOError("Could not parse rules: " + sOrig);
        continue;
      }
      if(startsWithAndStrip(s,"sui")) {
        if(startsWithAndStrip(s,"1")) rules.multiStoneSuicideLegal = true;
        else if(startsWithAndStrip(s,"0")) rules.multiStoneSuicideLegal = false;
        else throw IOError("Could not parse rules: " + sOrig);
        continue;
      }

      //Unknown rules format
      else throw IOError("Could not parse rules: " + sOrig);
    }
  }

  return rules;
}

Rules Rules::parseRules(const string& sOrig) {
  return parseRulesHelper(sOrig,true);
}

bool Rules::tryParseRules(const string& sOrig, Rules& buf) {
  Rules rules;
  try { rules = parseRulesHelper(sOrig,true); }
  catch(const StringError&) { return false; }
  buf = rules;
  return true;
}


const Hash128 Rules::ZOBRIST_KO_RULE_HASH[2] = {
  Hash128(0x3cc7e0bf846820f6ULL, 0x1fb7fbde5fc6ba4eULL),  //Based on sha256 hash of Rules::KO_SIMPLE
  Hash128(0xcc18f5d47188554aULL, 0x3a63152c23e4128dULL),  //Based on sha256 hash of Rules::KO_POSITIONAL
};

const Hash128 Rules::ZOBRIST_BLACK_CAPTURENUM_RULE_HASH =  
  Hash128(0x3bc55e42b23b35bfULL, 0xc75fa1e615621dcdULL);

const Hash128 Rules::ZOBRIST_WHITE_CAPTURENUM_RULE_HASH =  
  Hash128(0x5b2096e48241d21bULL, 0x23cc18d4e85cd67fULL);

const Hash128 Rules::ZOBRIST_MULTI_STONE_SUICIDE_HASH =   //Based on sha256 hash of Rules::ZOBRIST_MULTI_STONE_SUICIDE_HASH
  Hash128(0xf9b475b3bbf35e37ULL, 0xefa19d8b1e5b3e5aULL);
