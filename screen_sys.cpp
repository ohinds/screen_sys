// screen_sys.cpp
// oliver hinds <ohinds@gmail.com>
//
// usage: screen_sys <mode>
// modes: bat, cpu, mem, tmp

// TODO
// - allow other stations than KBOS
// - improve swap between modes
// - add switch between C and F

#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <unistd.h>

using namespace std;

class Vmstat {
public:
  typedef map<string, unsigned long long> dict_t;

  Vmstat() {
    FILE* fp = popen("vmstat -s", "r");
    if (!fp) return;
    char buf[512];
    while (fgets(buf, sizeof(buf), fp)) {
      string line(buf);
      if (!line.empty() && line.back() == '\n') line.pop_back();
      parse_line(line);
    }
    pclose(fp);
    for (const auto& p : data_)
      cout << p.first << endl;
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

string read_file(const string& file) {
  ifstream in(file.c_str());
  string ret;
  while (in.good()) {
    string line;
    getline(in, line);
    ret += line;
  }
  return ret;
}

void show_battery() {
  float now;
  istringstream(read_file("/sys/class/power_supply/BAT0/charge_now")) >> now;

  float full;
  istringstream(read_file("/sys/class/power_supply/BAT0/charge_full")) >> full;

  bool online;
  istringstream(read_file("/sys/class/power_supply/AC/online")) >> online;

  cout.width(3);
  cout << 100 * now / full
       << (online ? "+" : "-") << endl;
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
  unsigned long long free = data.at("free memory");

  float percentage = 100 * used / total;

  cout.width(5);
  cout.setf(ios::fixed | ios::right);
  cout.precision(0);
  cout << 100 * static_cast<float>(used) / total << endl;
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
    cout << buf;
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

    Vmstat vmstat;

    if(mode == "bat") {
      show_battery();
    }
    else if(mode == "cpu") {
      show_cpu(vmstat);
    }
    else if(mode == "mem") {
      show_mem(vmstat);
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
