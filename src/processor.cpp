#include "processor.h"

#include <algorithm>

#include "process.h"

using std::vector;

float Processor::Utilization() { return LinuxParser::CpuUtilization(); }

std::vector<Process> Processor::Processes() {
  vector<Process> processes = {};
  std::vector<int> pids = LinuxParser::Pids();

  // Sort descending
  auto compare = [](const Process a, const Process b) { return b < a; };

  for (int id : pids) {
    processes.push_back(Process(id));
  }

  sort(processes.begin(), processes.end(), compare);

  return processes;
}
