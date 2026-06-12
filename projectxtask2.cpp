#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <queue>
#include <deque>
#include <string>
using namespace std;

struct Task {
    string id;
    int burstTime;
    int remainingTime;
    vector<string> memoryBlocks;
    int memoryIndex;

    Task(string tid, int burst, vector<string> mem)
        : id(tid),
          burstTime(burst),
          remainingTime(burst),
          memoryBlocks(mem),
          memoryIndex(0) {}
};

class CacheLevel {
public:
    string name;
    int capacity;
    int latency;
    deque<string> blocks;

    CacheLevel(string n, int c, int l)
        : name(n), capacity(c), latency(l) {}

    bool contains(const string& block) {
        for (const auto& b : blocks) {
            if (b == block) return true;
        }
        return false;
    }

    string insertFIFO(const string& block) {
        string evicted = "";

        if ((int)blocks.size() >= capacity) {
            evicted = blocks.front();
            blocks.pop_front();
        }

        blocks.push_back(block);
        return evicted;
    }

    string state() const {
        string s = "[";
        for (size_t i = 0; i < blocks.size(); i++) {
            s += blocks[i];
            if (i + 1 < blocks.size()) s += ", ";
        }
        s += "]";
        return s;
    }
};

class CacheHierarchy {
private:
    CacheLevel L1;
    CacheLevel L2;
    CacheLevel L3;

public:
    long long totalLatency;
    long long ramAccesses;

    CacheHierarchy()
        : L1("L1", 32, 4),
          L2("L2", 128, 12),
          L3("L3", 512, 40),
          totalLatency(0),
          ramAccesses(0) {}

    int accessBlock(const string& block) {
        cout << "Requesting: " << block << endl;

        if (L1.contains(block)) {
            cout << "  L1 HIT (" << L1.latency << " cycles)" << endl;
            totalLatency += L1.latency;
            return L1.latency;
        }

        cout << "  L1 MISS" << endl;

        if (L2.contains(block)) {
            cout << "  L2 HIT (" << L2.latency << " cycles) -> Promote to L1" << endl;

            string evicted = L1.insertFIFO(block);
            if (!evicted.empty()) {
                cout << "  L1 Evicted: " << evicted << endl;
            }

            totalLatency += L2.latency;
            return L2.latency;
        }

        cout << "  L2 MISS" << endl;

        if (L3.contains(block)) {
            cout << "  L3 HIT (" << L3.latency << " cycles) -> Promote to L1" << endl;

            string evicted = L1.insertFIFO(block);
            if (!evicted.empty()) {
                cout << "  L1 Evicted: " << evicted << endl;
            }

            totalLatency += L3.latency;
            return L3.latency;
        }

        cout << "  L3 MISS" << endl;
        cout << "  RAM HIT (200 cycles) -> Load into L1" << endl;

        ramAccesses++;

        string evicted = L1.insertFIFO(block);
        if (!evicted.empty()) {
            cout << "  L1 Evicted: " << evicted << endl;
        }

        totalLatency += 200;
        return 200;
    }

    void printState() {
        cout << "  L1: " << L1.state() << endl;
        cout << "  L2: " << L2.state() << endl;
        cout << "  L3: " << L3.state() << endl;
    }

    void preloadExampleData() {
        L1.blocks.push_back("M2");
        L1.blocks.push_back("M5");
        L1.blocks.push_back("M1");

        L2.blocks.push_back("M3");
        L2.blocks.push_back("M7");

        L3.blocks.push_back("M6");
    }
};

vector<Task> loadTasks(const string& filename) {
    vector<Task> tasks;

    ifstream fin(filename);

    if (!fin.is_open()) {
        cerr << "Failed to open file: " << filename << endl;
        return tasks;
    }

    string line;

    while (getline(fin, line)) {
        if (line.empty()) continue;

        stringstream ss(line);

        string taskKeyword;
        string taskId;
        string burstKeyword;
        int burst;

        ss >> taskKeyword >> taskId >> burstKeyword >> burst;

        string memKeyword;
        ss >> memKeyword;

        vector<string> memBlocks;
        string block;

        while (ss >> block) {
            memBlocks.push_back(block);
        }

        tasks.emplace_back(taskId, burst, memBlocks);
    }

    return tasks;
}

int main(int argc, char* argv[]) {

    string inputFile = "tasks.txt";

    if (argc > 1) {
        inputFile = argv[1];
    }

    vector<Task> tasks = loadTasks(inputFile);

    if (tasks.empty()) {
        cout << "No tasks loaded." << endl;
        return 0;
    }

    const int QUANTUM = 3;

    queue<int> readyQueue;

    for (int i = 0; i < (int)tasks.size(); i++) {
        readyQueue.push(i);
    }

    CacheHierarchy cache;
    cache.preloadExampleData();

    long long cpuCycles = 0;
    int completedTasks = 0;
    int cycleNumber = 1;

    cout << "CPU + CACHE SIMULATOR" << endl;
    cout << "Scheduler: Round Robin (Quantum = 3)" << endl;
    cout << endl;

    while (!readyQueue.empty()) {

        int idx = readyQueue.front();
        readyQueue.pop();

        Task& task = tasks[idx];

        int slice = min(QUANTUM, task.remainingTime);

        cout << "Dispatching Task " << task.id
             << " for " << slice
             << " cycle(s)" << endl;

        for (int i = 0; i < slice; i++) {

            cout << "\nCycle " << cycleNumber << " - Running: " << task.id << endl;

            if (!task.memoryBlocks.empty()) {

                string block =
                    task.memoryBlocks[task.memoryIndex];

                cache.accessBlock(block);

                task.memoryIndex =
                    (task.memoryIndex + 1) % task.memoryBlocks.size();
            }

            cache.printState();

            task.remainingTime--;
            cpuCycles++;
            cycleNumber++;

            if (task.remainingTime == 0)
                break;
        }

        if (task.remainingTime > 0) {
            readyQueue.push(idx);
        } else {
            completedTasks++;
            cout << "\nTask " << task.id << " completed." << endl;
        }
    }

    cout << "Total CPU Cycles: " << cpuCycles << endl;
    cout << "Cache Latency Cycles: " << cache.totalLatency << endl;
    cout << "Overall Cycles: " << cpuCycles + cache.totalLatency << endl;
    cout << "Tasks Completed: " << completedTasks << endl;
    cout << "Scheduler: Round Robin (Q=3)" << endl;
    cout << "RAM Accesses: " << cache.ramAccesses << endl;

    return 0;
}
