#ifndef SEARCH_REPORTEDSEARCHVALUES_H_
#define SEARCH_REPORTEDSEARCHVALUES_H_

#include "../core/global.h"

struct Search;

struct ReportedSearchValues {
  double winValue;
  double lossValue;
  double noResultValue;
  double winLossValue;
  double utility;
  double weight;
  int64_t visits;

  ReportedSearchValues();
  ReportedSearchValues(
    const Search& search,
    double winLossValueAvg,
    double noResultValueAvg,
    double utilityAvg,
    double totalWeight,
    int64_t totalVisits
  );
  ~ReportedSearchValues();

  friend std::ostream& operator<<(std::ostream& out, const ReportedSearchValues& values);
};

#endif
