#include "simulation.h"
#include "scenario.h"
#include "bench_runner.h"
#include <iostream>
#include <fstream>
#include <vector>
int main() {

    // 1. 設定 benchmark
    benchrunner::BenchmarkConfig config;
    // 2. 建立 CSV writer
    benchrunner::BenchmarkCSVWriter writer("benchmark.csv");
    // 3. 建立 benchmark runner
    benchrunner::BenchmarkRunner runner(config, writer);
    // 4. 執行所有實驗
    runner.run();
    // 5. 完成
    std::cout << "Benchmark finished.\n";

    return 0;
}