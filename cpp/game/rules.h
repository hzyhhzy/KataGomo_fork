#ifndef GAME_RULES_H_
#define GAME_RULES_H_

#include "../core/global.h"
#include "../core/hash.h"

#include "../external/nlohmann_json/json.hpp"

struct Rules {


  static const int SCORING_AREA = 0;
  int scoringRule;



  float komi;
  //Min and max acceptable komi in various places involving user input validation
  static constexpr float MIN_USER_KOMI = -150.0f;
  static constexpr float MAX_USER_KOMI = 150.0f;

  Rules();
  Rules(
    int scoringRule,
    float komi
  );
  ~Rules();

  bool operator==(const Rules& other) const;
  bool operator!=(const Rules& other) const;

  bool equalsIgnoringKomi(const Rules& other) const;
  bool gameResultWillBeInteger() const;

  static Rules getTrompTaylorish();
  static Rules getSimpleTerritory();

  static std::set<std::string> scoringRuleStrings();
  static int parseScoringRule(const std::string& s);
  static std::string writeScoringRule(int scoringRule);

  static bool komiIsIntOrHalfInt(float komi);

  static Rules parseRules(const std::string& str);
  static Rules parseRulesWithoutKomi(const std::string& str, float komi);
  static bool tryParseRules(const std::string& str, Rules& buf);
  static bool tryParseRulesWithoutKomi(const std::string& str, Rules& buf, float komi);

  static Rules updateRules(const std::string& key, const std::string& value, Rules priorRules);

  friend std::ostream& operator<<(std::ostream& out, const Rules& rules);
  std::string toString() const;
  std::string toStringNoKomi() const;
  std::string toStringNoKomiMaybeNice() const;
  std::string toJsonString() const;
  std::string toJsonStringNoKomi() const;
  std::string toJsonStringNoKomiMaybeOmitStuff() const;
  nlohmann::json toJson() const;
  nlohmann::json toJsonNoKomi() const;
  nlohmann::json toJsonNoKomiMaybeOmitStuff() const;

  static const Hash128 ZOBRIST_SCORING_RULE_HASH[2];

private:
  nlohmann::json toJsonHelper(bool omitKomi, bool omitDefaults) const;
};

#endif  // GAME_RULES_H_
