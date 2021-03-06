//
// Created by chnlkw on 1/23/18.
//

#ifndef DMR_DEVICE_H
#define DMR_DEVICE_H

#include "DeviceBase.h"
#include "Config.h"

namespace Xuanwu {
    namespace detail {
        template <class T>
        struct UID {
            int uid;
            UID() {
                static int s_uid = 0;
                uid = s_uid++;
            }

            auto ID() { return uid; }

        };
    }
    class CPUDevice : public DeviceBase, public detail::UID<CPUDevice> {
    public:

        CPUDevice();

        int ScoreRunTask(TaskPtr t) override;

        CPUWorker& GetWorker() {
            return *(CPUWorker*)workers_.at(0).get();
        }

        void log(el::base::type::ostream_t &os) const override;
    };

    class GPUDevice : public DeviceBase {
        int gpu_id_;
        cudaDeviceProp deviceProp;

        static int GetGPUId() {
            static int id = 0;
            return id++;
        }

    public:
        using NumWorkers = Strong<int, 2>;
        using HeapLimit = Strong<size_t, 100LU<<20>;
        GPUDevice(NumWorkers, HeapLimit);

        int ScoreRunTask(TaskPtr t) override;

        int GPUID() const {
            return gpu_id_;
        }

        size_t MaxSharedMemorySize() const {
            return deviceProp.sharedMemPerBlock;
        }

        void log(el::base::type::ostream_t &os) const override;

    };

}
#endif //DMR_DEVICE_H
