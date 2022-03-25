#pragma once
#include <chrono>
#include <math.h>

#include "Parameters.h"
#include <Eigen/Dense>
#include "Declaration.h"
#include "Tasks.h"
#include "RTA_LL.h"
#include "Energy.h"
#include "utils.h"
#include "FrequencyModel.h"
#include "ControlFactorGraphUtils.h"
#include "MultiKeyFactor.h"
#include "InequalifyFactor.h"

struct FactorGraphEnergyLL
{
    static VectorDynamic ExtractResults(const Values &result, TaskSet tasks)
    {
        VectorDynamic executionTimes = GetParameterVD<double>(tasks, "executionTime");
        for (uint i = 0; i < tasks.size(); i++)
        {
            if (result.exists(GenerateControlKey(i, "executionTime")))
            {
                executionTimes(i, 0) = result.at<VectorDynamic>(GenerateControlKey(i, "executionTime"))(0, 0);
            }
        }
        return executionTimes;
    }

    class RTARelatedFactor : public NoiseModelFactor
    {
    public:
        TaskSet tasks;
        int index;
        VectorDynamic rtaBase;
        int dimension;
        vector<Symbol> keyVec;
        LambdaMultiKey f_with_RTA;

        RTARelatedFactor(vector<Symbol> &keyVec, TaskSet &tasks, int index, VectorDynamic rtaBase,
                         SharedNoiseModel model) : NoiseModelFactor(model, keyVec), tasks(tasks), index(index), rtaBase(rtaBase), dimension(keyVec.size()), keyVec(keyVec)
        {
            f_with_RTA = [tasks, index, rtaBase](const Values &x)
            {
                BeginTimer("f_with_RTA");
                VectorDynamic error = GenerateVectorDynamic(1);
                TaskSet tasksCurr = tasks;
                UpdateTaskSetExecutionTime(tasksCurr, FactorGraphEnergyLL::ExtractResults(x, tasks));
                RTA_LL r(tasksCurr);
                double rta = r.RTA_Common_Warm(rtaBase(index), index);
                error(0) = HingeLoss(min(tasksCurr[index].period, tasksCurr[index].deadline) - rta);
                EndTimer("f_with_RTA");
                return error;
            };
        }
        /* no need to optimize if it contains no keys */
        bool active(const Values &c) const override
        {
            return keyVec.size() != 0;
        }
        Vector unwhitenedError(const Values &x,
                               boost::optional<std::vector<Matrix> &> H = boost::none) const override
        {
            BeginTimer("RTARelatedFactor_unwhitenedError");
            if (H)
            {
                for (int i = 0; i < dimension; i++)
                {
                    if (exactJacobian)
                    {
                        NormalErrorFunction1D f =
                            [x, i, this](const VectorDynamic xi)
                        {
                            Symbol a = keyVec.at(i);
                            Values xx = x;
                            xx.update(a, xi);
                            return f_with_RTA(xx);
                        };
                        (*H)[i] = NumericalDerivativeDynamic(f, x.at<VectorDynamic>(keyVec[i]), deltaOptimizer);
                    }

                    else
                        (*H)[i] = GenerateVectorDynamic(1);
                }
                if (debugMode == 1)
                {
                    std::lock_guard<std::mutex> lock(mtx);
                    cout << Color::blue;
                    // PrintControlValues(x);
                    // x.print();
                    cout << Color::def;
                }
            }
            EndTimer("RTARelatedFactor_unwhitenedError");
            return f_with_RTA(x);
        }
    };

    static RTARelatedFactor
    GenerateRTARelatedFactor(std::vector<bool> maskForElimination, TaskSet &tasks, int index,
                             VectorDynamic &rtaBase)
    {

        std::vector<gtsam::Symbol> keys;
        keys.reserve(index);
        for (int i = 0; i <= index; i++)
        {
            // if (!maskForElimination[i])
            // {
            keys.push_back(GenerateControlKey(i, "executionTime"));
            // }
        }

        VectorDynamic sigma = GenerateVectorDynamic(1);
        sigma << noiseModelSigma / weightSchedulability;
        auto model = noiseModel::Diagonal::Sigmas(sigma);
        // return MultiKeyFactor(keys, f, model);
        return RTARelatedFactor(keys, tasks, index, rtaBase, model);
    }

    static MultiKeyFactor GenerateEliminationLLFactor(std::vector<bool> maskForElimination, TaskSet tasks,
                                                      int index, double rtaAtIndex)
    {
        std::vector<gtsam::Symbol> keys;
        for (int i = 0; i <= index; i++)
        {
            // if (!maskForElimination[i])
            keys.push_back(GenerateControlKey(i, "executionTime"));
        }
        LambdaMultiKey f = [tasks, index, rtaAtIndex](const Values &x)
        {
            VectorDynamic error = GenerateVectorDynamic(1);
            TaskSet tasksCurr = tasks;
            VectorDynamic executionTimeVecCurr = FactorGraphEnergyLL::ExtractResults(x, tasks).block(0, 0, index + 1, 1);

            MatrixDynamic coeff = executionTimeVecCurr.transpose();

            for (int i = 0; i < index; i++)
            {
                coeff(i) = ceil(rtaAtIndex / tasks[i].period);
            }
            coeff(index) = 1;
            error(0) = rtaAtIndex - (coeff * executionTimeVecCurr)(0, 0);
            return error;
        };
        auto model = noiseModel::Constrained::All(1);
        return MultiKeyFactor(keys, f, noiseModel::Constrained::All(1));
    }

    class EnergyFactor : public NoiseModelFactor1<VectorDynamic>
    {
    public:
        Task task_;
        /**
         * @brief Construct a new Inequality Factor 1 D object,
         *  mainly used in derived class because f is not defined
         */
        EnergyFactor(Key key, Task &task,
                     SharedNoiseModel model) : NoiseModelFactor1<VectorDynamic>(model, key), task_(task)
        {
        }

        Vector evaluateError(const VectorDynamic &x,
                             boost::optional<Matrix &> H = boost::none) const override
        {
            boost::function<Matrix(const VectorDynamic &)> f =
                [this](const VectorDynamic &executionTimeVector)
            {
                Task taskCurr = task_;
                taskCurr.executionTime = executionTimeVector(0, 0);
                VectorDynamic err = GenerateVectorDynamic1D(1.0 / taskCurr.period *
                                                            EstimateEnergyTask(taskCurr));
                return err;
            };
            VectorDynamic err = f(x);
            if (H)
            {
                *H = NumericalDerivativeDynamic(f, x, deltaOptimizer, 1);
            }
            return err;
        }
    };

    static NonlinearFactorGraph BuildControlGraph(std::vector<bool> maskForElimination, TaskSet tasks)
    {
        VectorDynamic rtaBase = RTALLVector(tasks);
        NonlinearFactorGraph graph;
        auto modelNormal = noiseModel::Isotropic::Sigma(1, noiseModelSigma);
        auto modelPunishmentSoft1 = noiseModel::Isotropic::Sigma(1, noiseModelSigma / weightHardConstraint);
        // auto modelPunishmentHard = noiseModel::Constrained::All(1);

        for (uint i = 0; i < tasks.size(); i++)
        {
            // energy factor
            graph.emplace_shared<EnergyFactor>(GenerateControlKey(i, "executionTime"), tasks[i], modelNormal);

            // add executionTime min/max limits
            graph.emplace_shared<LargerThanFactor1D>(GenerateControlKey(i, "executionTime"), tasks[i].executionTimeOrg, modelPunishmentSoft1);
            graph.emplace_shared<SmallerThanFactor1D>(GenerateControlKey(i, "executionTime"), min(tasks[i].deadline, tasks[i].period), modelPunishmentSoft1);

            // RTA factor
            graph.add(GenerateRTARelatedFactor(maskForElimination, tasks, i, rtaBase));

            if (maskForElimination[i])
            {
                graph.add(GenerateEliminationLLFactor(maskForElimination, tasks, i, rtaBase(i)));
            }
            // if (HasDependency(i, maskForElimination))
            // {
            //     auto factor = GenerateRTARelatedFactor(maskForElimination, tasks, i, rtaBase);
            //     graph.add(factor);
            // }
        }
        return graph;
    }

    static Values GenerateInitialFG(TaskSet tasks, std::vector<bool> &maskForElimination)
    {
        Values initialEstimateFG;
        for (uint i = 0; i < tasks.size(); i++)
        {
            // if (!maskForElimination[i])
            // {
            initialEstimateFG.insert(GenerateControlKey(i, "executionTime"),
                                     GenerateVectorDynamic1D(tasks[i].executionTime));
            // }
        }
        return initialEstimateFG;
    }

    static void FindEliminatedVariables(TaskSet &tasks, std::vector<bool> &maskForElimination, double disturb = disturb_init)
    {
        BeginTimer(__func__);

        RTA_LL r(tasks);
        VectorDynamic rtaBase = r.ResponseTimeOfTaskSet();
        bool whether_new_eliminate = false;
        while (!whether_new_eliminate && disturb <= disturb_max)
        {
            for (uint i = 0; i < tasks.size(); i++)
            {
                tasks[i].executionTime -= disturb;
                RTA_LL r1(tasks);
                VectorDynamic rtaCurr = r1.ResponseTimeOfTaskSet(rtaBase);
                if ((rtaBase - rtaCurr).array().abs().maxCoeff() >= disturb)
                // TODO: more analytic way
                {
                    if (!maskForElimination[i])
                        whether_new_eliminate = true;
                    maskForElimination[i] = true;
                }
                tasks[i].executionTime += disturb;
            }
            if (!whether_new_eliminate)
                disturb *= disturb_step;
            if (debugMode == 1)
            {
                std::lock_guard<std::mutex> lock(mtx);
                for (auto a : maskForElimination)
                    cout << a << ", ";
                cout << endl;
            }
        }

        EndTimer(__func__);
    }
};