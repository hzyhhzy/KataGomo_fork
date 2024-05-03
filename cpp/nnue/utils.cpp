#include "NNUEglobal.h"
using namespace std;
namespace NNUE::strOp {

string trim(const string &s)
{
  size_t p2 = s.find_last_not_of(" \t\r\n");
  if (p2 == string::npos)
    return string();
  size_t p1 = s.find_first_not_of(" \t\r\n");
  if (p1 == string::npos)
    p1 = 0;

  return s.substr(p1, (p2 - p1) + 1);
}

vector<string> split(const string &s)
{
  istringstream  in(s);
  string         token;
  vector<string> tokens;
  while (in >> token) {
    token = trim(token);
    tokens.push_back(token);
  }
  return tokens;
}
vector<string> split(const string &s, char delim)
{
  istringstream  in(s);
  string         token;
  vector<string> tokens;
  while (getline(in, token, delim))
    tokens.push_back(token);
  return tokens;
}

bool tryStringToInt(const string &str, int &x)
{
  int           val = 0;
  istringstream in(trim(str));
  in >> val;
  if (in.fail() || in.peek() != EOF)
    return false;
  x = val;
  return true;
}
}  // namespace strOp
