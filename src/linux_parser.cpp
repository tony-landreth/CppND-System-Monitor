#include "linux_parser.h"

#include <dirent.h>
#include <unistd.h>

#include <iostream>
#include <string>
#include <vector>

using std::stof;
using std::string;
using std::to_string;
using std::vector;

string LinuxParser::OperatingSystem() {
  string line;
  string key;
  string value;
  std::ifstream filestream(kOSPath);
  if (filestream.is_open()) {
    while (std::getline(filestream, line)) {
      std::replace(line.begin(), line.end(), ' ', '_');
      std::replace(line.begin(), line.end(), '=', ' ');
      std::replace(line.begin(), line.end(), '"', ' ');
      std::istringstream linestream(line);
      while (linestream >> key >> value) {
        if (key == "PRETTY_NAME") {
          std::replace(value.begin(), value.end(), '_', ' ');
          return value;
        }
      }
    }
  }
  return value;
}

string LinuxParser::Kernel() {
  string os, label, kernel;
  string line;
  std::ifstream stream(kProcDirectory + kVersionFilename);
  if (stream.is_open()) {
    std::getline(stream, line);
    std::istringstream linestream(line);
    linestream >> os >> label >> kernel;
  }
  return kernel;
}

// BONUS: Update this to use std::filesystem
vector<int> LinuxParser::Pids() {
  vector<int> pids;
  DIR* directory = opendir(kProcDirectory.c_str());
  struct dirent* file;
  while ((file = readdir(directory)) != nullptr) {
    // Is this a directory?
    if (file->d_type == DT_DIR) {
      // Is every character of the name a digit?
      string filename(file->d_name);
      if (std::all_of(filename.begin(), filename.end(), isdigit)) {
        int pid = stoi(filename);
        pids.push_back(pid);
      }
    }
  }
  closedir(directory);
  return pids;
}

float LinuxParser::MemoryUtilization() {
  string line, key, value;
  float mem_total = 0.0;
  float mem_free = 0.0;

  std::ifstream filestream(kProcDirectory + kMeminfoFilename);

  if (filestream.is_open()) {
    while (std::getline(filestream, line)) {
      std::istringstream linestream(line);
      linestream >> key >> value;

      if (key == "MemTotal:") {
        mem_total = stof(value);
      }
      if (key == "MemFree:") {
        mem_free = stof(value);
      }
    }
  }
  return (mem_total - mem_free) / mem_total;
}

long int LinuxParser::UpTime() {
  string uptime, line;
  long int value;
  std::ifstream stream(kProcDirectory + kUptimeFilename);

  if (stream.is_open()) {
    std::getline(stream, line);
    std::istringstream linestream(line);
    linestream >> uptime;
    value = stol(uptime);
  }
  return value;
}

long LinuxParser::Jiffies() {
  return LinuxParser::ActiveJiffies() + LinuxParser::IdleJiffies();
}

std::istringstream LinuxParser::StringStreamFromStatFile(string file_path,
                                                         string key) {
  string line, k;
  std::ifstream filestream(file_path);

  if (filestream.is_open()) {
    while (std::getline(filestream, line)) {
      std::istringstream linestream(line);

      // Find the line
      while (linestream >> k) {
        if (k == key) {
          return linestream;
        }
      }
    }
  }
  return std::istringstream();
}

long LinuxParser::ActiveJiffies() {
  string line, key, val;
  int long j_sum = 0;
  std::istringstream linestream = LinuxParser::StringStreamFromStatFile(
      kProcDirectory + kStatFilename, "cpu");

  int i = 0;
  while (linestream >> val) {
    if (i >= kUser_ && i <= kSystem_) {
      j_sum += stol(val);
    }

    if (i >= kIRQ_ && i <= kSteal_) {
      j_sum += stol(val);
    }

    i++;
  }

  return j_sum;
}

long LinuxParser::IdleJiffies() {
  string line, key, val;
  int long idle_jiffies = 0;

  std::istringstream linestream = LinuxParser::StringStreamFromStatFile(
      kProcDirectory + kStatFilename, "cpu");

  int i = 0;
  while (linestream >> val) {
    if (i == kIdle_ || i == kIOwait_) {
      idle_jiffies += stol(val);
    }
    i++;
  }

  return idle_jiffies;
}

float LinuxParser::CpuUtilization() {
  float active_t1 = LinuxParser::ActiveJiffies();
  float total_t1 = LinuxParser::Jiffies();

  usleep(1000000);

  float active_t2 = LinuxParser::ActiveJiffies();
  float total_t2 = LinuxParser::Jiffies();

  float delta_active = active_t2 - active_t1;
  float delta_total = total_t2 - total_t1;

  if(delta_total == 0.0) {
    return 0.0;
  }

  return delta_active / delta_total;
}

float LinuxParser::CpuUtilization(int pid) {
  string v;
  int uptime = LinuxParser::UpTime();
  int i = 0, total_time = 0, start_time = 0;

  std::istringstream linestream =
      LinuxParser::StringStreamFromStatFile(kProcDirectory + to_string(pid) + kStatFilename, to_string(pid));

  while (linestream >> v) {
    if(i >= kUTime_ && i <= kCSTime_) {
      total_time += stoi(v);
    }

    if(i == kStartTime_) {
      start_time = stoi(v);
    }
    i++;
  }

  // Ref: https://stackoverflow.com/questions/16726779/how-do-i-get-the-total-cpu-usage-of-an-application-from-proc-pid-stat/16736599#16736599
  int Hertz = sysconf(_SC_CLK_TCK);
  float seconds = uptime - (start_time / Hertz);
  float cpu_usage = (total_time / Hertz) / seconds;

  return cpu_usage;
}

int LinuxParser::TotalProcesses() {
  string total;
  std::istringstream linestream = LinuxParser::StringStreamFromStatFile(
      kProcDirectory + kStatFilename, "processes");
  linestream >> total;
  return stoi(total);
}

int LinuxParser::RunningProcesses() {
  string value;
  std::istringstream linestream = LinuxParser::StringStreamFromStatFile(
      kProcDirectory + kStatFilename, "procs_running");
  linestream >> value;
  return stoi(value);
}

string LinuxParser::Command(int pid) {
  string line, comm;
  std::ifstream stream(kProcDirectory + to_string(pid) + kCommFilename);

  if (stream.is_open()) {
    std::getline(stream, line);
    std::istringstream linestream(line);
    linestream >> comm;
  }

  return line;
}

string LinuxParser::Ram(int pid) {
  string line, key, kbs;
  string ram = "0.0";

  std::ifstream filestream(kProcDirectory + to_string(pid) + kStatusFilename);

  if (filestream.is_open()) {
    while (std::getline(filestream, line)) {
      std::istringstream linestream(line);

      while (linestream >> key >> kbs) {
        if (key == "VmSize:") {
          ram = to_string(stoi(kbs) / 1000);
        }
      }
    }
  }
  return ram;
}

string LinuxParser::Uid(int pid) {
  string line, key, uid, value;
  std::ifstream filestream(kProcDirectory + to_string(pid) + kStatusFilename);

  if (filestream.is_open()) {
    while (std::getline(filestream, line)) {
      std::istringstream linestream(line);

      while (linestream >> key >> uid) {
        if (key == "Uid:") {
          value = uid;
        }
      }
    }
  }
  return value;
}

string LinuxParser::User(int pid) {
  string line, user, x, uid, num, value;
  std::ifstream filestream(kPasswordPath);

  uid = LinuxParser::Uid(pid);

  if (filestream.is_open()) {
    while (std::getline(filestream, line)) {
      std::replace(line.begin(), line.end(), ':', ' ');
      std::istringstream linestream(line);

      while (linestream >> user >> x >> num) {
        if (num == uid) {
          value = user;
        }
      }
    }
  }
  return value;
}

long LinuxParser::UpTime(int pid) {
  string v;
  string file_path = kProcDirectory + to_string(pid) + kStatFilename;
  int long value = 0;

  std::istringstream linestream =
      LinuxParser::StringStreamFromStatFile(file_path, to_string(pid));

  int i = 0;
  while (linestream >> v) {
    if (i == kStartTime_) {
      return stol(v) / sysconf(_SC_CLK_TCK);
    }
    i++;
  }

  return value;
}
