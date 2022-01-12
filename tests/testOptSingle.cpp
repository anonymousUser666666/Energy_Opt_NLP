#include <chrono>

#include <CppUnitLite/TestHarness.h>
#include "../sources/Parameters.h"
#include "../sources/Optimize.h"
#include "../sources/RTA_LL.h"
#include "../sources/RTA_WAP.h"
#include "../sources/Generate_WAP.h"
using namespace std::chrono;
using Opt_LL = Energy_Opt<Task, RTA_LL>;
using Opt_WAP = Energy_Opt<Task, RTA_WAP>;

TEST(OptimizeTaskSet, RTA_LL_V1)
{
    // string path = "/home/zephyr/Programming/Energy_Opt_NLP/TaskData/test_n3_v4.csv";
    string path = "/home/zephyr/Programming/Energy_Opt_NLP/TaskData/" + testDataSetName + ".csv";
    // string path = "/home/zephyr/Programming/Energy_Opt_NLP/TaskData/test_data_N5_v2.csv";
    // string path = "/home/zephyr/Programming/Energy_Opt_NLP/TaskData/test_n10_v2.csv";
    // string path = "/home/zephyr/Programming/Energy_Opt_NLP/TaskData/test_n20_v1.csv";

    TaskSet taskSet1 = ReadTaskSet(path, readTaskMode);
    TaskSetNormal tasksN(taskSet1);
    InitializeGlobalVector(taskSet1.size());
    auto start = chrono::high_resolution_clock::now();
    double res = Energy_Opt<TaskSetNormal, RTA_LL>::OptimizeTaskSet(tasksN);
    cout << blue << "The energy saving ratio is " << res << def << endl;
    auto stop = chrono::high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(stop - start);
    cout << "The time taken is: " << double(duration.count()) / 1e6 << "seconds" << endl;
}

int main()
{
    TestResult tr;
    return TestRegistry::runAllTests(tr);
}
