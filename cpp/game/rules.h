#ifndef GAME_RULES_H_
#define GAME_RULES_H_

#include "../core/global.h"
#include "../core/hash.h"

#include "../external/nlohmann_json/json.hpp"

struct Rules {

  static const int BASICRULE_FREESTYLE = 0;
  static const int BASICRULE_STANDARD = 1;
  int basicRule;

  static const int VCNRULE_NOVC = 0;
  static const int VCNRULE_VC1_B = 1;
  static const int VCNRULE_VC2_B = 2;
  static const int VCNRULE_VC3_B = 3;
  static const int VCNRULE_VC4_B = 4;
  static const int VCNRULE_VC1_W = 11;
  static const int VCNRULE_VC2_W = 12;
  static const int VCNRULE_VC3_W = 13;
  static const int VCNRULE_VC4_W = 14;
  int VCNRule;

  bool firstPassWin;//和棋的时候，先pass的获胜

  int maxMoves;//达到最大步数直接和棋，0不限制


  float komi;
  //Min and max acceptable komi in various places involving user input validation
  static constexpr float MIN_USER_KOMI = -150.0f;
  static constexpr float MAX_USER_KOMI = 150.0f;

  Rules();
  Rules(
    int basicRule,
    int VCNRule,
    bool firstPassWin,
    int maxMoves,
    float komi
  );
  ~Rules();

  bool operator==(const Rules& other) const;
  bool operator!=(const Rules& other) const;

  static std::set<std::string> basicRuleStrings();
  static std::set<std::string> VCNRuleStrings();

  static Rules getTrompTaylorish();

  static int parseBasicRule(std::string s);
  static std::string writeBasicRule(int basicRule);

  static int parseVCNRule(std::string s);
  static std::string writeVCNRule(int VCNRule);

  static bool komiIsIntOrHalfInt(float komi);

  static Rules parseRules(const std::string& str);
  static Rules parseRulesWithoutKomi(const std::string& str, float komi);
  static bool tryParseRules(const std::string& str, Rules& buf);
  static bool tryParseRulesWithoutKomi(const std::string& str, Rules& buf, float komi);

  static Rules updateRules(const std::string& key, const std::string& value, Rules priorRules);

  Color vcSide() const;
  int vcLevel() const;



  friend std::ostream& operator<<(std::ostream& out, const Rules& rules);
  std::string toString() const;
  std::string toJsonString() const;
  std::string toJsonStringNoKomi() const;
  std::string toJsonStringNoKomiMaybeOmitStuff() const;
  nlohmann::json toJson() const;
  nlohmann::json toJsonNoKomi() const;
  nlohmann::json toJsonNoKomiMaybeOmitStuff() const;

  static const Hash128 ZOBRIST_BASIC_RULE_HASH[3];
  static const Hash128 ZOBRIST_VCNRULE_HASH_BASE;
  static const Hash128 ZOBRIST_FIRSTPASSWIN_HASH;
  static const Hash128 ZOBRIST_MAXMOVES_HASH_BASE;
  static const Hash128 ZOBRIST_PASSNUM_B_HASH_BASE;
  static const Hash128 ZOBRIST_PASSNUM_W_HASH_BASE;

private:
  nlohmann::json toJsonHelper(bool omitKomi, bool omitDefaults) const;
};

#endif  // GAME_RULES_H_
