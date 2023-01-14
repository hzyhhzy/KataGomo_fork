#include "core/global.h"

namespace MainCmds {
  int analysis(const std::vector<std::string>& args);
  int benchmark(const std::vector<std::string>& args);
  int contribute(const std::vector<std::string>& args);
  int evalsgf(const std::vector<std::string>& args);
  int gatekeeper(const std::vector<std::string>& args);
  int genconfig(const std::vector<std::string>& args, const std::string& firstCommand);
  int gtp(const std::vector<std::string>& args);
  int tuner(const std::vector<std::string>& args);
  int match(const std::vector<std::string>& args);
  int matchauto(const std::vector<std::string>& args);
  int selfplay(const std::vector<std::string>& args);

  int testgpuerror(const std::vector<std::string>& args);


  int samplesgfs(const std::vector<std::string>& args);
  int dataminesgfs(const std::vector<std::string>& args);
  int genbook(const std::vector<std::string>& args);
  int checkbook(const std::vector<std::string>& args);

  int trystartposes(const std::vector<std::string>& args);
  int viewstartposes(const std::vector<std::string>& args);

  int demoplay(const std::vector<std::string>& args);
  int printclockinfo(const std::vector<std::string>& args);
  int sampleinitializations(const std::vector<std::string>& args);

  int sandbox();
}

namespace Version {
  std::string getKataGoVersion();
  std::string getKataGoVersionForHelp();
  std::string getKataGoVersionFullInfo();
  std::string getGitRevision();
  std::string getGitRevisionWithBackend();
}
