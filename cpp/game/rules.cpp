#include "../game/rules.h"

#include "../external/nlohmann_json/json.hpp"

#include <sstream>

using namespace std;
using json = nlohmann::json;

Rules::Rules() {
  //Defaults if not set - closest match to TT rules
  scoringRule = SCORING_AREA;
}

Rules::Rules(
  int sRule
)
  :scoringRule(sRule)
{}

Rules::~Rules() {
}

bool Rules::operator==(const Rules& other) const {
  return
    scoringRule == other.scoringRule;
}

bool Rules::operator!=(const Rules& other) const {
  return
    scoringRule != other.scoringRule ;
}


Rules Rules::getTrompTaylorish() {
  Rules rules;
  rules.scoringRule = SCORING_AREA;
  return rules;
}



set<string> Rules::scoringRuleStrings() {
  return {"AREA"};
}

int Rules::parseScoringRule(const string& s) {
  if(s == "AREA") return Rules::SCORING_AREA;
  else throw IOError("Rules::parseScoringRule: Invalid scoring rule: " + s);
}

string Rules::writeScoringRule(int scoringRule) {
  if(scoringRule == Rules::SCORING_AREA) return string("AREA");
  return string("UNKNOWN");
}

ostream& operator<<(ostream& out, const Rules& rules) {
  out << "score" << Rules::writeScoringRule(rules.scoringRule);
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
  ret["scoring"] = writeScoringRule(scoringRule);
  return ret;
}


Rules Rules::updateRules(const string& k, const string& v, Rules oldRules) {
  Rules rules = oldRules;
  string key = Global::trim(k);
  string value = Global::trim(Global::toUpper(v));
  if(key == "score") rules.scoringRule = Rules::parseScoringRule(value);
  else if(key == "scoring") rules.scoringRule = Rules::parseScoringRule(value);
  else throw IOError("Unknown rules option: " + key);
  return rules;
}

static Rules parseRulesHelper(const string& sOrig) {
  Rules rules;
  string lowercased = Global::trim(Global::toLower(sOrig));
  
  if(lowercased == "tromp-taylor" || lowercased == "tromp_taylor" || lowercased == "tromp taylor" || lowercased == "tromptaylor") {
    rules.scoringRule = Rules::SCORING_AREA;
  }
  else if(sOrig.length() > 0 && sOrig[0] == '{') {
    //Default if not specified
    rules = Rules::getTrompTaylorish();
    try {
      json input = json::parse(sOrig);
      string s;
      for(json::iterator iter = input.begin(); iter != input.end(); ++iter) {
        string key = iter.key();
        if(key == "score")
          rules.scoringRule = Rules::parseScoringRule(iter.value().get<string>());
        else if(key == "scoring")
          rules.scoringRule = Rules::parseScoringRule(iter.value().get<string>());
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

      if(startsWithAndStrip(s,"scoring")) {
        if(startsWithAndStrip(s,"AREA")) rules.scoringRule = Rules::SCORING_AREA;
        else throw IOError("Could not parse rules: " + sOrig);
        continue;
      }
      if(startsWithAndStrip(s,"score")) {
        if(startsWithAndStrip(s,"AREA")) rules.scoringRule = Rules::SCORING_AREA;
        else throw IOError("Could not parse rules: " + sOrig);
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




const Hash128 Rules::ZOBRIST_SCORING_RULE_HASH[2] = {
  //Based on sha256 hash of Rules::SCORING_AREA, but also mixing none tax rule hash, to preserve legacy hashes
  Hash128(0x8b3ed7598f901494ULL ^ 0x72eeccc72c82a5e7ULL, 0x1dfd47ac77bce5f8ULL ^ 0x0d1265e413623e2bULL),
  //Based on sha256 hash of Rules::SCORING_TERRITORY, but also mixing seki tax rule hash, to preserve legacy hashes
  Hash128(0x381345dc357ec982ULL ^ 0x125bfe48a41042d5ULL, 0x03ba55c026026b56ULL ^ 0x061866b5f2b98a79ULL),
};