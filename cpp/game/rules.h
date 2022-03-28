#ifndef GAME_RULES_H_
#define GAME_RULES_H_

#include "../core/global.h"
#include "../core/hash.h"

#include "../external/nlohmann_json/json.hpp"

struct Rules {

  //taxRule不删只是为了给之后新增规则留下个模板
  static const int TAX_NONE = 0;
  static const int TAX_SEKI = 1;
  static const int TAX_ALL = 2;
  int taxRule;


  float komi;
  //Min and max acceptable komi in various places involving user input validation
  static constexpr float MIN_USER_KOMI = -150.0f;
  static constexpr float MAX_USER_KOMI = 1500.0f;

  Rules();
  Rules(
    int taxRule,
    float komi
  );
  ~Rules();

  bool operator==(const Rules& other) const;
  bool operator!=(const Rules& other) const;

  bool equalsIgnoringKomi(const Rules& other) const;
  bool gameResultWillBeInteger() const;

  static Rules getTrompTaylorish();

  static std::set<std::string> taxRuleStrings();
  static int parseTaxRule(const std::string& s);
  static std::string writeTaxRule(int taxRule);

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

  static const Hash128 ZOBRIST_TAX_RULE_HASH[3];

private:
  nlohmann::json toJsonHelper(bool omitKomi, bool omitDefaults) const;
};

#endif  // GAME_RULES_H_
