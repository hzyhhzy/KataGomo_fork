#ifndef GAME_RULES_H_
#define GAME_RULES_H_

#include "../core/global.h"
#include "../core/hash.h"

#include "../external/nlohmann_json/json.hpp"

struct Rules {

  // it's better to change the names 
  // but it's not necessary
  static const int XXXRULE0 = 0;
  static const int XXXRULE1 = 1;  
  static const int XXXRULE2 = 2;  
  static const int XXXRULE3 = 3;  
  int xxxRule; 

  int komi; //some integer rule



  Rules();
  Rules(
    int xxxRule,
    int komi
  );
  ~Rules();

  bool operator==(const Rules& other) const;
  bool operator!=(const Rules& other) const;


  static Rules getTrompTaylorish();

  static std::map<std::string, int> Rules::xxxRuleStringsMap();
  static std::set<std::string> Rules::xxxRuleStrings();
  static int parsexxxRule(const std::string& s);
  static std::string writexxxRule(int xxxRule);


  static Rules parseRules(const std::string& str);
  static bool tryParseRules(const std::string& str, Rules& buf);

  static Rules updateRules(const std::string& key, const std::string& value, Rules priorRules);

  friend std::ostream& operator<<(std::ostream& out, const Rules& rules);
  std::string toString() const;
  std::string toStringMaybeNice() const;
  std::string toJsonString() const;
  nlohmann::json toJson() const;

  static const Hash128 ZOBRIST_XXX_RULE_HASH[4];
  static const Hash128 ZOBRIST_KOMI_RULE_HASH_BASE;

};

#endif  // GAME_RULES_H_
