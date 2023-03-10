#ifndef GAME_RULES_H_
#define GAME_RULES_H_

#include "../core/global.h"
#include "../core/hash.h"

#include "../external/nlohmann_json/json.hpp"

struct Rules {

  

  // LOOPDRAW: if the situation repeats, draw
  // LOOPLOSE: who make the situation repeats is a lose
  // LOOPSCORING: if the situation repeats, all empty locations belong to opponent and count stones
  // PASSSCORING: if one player have no legal moves, all empty locations belong to opponent and count score
  // PASSCONTINUE: if one player have no legal moves, pass and let opponent continue playing. this is only different from PASS_SCORING when using LOOP_DRAW rule on some rare conditions

  static const int LOOPDRAW_PASSSCORING = 0;
  static const int LOOPDRAW_PASSCONTINUE = 1;  
  static const int LOOPLOSE_PASSSCORING = 2;  
  static const int LOOPSCORING_PASSSCORING = 3;  
  int loopPassRule;

  int komi; //non-integer komi is meaningless



  Rules();
  Rules(
    int loopPassRule,
    int komi
  );
  ~Rules();

  bool operator==(const Rules& other) const;
  bool operator!=(const Rules& other) const;


  static Rules getTrompTaylorish();

  static std::map<std::string, int> Rules::loopPassRuleStringsMap();
  static std::set<std::string> Rules::loopPassRuleStrings();
  static int parseLoopPassRule(const std::string& s);
  static std::string writeLoopPassRule(int scoringRule);


  static Rules parseRules(const std::string& str);
  static bool tryParseRules(const std::string& str, Rules& buf);

  static Rules updateRules(const std::string& key, const std::string& value, Rules priorRules);

  friend std::ostream& operator<<(std::ostream& out, const Rules& rules);
  std::string toString() const;
  std::string toStringMaybeNice() const;
  std::string toJsonString() const;
  nlohmann::json toJson() const;

  static const Hash128 ZOBRIST_LOOPPASS_RULE_HASH[4];
  static const Hash128 ZOBRIST_KOMI_RULE_HASH_BASE;

};

#endif  // GAME_RULES_H_
