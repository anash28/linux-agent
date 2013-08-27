#ifndef DATTO_CLIENT_DEVICE_MAPPER_DM_RESUME_TASK_H_
#define DATTO_CLIENT_DEVICE_MAPPER_DM_RESUME_TASK_H_

#include "device_mapper/dm_task.h"

namespace datto_linux_client {

class DmResumeTask : public DmTask {
 public:
  explicit DmResumeTask(std::string device_name);
  void Run();
  ~DmResumeTask();
};

}

#endif //  DATTO_CLIENT_DEVICE_MAPPER_DM_RESUME_TASK_H_