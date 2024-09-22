#ifndef GAME_RULES_H_
#define GAME_RULES_H_

#include "../core/global.h"
#include "../core/hash.h"

#include "../external/nlohmann_json/json.hpp"

struct Rules {

  
  static const int RULE_NOPASS = 0;
  static const int RULE_ALLOWPASS = 1;
  int basicRule;



  Rules();
  Rules(int basicRule
  );
  ~Rules();

  bool operator==(const Rules& other) const;
  bool operator!=(const Rules& other) const;

  bool equals(const Rules& other) const;
  bool gameResultWillBeInteger() const;

  static Rules getTrompTaylorish();

  static std::set<std::string> basicRuleStrings();
  static int parseBasicRule(const std::string& s);
  static std::string writeBasicRule(int basicRule);


  static Rules parseRules(const std::string& str);
  static bool tryParseRules(const std::string& str, Rules& buf);

  static Rules updateRules(const std::string& key, const std::string& value, Rules priorRules);

  friend std::ostream& operator<<(std::ostream& out, const Rules& rules);
  std::string toString() const;
  std::string toStringMaybeNice() const;
  std::string toJsonString() const;
  nlohmann::json toJson() const;

  static const Hash128 ZOBRIST_BASIC_RULE_HASH[2];

};

#endif  // GAME_RULES_H_
