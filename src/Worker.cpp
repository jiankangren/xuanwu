//
// Created by chnlkw on 3/2/18.
//

#include "Worker.h"
#include "Device.h"
#include "Task.h"
#include "Data.h"
#include "clock.h"

#define LG(x) CLOG(x, "Worker")
namespace Xuanwu {

    CPUWorker::CPUWorker(CPUDevice *cpu) : WorkerBase(cpu) {}

    std::vector<TaskPtr> CPUWorker::GetCompleteTasks() {
        auto cpu = dynamic_cast<CPUDevice *>(device_);
        assert(cpu);
        std::vector<TaskPtr> ret;
        for (TaskPtr t : tasks_) {
            CPUTask *cputask = dynamic_cast<CPUTask *>(t.get());
            if (!cputask)
                cputask = t->GetCPUTask();
            CLOG(INFO, "Worker") << *this << " Run " << *t;
            Clock clk;
            if (cputask) {
                for (auto &m : t->GetMetas()) {
                    if (m.readable) {
                        while (!m.data->ReadAsync(this, device_));
                    }
                    if (m.writable && m.data->Bytes() > 0) {
                        while (!m.data->WriteAsync(this, device_));
                    }
                    CLOG(DEBUG, "Worker") << m;
                }
                (*cputask)(CPUContext(cpu));
            } else
                t->Run(this);
            CLOG(INFO, "Worker") << *this << " " << *t << " uses " << clk.timeElapsed() << " seconds";
            ret.push_back(t);
        }
        tasks_.clear();
        return ret;
    }

    Event CPUWorker::Copy(Ptr dst, Ptr src, size_t bytes) {
        return CPUCopy(dst, src, bytes);
    }

    GPUWorker::GPUWorker(GPUDevice *gpu) :
            WorkerBase(gpu) {
        CLOG(INFO, "Worker") << "Create GPU Worker with device = " << gpu->GPUID();
        CUDA_CALL(cudaSetDevice, gpu->GPUID());
        CUDA_CALL(cudaStreamCreate, &stream_);
    }

    bool GPUWorker::RunTask(TaskPtr t) {
        auto gpu = dynamic_cast<GPUDevice *>(device_);
        assert(gpu);
        CUDA_CALL(cudaSetDevice, gpu->GPUID());

        Meta meta{GetEvent(), GetEvent(), GetEvent(), t};
        CUDA_CALL(cudaEventRecord, meta.beg_event, stream_);

        auto gputask = dynamic_cast<GPUTask *>(t.get());
        if (!gputask)
            gputask = t->GetGPUTask();
        if (gputask) {
            for (auto &m : t->GetMetas()) {
                if (m.readable) {
                    if (!m.data->ReadAsync(this, device_))
                        return false;
                }
                if (m.writable && m.data->Bytes() > 0) {
                    if (!m.data->WriteAsync(this, device_))
                        return false;
                }
                CLOG(DEBUG, "Worker") << m;
            }
            CLOG(INFO, "Worker") << *this << " Run " << *t;
            CUDA_CALL(cudaEventRecord, meta.transfer_event, stream_);
            (*gputask)(GPUContext(GetDefaultMM(), gpu, stream_, this, t));
        } else {
            CLOG(INFO, "Worker") << *this << " RunOld " << *t;
            CUDA_CALL(cudaEventRecord, meta.transfer_event, stream_);
            t->Run(this);
        }
        CUDA_CALL(cudaEventRecord, meta.end_event, stream_);
        queue_.push_back(meta);
        return true;
    }

    std::vector<TaskPtr> GPUWorker::GetCompleteTasks() {
#ifdef USE_CUDA
        auto gpu = dynamic_cast<GPUDevice *>(device_);
        CUDA_CALL(cudaSetDevice, gpu->GPUID());

        std::vector<TaskPtr> ret;
        if (Empty())
            return ret;

        while (true) {
            Meta meta = queue_.front();
            cudaError_t err = cudaEventQuery(meta.end_event);
            if (err == cudaSuccess) {
                queue_.pop_front();
                float tranfer_ms, calc_ms;
                CUDA_CALL(cudaEventElapsedTime, &tranfer_ms, meta.beg_event, meta.transfer_event);
                CUDA_CALL(cudaEventElapsedTime, &calc_ms, meta.transfer_event, meta.end_event);
                CLOG(INFO, "Worker") << *this << " " << *meta.task << " transfer " << tranfer_ms << " ms, " << " calc "
                                     << calc_ms << " ms";

                ret.push_back(meta.task);
                events_unused_.push_back(meta.end_event);
                events_unused_.push_back(meta.beg_event);
                events_unused_.push_back(meta.transfer_event);

                for (auto &p : meta.task->GetTempDataMappings()) {
//                    CLOG(INFO, "Worker") << "Cleaning temp data mapping";
                    auto &tmp_arr = p.first;
                    auto &data = p.second;
                    DeviceArrayBase h_arr;
                    CUDA_CALL(cudaMemcpyAsync, &h_arr, tmp_arr.GetArrPtr(), sizeof(DeviceArrayBase), cudaMemcpyDefault,
                              stream_);
                    CUDA_CALL(cudaStreamSynchronize, stream_);
                    data->Create(h_arr.bytes, device_);
//                    printf("d.data() = %p m.ptr=%p m.bytes=%lu\n", data->data(), h_arr.ptr, h_arr.bytes);
                    run_copy_free_kernel(data->data(), h_arr.ptr, h_arr.bytes, stream_);
                    CUDA_CALL(cudaStreamSynchronize, stream_);
                }
                break;
            } else if (err == cudaErrorNotReady) {
                continue;
            } else {
                LOG(ERROR) << *this << " Run task error " << *meta.task << " err = "
                           << cudaGetErrorString(cudaGetLastError());
                CUDA_CHECK();
            }
        }
        return ret;
#else
        return {};
#endif
    }

    cudaEvent_t GPUWorker::GetEvent() {
        cudaEvent_t e;
        if (!events_unused_.empty()) {
            e = events_unused_.back();
            events_unused_.pop_back();
        } else {
            CUDA_CALL(cudaEventCreate, &e);
        }
        return e;
    }

    Event GPUWorker::Copy(Ptr dst, Ptr src, size_t bytes) {
        auto gpu = dynamic_cast<GPUDevice *>(device_);
        assert(gpu);
        return GPUCopy(dst, src, bytes, gpu->GPUID(), stream_);
    }
}
