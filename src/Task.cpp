//
// Created by chnlkw on 2/28/18.
//

#include <functional>
#include "Task.h"
#include "Worker.h"
#include "Data.h"
#include "Device.h"

namespace Xuanwu {
#define LG(x) CLOG(x, "Task")

    TaskBase::~TaskBase() {
        LG(INFO) << "Destory " << *this;
        LOG_IF(!finished, FATAL) << "Not Finsihed " << *this;
    }

    TaskBase::TaskBase(std::string name, std::unique_ptr<CPUTask> cputask, std::unique_ptr<GPUTask> gputask) :
            name_(std::move(name)), cputask_(std::move(cputask)), gputask_(std::move(gputask)) {
        static int g_seq = 0;
        seq = g_seq++;
        LG(INFO) << "Create " << *this;
    }

    TaskBase::TaskBase(std::string name) :
            name_(std::move(name)) {
        LG(INFO) << "Create " << *this;
    }

    void TaskBase::AddInputRemote(DataBasePtr data) {
        LG(INFO) << *this << " AddInputRemote " << *data;
        metas_.emplace_back(data, true, false, true);
    }

    void TaskBase::AddRemoteInputs(std::vector<DataBasePtr> data) {
        for (auto &d : data)
            AddInputRemote(d);
    }

    void TaskBase::AddOutputRemote(DataBasePtr data) {
        LG(INFO) << *this << "AddInputRemote " << *data;
        metas_.emplace_back(data, false, true, true);
    }

    void TaskBase::AddRemoteOutputs(std::vector<DataBasePtr> data) {
        for (auto &d : data)
            AddOutputRemote(d);
    }

    void TaskBase::AddInput(DataBasePtr data) {
        LG(INFO) << *this << "AddInput " << *data;
        metas_.emplace_back(data, true, false, false);
    }

    void TaskBase::AddInputs(std::vector<DataBasePtr> data) {
        for (auto &d : data)
            AddInput(d);
    }

    void TaskBase::AddOutput(DataBasePtr data) {
        LG(INFO) << *this << "AddOutput " << *data;
        metas_.emplace_back(data, false, true, false);
    }

    void TaskBase::AddOutputs(std::vector<DataBasePtr> data) {
        for (auto &d : data)
            AddOutput(d);
    }

    void TaskBase::AddInOutput(DataBasePtr data) {
        LG(INFO) << *this << "AddInOutput " << *data;
        metas_.emplace_back(data, true, true, false);
    }

    void TaskBase::AddInOutputs(std::vector<DataBasePtr> data) {
        for (auto &d : data)
            AddInOutput(d);
    }

    void TaskBase::Finish() {
        LG(INFO) << *this << " Finish ";
        tmp_datas_.clear();
        metas_.clear();
        tmp_datas_.clear();
        cputask_.reset();
        gputask_.reset();
        finished = true;
    }

    CPUTask *TaskBase::GetCPUTask() const { return cputask_.get(); }

    GPUTask *TaskBase::GetGPUTask() const { return gputask_.get(); }

    bool TaskBase::IsFinished() const {
        return finished;
    }

    void TaskBase::AddTempData(DataBasePtr data) {
        tmp_datas_.push_back(std::move(data));
    }

    void TaskBase::AddDependency(std::vector<TaskPtr> tasks) {
        Append(depend_tasks_, std::move(tasks));
    }

    void TaskBase::RunAfter(std::vector<TaskPtr> tasks) {
        Append(run_after_tasks_, std::move(tasks));
    }


    ArrayBase *GPUContext::MakeArrayBase(size_t bytes) {
        auto data = mm->MakeDataBase(bytes);// std::make_unique<ArrayBase>(bytes, mm->GetAllocatorByDevice(dev));
        data->Create(bytes, dev);
        task->AddTempData(data);
        return data->CurrentArray();
    }

    void CPUContext::Copy(Ptr dst, Ptr src, size_t bytes) {
        worker->Copy(dst, src, bytes);
    }

}
