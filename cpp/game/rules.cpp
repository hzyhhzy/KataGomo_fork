#include "../game/rules.h"

#include "../external/nlohmann_json/json.hpp"

#include <sstream>

using namespace std;
using json = nlohmann::json;

Rules::Rules() {
  //Defaults if not set - closest match to TT rules
  loopPassRule = LOOPDRAW_PASSSCORING;
  komi = 0;
}

Rules::Rules(
  int loopPassRule,
  int komi
)
  :loopPassRule(loopPassRule), komi(komi) {}

Rules::~Rules() {
}

bool Rules::operator==(const Rules& other) const {
  return loopPassRule == other.loopPassRule && komi == other.komi;
}

bool Rules::operator!=(const Rules& other) const {
  return !(*this == other);
}


Rules Rules::getTrompTaylorish() {
  Rules rules;
  rules.loopPassRule = LOOPDRAW_PASSSCORING;
  return rules;
}



map<string,int> Rules::loopPassRuleStringsMap() {
  return {
    pair<string, int>("LOOPDRAW_PASSSCORING", 0),
    pair<string, int>("LOOPDRAW_PASSCONTINUE", 1),
    pair<string, int>("LOOPLOSE_PASSSCORING", 2),
    pair<string, int>("LOOPSCORING_PASSSCORING", 3)
  };
}

std::set<std::string> Rules::loopPassRuleStrings() {
  set<string> ruleSet;
  auto ruleMap = Rules::loopPassRuleStringsMap();
  for(auto r = ruleMap.begin(); r != ruleMap.end(); r++) {
    ruleSet.insert(r->first);
  }
  return ruleSet;
}

int Rules::parseLoopPassRule(const string& s) {
  auto ruleMap = loopPassRuleStringsMap();
  if(ruleMap.count(s))
    return ruleMap[s];
  else throw IOError("Rules::parseScoringRule: Invalid scoring rule: " + s);
}

string Rules::writeLoopPassRule(int scoringRule) {
  auto ruleMap = loopPassRuleStringsMap();
  for(auto s=ruleMap.begin();s!=ruleMap.end();s++) {
    if(s->second == scoringRule)
      return s->first;
  }
  return string("UNKNOWN");
}

ostream& operator<<(ostream& out, const Rules& rules) {
  out << "looppass" << Rules::writeLoopPassRule(rules.loopPassRule) << "komi" << rules.komi;
  return out;
}


string Rules::toString() const {
  ostringstream out;
  out << (*this);
  return out.str();
}

string Rules::toJsonString() const {
  return toJson().dump();
}

//omitDefaults: Takes up a lot of string space to include stuff, so omit some less common things if matches tromp-taylor rules
//which is the default for parsing and if not otherwise specified
json Rules::toJson() const {
  json ret;
  ret["looppass"] = writeLoopPassRule(loopPassRule);
  ret["komi"] = komi;
  return ret;
}


Rules Rules::updateRules(const string& k, const string& v, Rules oldRules) {
  Rules rules = oldRules;
  string key = Global::trim(k);
  string value = Global::trim(Global::toUpper(v));
  if(key == "looppass")
    rules.loopPassRule = Rules::parseLoopPassRule(value);
  else if(key == "komi") {
    int newKomi = oldRules.komi;
    bool suc = Global::tryStringToInt(value, newKomi);
    if(suc)
      rules.komi = newKomi;
    else
      throw IOError("Wrong komi: " + value + ", komi should be an integer");
  }
  else throw IOError("Unknown rules option: " + key);
  return rules;
}

static Rules parseRulesHelper(const string& sOrig) {
  Rules rules;
  string lowercased = Global::trim(Global::toLower(sOrig));
  
  if(lowercased == "tromp-taylor" || lowercased == "tromp_taylor" || lowercased == "tromp taylor" || lowercased == "tromptaylor") {
    rules.loopPassRule= Rules::LOOPDRAW_PASSSCORING;
    rules.komi = 0;
  }
  else if(sOrig.length() > 0 && sOrig[0] == '{') {
    //Default if not specified
    rules = Rules::getTrompTaylorish();
    try {
      json input = json::parse(sOrig);
      string s;
      for(json::iterator iter = input.begin(); iter != input.end(); ++iter) {
        string key = iter.key();
        rules = Rules::updateRules(key, iter.value().get<string>(),rules);
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

      if(startsWithAndStrip(s,"looppass")) {
        auto ruleMap = Rules::loopPassRuleStringsMap();
        for(auto r = ruleMap.begin(); r != ruleMap.end(); r++) {
          if(startsWithAndStrip(s, r->first)) {
            rules.loopPassRule = r->second;
            continue;
          }
        }
        throw IOError("Could not parse rules: " + sOrig);
        continue;
      }
      if(startsWithAndStrip(s, "komi")) {
        int endIdx = 0;
        while(endIdx < s.length() && Global::isDigit(s[endIdx]))
          endIdx++;
        int komi;
        bool suc = Global::tryStringToInt(s.substr(0, endIdx), komi);
        if(!suc)
          throw IOError("Could not parse rules: " + sOrig);
        if(komi > 1e5 || komi < -1e5)
          throw IOError("Could not parse rules: " + sOrig);
        rules.komi = komi;
        s = s.substr(endIdx);
        s = Global::trim(s);
        continue;
      }

      //Unknown rules format
      else throw IOError("Could not parse rules: " + sOrig);
    }
  }

  return rules;
}

string Rules::toStringMaybeNice() const {
  if(*this == parseRulesHelper("TrompTaylor"))
    return "TrompTaylor";
  return toString();
}

Rules Rules::parseRules(const string& sOrig) {
  return parseRulesHelper(sOrig);
}


bool Rules::tryParseRules(const string& sOrig, Rules& buf) {
  Rules rules;
  try { rules = parseRulesHelper(sOrig); }
  catch(const StringError&) { return false; }
  buf = rules;
  return true;
}





const Hash128 Rules::ZOBRIST_LOOPPASS_RULE_HASH[4] = {
  Hash128(0xcfe353052ab23e7aULL, 0x243466cc5740fa07ULL),
  Hash128(0x3bdac963636f8efbULL, 0x7f5d9b5d76a70889ULL),
  Hash128(0xd4aecfb2904ed7d1ULL, 0x9adba41979253974ULL),
  Hash128(0x23af6fd73de24455ULL, 0x2339118e63d7a780ULL)};
const Hash128 Rules::ZOBRIST_KOMI_RULE_HASH_BASE =
  Hash128(0x4c927b66ae674d8fULL, 0x1956c0e6b45360fbULL);