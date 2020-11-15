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
  string temp_file = temp_filename();

  int ret_val = system(("/usr/bin/free -m > " + temp_file).c_str());
  if(ret_val != 0) {
    cerr << "error running free" << endl;
    return;
  }

  string mem = read_and_unlink_file(temp_file);

  string total_search = "Mem:";
  size_t total_pos = mem.find(total_search) + total_search.length() + 1;
  unsigned long total;
  istringstream(mem.substr(total_pos)) >> total;

  string used_search = "-/+ buffers/cache:";
  size_t used_pos = mem.find(used_search) + used_search.length() + 1;
  unsigned long used;
  istringstream(mem.substr(used_pos)) >> used;

  cout.width(5);
  cout.setf(ios::fixed | ios::right);
  cout.precision(0);
  cout << 100 * static_cast<float>(used) / total << endl;
}

void show_temperature(const string& station) {
  string temp_file = temp_filename();
  string cmd = "curl -s "
    "\"https://geo.weather.gc.ca/geomet/?"
    "SERVICE=WMS&"
    "VERSION=1.3.0&"
    "REQUEST=GetFeatureInfo&"
    "QUERY_LAYERS=GDPS.ETA_TT&"
    "INFO_FORMAT=application/json&"
    "i=5&"
    "j=5&"
    "EXCEPTIONS=xml&"
    "LAYERS=GDPS.ETA_TT&"
    "CRS=EPSG:4326&"
    "BBOX=" + station + "&"
    "WIDTH=10&"
    "HEIGHT=10\""
    " > " + temp_file;
  int ret_val = system(cmd.c_str());

  if(ret_val != 0) {
    cerr << "error getting weather for station " << station << endl;
    cout << "ERR" << endl;
    return;
  }

  string metar = read_and_unlink_file(temp_file);
  string value_token = "\"value\": \"";
  size_t temp_pos = metar.find(value_token);
  if(temp_pos == string::npos) {
    cerr << "error searching for temperature in METAR data" << endl;
    cout << "ERR" << endl;
    return;
 }

  size_t quote_pos = metar.find("\"", temp_pos + value_token.length() + 1);
  if(quote_pos == string::npos) {
    cerr << "error parsing temperature in METAR data" << endl;
    cout << "ERR" << endl;
    return;
  }

  string temperature = metar.substr(temp_pos + value_token.length(), 8);
  float cels = atof(temperature.c_str());
  float fahr = cels * 9 / 5 + 32;
  
  cout.precision(0);
  cout.setf(ios::fixed | ios::right);
  cout.width(5);
  cout << fahr << endl;
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
      show_temperature("45.5,-73.6,45.6,-73.5");
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
