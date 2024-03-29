// screen_sys.cpp
// oliver hinds <ohinds@gmail.com>
//
// usage: screen_sys <mode>
// modes: bat, cpu, mem, tmp

// TODO
// - allow other stations than KBOS
// - improve swap between modes
// - add switch between C and F

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>

using namespace std;

string temp_filename() {
  char name[] = "/tmp/screen_sys.XXXXXX";
  mkstemp(name);
  return string(name);
}

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

string read_and_unlink_file(const string& file) {
  string ret = read_file(file);
  unlink(file.c_str());
  return ret;
}

void show_battery() {
  float now;
  istringstream(read_file("/sys/class/power_supply/BAT0/energy_now")) >> now;

  float full;
  istringstream(read_file("/sys/class/power_supply/BAT0/energy_full")) >> full;

  bool online;
  istringstream(read_file("/sys/class/power_supply/AC/online")) >> online;

  cout.width(3);
  cout << 100 * now / full
       << (online ? "+" : "-") << endl;
}

void show_cpu() {
  static unsigned long long last_user = 0;
  static unsigned long long last_syst = 0;
  static unsigned long long last_nice = 0;
  static unsigned long long last_idle = 0;

  string trash;
  unsigned long long user;
  unsigned long long syst;
  unsigned long long nice;
  unsigned long long idle;

  istringstream ins(read_file("/proc/stat"));
  ins >> trash;
  ins >> user;
  ins >> syst;
  ins >> nice;
  ins >> idle;

  unsigned long long user_diff = user - last_user;
  unsigned long long syst_diff = syst - last_syst;
  unsigned long long nice_diff = nice - last_nice;
  unsigned long long idle_diff = idle - last_idle;


  float percentage = 100 *
      static_cast<float>(user_diff + syst_diff + nice_diff) /
      (user_diff + syst_diff + nice_diff + idle_diff);

  last_user = user;
  last_syst = syst;
  last_nice= nice;
  last_idle = idle;

  cout.width(5);
  cout.precision(1);
  cout << std::fixed << percentage << endl;
}

void show_mem() {
  string temp_file1 = temp_filename();
  string temp_file2 = temp_filename();

  int ret_val1 = system(("/usr/bin/free | grep '^Mem:' | awk '{print $2}' > " + temp_file1).c_str());
  if(ret_val1 != 0) {
    cerr << "error running free" << endl;
    return;
  }

  int ret_val2 = system(("/usr/bin/free | grep '^Mem:' | awk '{print $6}' > " + temp_file2).c_str());
  if(ret_val2 != 0) {
    cerr << "error running free" << endl;
    return;
  }

  string total_str = read_and_unlink_file(temp_file1);
  unsigned long total;
  istringstream(total_str) >> total;

  string used_str = read_and_unlink_file(temp_file2);
  unsigned long used;
  istringstream(used_str) >> used;

  cout.width(5);
  cout.setf(ios::fixed | ios::right);
  cout.precision(0);
  cout << 100 * static_cast<float>(used) / total << endl;
}

void show_temperature(const string& station) {
  string temp_file = "tmp"; //temp_filename();
  string cmd = "curl -s https://api.weather.gov/stations/" +
    station + "/observations/latest | grep -A 2 '\"temperature\"'"
    " | tail -1 | awk '{printf \"%.0f\\n\", 9 / 5 * $2 + 32}' > " + temp_file;
  int ret_val = system(cmd.c_str());

  if(ret_val != 0) {
    cerr << "error getting weather for station " << station << endl;
    cout << "ERR" << endl;
    return;
  }

  string metar = read_and_unlink_file(temp_file);
  cout << metar << endl;
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
      show_cpu();
    }
    else if(mode == "mem") {
      show_mem();
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
