#include "backup/backup_builder.h"

#include "backup/backup_exception.h"
#include "device_synchronizer/device_synchronizer.h"

#include <glog/logging.h>

namespace datto_linux_client {

std::shared_ptr<Backup> BackupBuilder::CreateBackup(
    const std::vector<Vector> &vectors,
    const std::shared_ptr<BackupCoordinator> &coordinator,
    bool is_full) {
  std::vector<std::shared_ptr<DeviceSynchronizerInterface>> syncs_to_do;

  for (auto vector : vectors) {
    syncs_to_do.push_back(CreateDeviceSynchronizer(vector, is_full));
  }

  return std::make_shared<Backup>(syncs_to_do, coordinator);
}

std::shared_ptr<DeviceSynchronizerInterface>
BackupBuilder::CreateDeviceSynchronizer(const Vector &vector,
                                        bool is_full) {
  auto source_device =
      block_device_factory_->CreateMountableBlockDeviceFromUuid(
          vector.block_device_uuid());

  std::string host = vector.destination_host();
  uint16_t port = (uint16_t)vector.destination_port();

  auto remote_device =
      block_device_factory_->CreateRemoteBlockDevice(host, port);

  if (!sector_manager_->IsTracing(*source_device)) {
    if (is_full) {
      sector_manager_->StartTracer(*source_device);
    } else {
      LOG(ERROR) << source_device->path() << " has no trace data."
                 << " Must do a full.";
      throw BackupException("No trace data exists for source");
    }
  }

  if (is_full) {
    auto store = sector_manager_->GetStore(*source_device);
    store->ClearIntervals();

    auto in_use_set = source_device->GetInUseSectors();
    for (const SectorInterval &interval : *in_use_set) {
      DLOG(INFO) << "Adding interval " << interval;
      store->AddNonVolatileInterval(interval);
    }
  }

  DLOG(INFO) << "Creating DeviceSynchronizer for "
             << source_device->path();
  return std::make_shared<DeviceSynchronizer>(source_device,
                                              sector_manager_,
                                              remote_device);
}

} // datto_linux_client
