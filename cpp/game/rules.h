#ifndef GAME_RULES_H_
#define GAME_RULES_H_

#include "../core/global.h"
#include "../core/hash.h"

#include "../external/nlohmann_json/json.hpp"

struct Rules {

  static const int KO_SIMPLE = 0;
  int koRule;


  bool multiStoneSuicideLegal;

  float komi;
  //Min and max acceptable komi in various places involving user input validation
  static constexpr float MIN_USER_KOMI = -750.0f;
  static constexpr float MAX_USER_KOMI = 750.0f;

  Rules();
  Rules(
    int koRule,
    bool multiStoneSuicideLegal,
    float komi
  );
  ~Rules();

  bool operator==(const Rules& other) const;
  bool operator!=(const Rules& other) const;

  bool equalsIgnoringKomi(const Rules& other) const;
  bool gameResultWillBeInteger() const;

  static Rules getTrompTaylorish();

  static std::set<std::string> koRuleStrings();
  static std::set<std::string> scoringRuleStrings();
  static int parseKoRule(const std::string& s);
  static std::string writeKoRule(int koRule);

  static bool komiIsIntOrHalfInt(float komi);

  static Rules parseRules(const std::string& str);
  static bool tryParseRules(const std::string& str, Rules& buf);

  static Rules updateRules(const std::string& key, const std::string& value, Rules priorRules);

  friend std::ostream& operator<<(std::ostream& out, const Rules& rules);
  std::string toString() const;
  std::string toJsonString() const;
  nlohmann::json toJson() const;

  static const Hash128 ZOBRIST_KO_RULE_HASH[4];
  static const Hash128 ZOBRIST_MULTI_STONE_SUICIDE_HASH;

};

#endif  // GAME_RULES_H_
