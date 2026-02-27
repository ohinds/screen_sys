// screen_sys.cpp
// oliver hinds <ohinds@gmail.com>
//
// usage: screen_sys <mode>
// modes: bat, cpu, mem, tmp

// TODO
// - allow other stations than KBOS
// - improve swap between modes
// - add switch between C and F

#include <iostream>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <unistd.h>

using namespace std;

string run_command(const string &cmd) {
  string result;
  FILE* fp = popen(cmd.c_str(), "r");
  if (!fp) return "";
  char buf[512];
  while (fgets(buf, sizeof(buf), fp)) {
    string line(buf);
    if (!line.empty() && line.back() == '\n') line.pop_back();
    result += line + "\n";
  }
  pclose(fp);

  return result;
}

class Vmstat {
public:
  typedef map<string, unsigned long long> dict_t;

  Vmstat() {
    std::istringstream lines(run_command("vmstat -s"));

    string line;
    while (getline(lines, line)) {
      parse_line(line);
    }
  }

  const dict_t& data() const { return data_; }
  bool ok() const { return !data_.empty(); }

private:
  void parse_line(const string& line) {
    istringstream iss(line);
    unsigned long long val;
    if (!(iss >> val)) return;
    string token;
    iss >> token;
    // If token is a unit (K, KB, M, etc.), take the rest as key; else use token + rest as key
    string rest;
    getline(iss, rest);
    while (!rest.empty() && (rest[0] == ' ' || rest[0] == '\t')) rest.erase(0, 1);
    string key = token == "K" || token == "KB" || token == "M" || token == "MB" || token == "B"
        ? rest : (token + " " + rest);
    while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();
    if (!key.empty())
      data_[key] = val;
  }

  dict_t data_;
};

void show_battery() {
  string upower_result = run_command(
    "upower -i $(upower -e | grep '/battery') | grep --color=never -E 'state|to full|to empty|percentage'");

  regex state_re("state:\\s*([a-z]+)");
  smatch state_match;
  regex_search(upower_result, state_match, state_re);

  regex time_re("time to .*:\\s*([a-z0-9\\ \\.]+)");
  smatch time_match;
  regex_search(upower_result, time_match, time_re);

  regex pct_re("percentage:\\s*([0-9]+)\%");
  smatch pct_match;
  regex_search(upower_result, pct_match, pct_re);

  cout << (state_match[1] == "discharging" ? "-" : "+")
       << pct_match[1] << "% (" << time_match[1] << ")" << endl;
}

void show_cpu(const Vmstat& vmstat) {
  const auto& data = vmstat.data();
  unsigned long long user = data.at("non-nice user cpu ticks");
  unsigned long long syst = data.at("system cpu ticks");
  unsigned long long nice = data.at("nice user cpu ticks");
  unsigned long long idle = data.at("idle cpu ticks");

  float percentage = 100 * (user + syst + nice) / (user + syst + nice + idle);

  cout.width(5);
  cout.precision(1);
  cout << std::fixed << percentage << endl;
}

void show_mem(const Vmstat& vmstat) {
  const auto& data = vmstat.data();
  unsigned long long total = data.at("total memory");
  unsigned long long used = data.at("used memory");

  cout.width(5);
  cout.setf(ios::fixed | ios::right);
  cout.precision(0);
  cout << 100.f * used / total << endl;
}

void show_temperature(const string& station) {
  string cmd = "curl -s https://api.weather.gov/stations/" +
    station + "/observations/latest | grep -A 2 '\"temperature\"'"
    " | tail -1 | awk '{printf \"%.0f\\n\", 9 / 5 * $2 + 32}'";
  FILE* fp = popen(cmd.c_str(), "r");
  if (!fp) {
    cerr << "error getting weather for station " << station << endl;
    cout << "ERR" << endl;
    return;
  }
  char buf[64];
  if (fgets(buf, sizeof(buf), fp)) {
    cout << buf << endl;
  } else {
    cout << "ERR" << endl;
  }
  pclose(fp);
}

int main(int argc, char** argv) {

  if(argc < 2) {
    cout << "usage: screen_sys <mode> [refresh(s) or -1 to exit immediately]"
         << endl;
    return 1;
  }

  string mode(argv[1]);
  int refresh = 1;
  if(argc > 2) {
    istringstream(argv[2]) >> refresh;
  }

  while(true) {
    if(mode == "bat") {
      show_battery();
    }
    else if(mode == "cpu") {
      show_cpu(Vmstat());
    }
    else if(mode == "mem") {
      show_mem(Vmstat());
    }
    else if(mode == "tmp") {
      show_temperature("UCCC1");
    }
    else {
      cerr << "unknown mode " << mode << endl;
      break;
    }

    if(refresh == -1) {
      break;
    }

    sleep(refresh);
  }

  return 0;
}
