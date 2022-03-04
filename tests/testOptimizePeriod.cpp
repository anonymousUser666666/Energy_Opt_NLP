#include <chrono>

#include <CppUnitLite/TestHarness.h>
#include "../sources/Parameters.h"
#include "../sources/Optimize.h"
using namespace std::chrono;
using Opt_LL = Energy_Opt<TaskSetNormal, RTA_LL>;
class ControlFactorT : public NoiseModelFactor1<VectorDynamic>
{
public:
    TaskSet tasks_;
    VectorDynamic coeffVec_;
    int N;

    ControlFactorT(Key key, TaskSet &tasks, VectorDynamic coeffVec,
                   SharedNoiseModel model) : NoiseModelFactor1<VectorDynamic>(model, key),
                                             tasks_(tasks), coeffVec_(coeffVec)
    {
        N = tasks_.size();
    }
    // bool active(const Values &) const override { return true; }

    VectorDynamic EstimateControlAndSchedulability(const TaskSet &tasks) const
    {
        VectorDynamic resV = GenerateVectorDynamic(tasks.size() * 5);
        RTA_LL r(tasks);
        VectorDynamic rta = r.ResponseTimeOfTaskSet();
        double totalExecution = GetParameterVD<double>(tasks, "executionTime").sum();
        for (uint i = 0; i < tasks.size(); i++)
        {
            resV(5 * i) = pow(coeffVec_[2 * i] * tasks[i].period, 1);
            resV(5 * i + 1) = pow(coeffVec_[2 * i + 1] * rta(i), 1);
            resV(5 * i + 2) = max(0, tasks[i].period - totalExecution * 5) * weightEnergy;
            resV(5 * i + 3) = max(0, 0 - tasks[i].period) * weightEnergy;
            resV(5 * i + 4) = max(0, rta(i) - tasks[i].deadline) * weightEnergy;
            if (resV(5 * i + 2) > 1)
            {
                int a = 1;
            }
            if (resV(5 * i + 2) != 0 || resV(5 * i + 3) != 0 || resV(5 * i + 4) != 0)
            {
                int a = 1;
            }
        }
        return resV;
    }

    /**
         * @brief 
         * 
         * @param executionTimeVector (numberOfTasksNeedOptimize, 1)
         * @param H 
         * @return Vector 
         */
    Vector evaluateError(const VectorDynamic &executionTimeVector, boost::optional<Matrix &> H = boost::none) const override
    {
        TaskSet taskDurOpt = tasks_;

        boost::function<Matrix(const VectorDynamic &)> f2 =
            [this](const VectorDynamic &executionTimeVector)
        {
            TaskSet taskT = tasks_;
            UpdateTaskSetPeriod(taskT, executionTimeVector);
            return EstimateControlAndSchedulability(taskT);
        };

        // boost::function<Matrix(const VectorDynamic &)> f =
        //     [&taskDurOpt, &f2, this](const VectorDynamic &executionTimeVector)
        // {
        //     UpdateTaskSetExecutionTime(taskDurOpt.tasks_, executionTimeVector, lastTaskDoNotNeedOptimize);
        //     Schedul_Analysis r(taskDurOpt);
        //     VectorDynamic responseTimeVec = r.ResponseTimeOfTaskSet(responseTimeInitial);
        //     VectorDynamic err = f2(executionTimeVector);

        //     double currentEnergyConsumption = err.sum();
        //     for (int i = 0; i < N; i++)
        //     {
        //         // barrier function part
        //         err(i, 0) += Barrier(taskDurOpt.tasks_[i].deadline - responseTimeVec(i, 0));
        //         if (enableMaxComputationTimeRestrict)
        //             err(i, 0) += Barrier(taskDurOpt.tasks_[i].executionTimeOrg * MaxComputationTimeRestrict -
        //                                  taskDurOpt.tasks_[i].executionTime);
        //     }
        //     UpdateGlobalVector(responseTimeVec, currentEnergyConsumption, taskDurOpt.tasks_);
        //     return err;
        // };

        VectorDynamic err;
        err = f2(executionTimeVector);
        if (H)
        {
            // if (exactJacobian)
            //     *H = NumericalDerivativeDynamicUpper(f, executionTimeVector, deltaOptimizer, N);
            // else
            *H = NumericalDerivativeDynamicUpper(f2, executionTimeVector, deltaOptimizer, N * 5);
            // *H = NumericalDerivativeDynamic(f2, executionTimeVector, deltaOptimizer, numberOfTasksNeedOptimize);
            // *H = jacobian;
            if (debugMode == 1)
            {
                cout << endl;
                cout << "The current evaluation point is " << endl
                     << executionTimeVector << endl;
                cout << "The Jacobian is " << endl
                     << *H << endl;
                // cout << "The approximated Jacobian is " << endl
                //      << jacobian << endl;
                cout << "The current error is " << endl
                     << err << endl
                     << endl
                     << err.norm() << endl
                     << endl;
            }
        }

        return err;
    }
};

// class

TEST(period, error1)
{
    weightEnergy = 1;

    TaskSet tasks;
    tasks.push_back(Task{0, 635, 0, 2, 635, 0, 0});
    tasks.push_back(Task{0, 635, 0, 48, 635, 1, 0});
    tasks.push_back(Task{0, 635, 0, 18, 635, 2, 0});
    tasks.push_back(Task{0, 635, 0, 47, 635, 3, 0});
    tasks.push_back(Task{0, 635, 0, 12, 635, 4, 0});
    int N = tasks.size();
    auto model = noiseModel::Isotropic::Sigma(N * 5, noiseModelSigma);

    Symbol key('a', 0);
    VectorDynamic coeff;
    coeff.resize(10, 1);
    coeff << 645, 7143, 275, 9334, 217, 5031, 489, 3778, 285, 380;
    VectorDynamic initialEstimate = GenerateVectorDynamic(5);
    initialEstimate << 635, 635, 635, 635, 635;
    ControlFactorT factor1(key, tasks, coeff, model);
    cout << factor1.evaluateError(initialEstimate) << endl;
    AssertEqualScalar(2519309, factor1.evaluateError(initialEstimate).sum());
    initialEstimate << 634.9, 635, 635, 635, 635;
    AssertEqualScalar(2519244.5, factor1.evaluateError(initialEstimate).sum());
}
double realObj(TaskSet &tasks, VectorDynamic coeff)
{
    double res = 0;
    Symbol key('a', 0);
    auto model = noiseModel::Isotropic::Sigma(tasks.size() * 5, 1);
    ControlFactorT factor(key, tasks, coeff, model);
    return factor.evaluateError(GetParameterVD<double>(tasks, "period")).sum();
}
TEST(optimizeperiod1, v1)
{
    // weightEnergy = 1e8;
    TaskSet tasks;
    tasks.push_back(Task{0, 635, 0, 2, 635, 0, 0});
    tasks.push_back(Task{0, 635, 0, 48, 635, 1, 0});
    tasks.push_back(Task{0, 635, 0, 18, 635, 2, 0});
    tasks.push_back(Task{0, 635, 0, 47, 635, 3, 0});
    tasks.push_back(Task{0, 635, 0, 12, 635, 4, 0});
    int N = tasks.size();

    auto model = noiseModel::Isotropic::Sigma(N * 5, noiseModelSigma);
    NonlinearFactorGraph graph;
    Symbol key('a', 0);
    VectorDynamic coeff;
    coeff.resize(10, 1);
    coeff << 645, 7143, 275, 9334, 217, 5031, 489, 3778, 285, 380;
    VectorDynamic initialEstimate = GenerateVectorDynamic(5);
    initialEstimate << 635, 635, 635, 635, 635;

    graph.emplace_shared<ControlFactorT>(key, tasks, coeff, model);

    Values initialEstimateFG;
    initialEstimateFG.insert(key, initialEstimate);

    Values result;
    if (optimizerType == 1)
    {
        DoglegParams params;
        // if (debugMode == 1)
        //     params.setVerbosityDL("VERBOSE");
        params.setDeltaInitial(deltaInitialDogleg);
        params.setRelativeErrorTol(relativeErrorTolerance);
        DoglegOptimizer optimizer(graph, initialEstimateFG, params);
        result = optimizer.optimize();
    }
    else if (optimizerType == 2)
    {
        LevenbergMarquardtParams params;
        params.setlambdaInitial(initialLambda);
        // if (debugMode > 1 && debugMode < 5)
        params.setVerbosityLM("SUMMARY");
        params.setlambdaLowerBound(lowerLambda);
        params.setlambdaUpperBound(upperLambda);
        params.setRelativeErrorTol(relativeErrorTolerance);
        LevenbergMarquardtOptimizer optimizer(graph, initialEstimateFG, params);
        result = optimizer.optimize();
    }
    else if (optimizerType == 3)
    {
        GaussNewtonParams params;
        if (debugMode == 1)
            params.setVerbosity("DELTA");
        params.setRelativeErrorTol(relativeErrorTolerance);
        GaussNewtonOptimizer optimizer(graph, initialEstimateFG, params);
        result = optimizer.optimize();
    }
    else if (optimizerType == 4)
    {
        NonlinearOptimizerParams params;
        params.setRelativeErrorTol(relativeErrorTolerance);
        if (debugMode == 1)
            params.setVerbosity("DELTA");
        NonlinearConjugateGradientOptimizer optimizer(graph, initialEstimateFG, params);
        result = optimizer.optimize();
    }

    VectorDynamic optComp = result.at<VectorDynamic>(key);

    cout << "After optimization, the period vector is " << optComp << endl;
    cout << "Before optimization, the total error is " << realObj(tasks, coeff) << endl;
    UpdateTaskSetPeriod(tasks, optComp);
    cout << "The objective function is " << realObj(tasks, coeff) << endl;
}

int main()
{
    TestResult tr;
    return TestRegistry::runAllTests(tr);
}
