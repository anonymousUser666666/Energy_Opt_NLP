#pragma once

#include <iostream>
#include <vector>
#include <fstream>
#include <math.h>
#include <filesystem>

#include <CppUnitLite/TestHarness.h>

using namespace std;

class TaskPeriodic
{
public:
    // Member list
    int offset;
    int period;
    int overhead;
    int executionTime;
    int deadline;

    // initializer

    TaskPeriodic() : offset(0), period(0),
                     overhead(0), executionTime(0),
                     deadline(0) {}
    TaskPeriodic(int offset, int period, int overhead, int executionTime,
                 int deadline) : offset(offset), period(period),
                                 overhead(overhead), executionTime(executionTime),
                                 deadline(deadline) {}

    TaskPeriodic(vector<int> dataInLine)
    {
        if (dataInLine.size() != 5)
        {
            cout << "The length of dataInLine in TaskPeriodic constructor is wrong! Must be 5!\n";
            throw;
        }

        offset = dataInLine[0];
        period = dataInLine[1];
        overhead = dataInLine[2];
        executionTime = dataInLine[3];
        deadline = dataInLine[4];
    }

    void print()
    {
        cout << "The period is: " << period << " The executionTime is " << executionTime << " The deadline is "
             << deadline << " The overhead is " << overhead << " The offset is " << offset << endl;
    }

    float utilization()
    {
        return float(executionTime) / period;
    }

    int slack()
    {
        return deadline - executionTime;
    }
};

class TaskSet
{
public:
    // member list
    vector<TaskPeriodic> tasks;

    // method list
    // TaskSet()  { vector<TaskPeriodic> tasks; }
    TaskSet(vector<TaskPeriodic> tasks) : tasks(tasks) {}

    int size()
    {
        return tasks.size();
    }

    vector<int> GetParameter(string parameterType)
    {
        uint N = size();
        vector<int> parameterList;
        parameterList.reserve(N);

        for (uint i = 0; i < N; i++)
        {
            if (parameterType == "period")
                parameterList.push_back(tasks[i].period);
            else if (parameterType == "executionTime")
                parameterList.push_back(tasks[i].executionTime);
            else if (parameterType == "overhead")
                parameterList.push_back(tasks[i].overhead);
            else if (parameterType == "deadline")
                parameterList.push_back(tasks[i].deadline);
            else if (parameterType == "offset")
                parameterList.push_back(tasks[i].offset);
            else
                throw "parameterType in GetParameter is not recognized!\n";
        }
        return parameterList;
    }

    // some helper function for Reorder
    static bool comparePeriod(TaskPeriodic task1, TaskPeriodic task2)
    {
        return task1.period < task2.period;
    };

    void Reorder(string priorityType)
    {
        if (priorityType == "RM")
        {
            sort(tasks.begin(), tasks.end(), comparePeriod);
        }
        else if (priorityType == "orig")
        {
            return;
        }
        else
        {
            throw "Unrecognized priorityType in Reorder!\n";
        }
    }
};

TaskSet ReadTaskSet(string path, string priorityType = "RM")
{
    // some default parameters in this function
    string delimiter = ",";
    string token;
    string line;
    size_t pos = 0;

    vector<TaskPeriodic> taskSet;

    fstream file;
    file.open(path, ios::in);
    if (file.is_open())
    {
        string line;
        while (getline(file, line))
        {
            if (!(line[0] >= '0' && line[0] <= '9'))
                continue;
            vector<int> dataInLine;
            while ((pos = line.find(delimiter)) != string::npos)
            {
                token = line.substr(0, pos);
                int temp = atoi(token.c_str());
                dataInLine.push_back(temp);
                line.erase(0, pos + delimiter.length());
            }
            dataInLine.push_back(atoi(line.c_str()));
            dataInLine.erase(dataInLine.begin());
            taskSet.push_back(TaskPeriodic(dataInLine));
        }
        TaskSet ttt(taskSet);
        ttt.Reorder(priorityType);
        return ttt;
    }
    else
    {
        cout << "The path does not exist in ReadTaskSet!\n";
        throw;
    }
}
