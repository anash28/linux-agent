#include "device_synchronizer/device_synchronizer.h"

#include "backup/backup_coordinator.h"
#include "backup_status_tracker/sync_count_handler.h"
#include "test/loop_device.h"
#include "unsynced_sector_manager/sector_interval.h"
#include "unsynced_sector_manager/sector_set.h"

#include "backup_status_reply.pb.h"

#include <memory>
#include <array>

#include <glog/logging.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace {

using ::datto_linux_client::BackupCoordinator;
using ::datto_linux_client::BlockDevice;
using ::datto_linux_client::DeviceSynchronizer;
using ::datto_linux_client::DeviceTracer;
using ::datto_linux_client::MountableBlockDevice;
using ::datto_linux_client::SectorInterval;
using ::datto_linux_client::SectorSet;
using ::datto_linux_client::SyncCountHandler;
using ::datto_linux_client::UnsyncedSectorManager;
using ::datto_linux_client::UnsyncedSectorStore;
using ::datto_linux_client_test::LoopDevice;
using ::testing::Assign;
using ::testing::AtLeast;
using ::testing::NiceMock;
using ::testing::StrictMock;
using ::testing::Return;
using ::testing::ReturnPointee;
using ::testing::SetArgPointee;
using ::testing::Truly;
using ::testing::_;

class MockBackupCoordinator : public BackupCoordinator {
 public:
  MockBackupCoordinator() {}

  MOCK_METHOD0(SignalFinished, void());
  MOCK_METHOD0(SignalMoreWorkToDo, bool());
  MOCK_CONST_METHOD0(IsCancelled, bool());
  MOCK_METHOD1(WaitUntilFinished, bool(int));
};

class MockSyncCountHandler : public SyncCountHandler {
 public:
  MockSyncCountHandler() {}
  MOCK_METHOD1(UpdateSyncedCount, void(uint64_t num_synced));
  MOCK_METHOD1(UpdateUnsyncedCount, void(uint64_t num_unsynced));
};

class MockMountableBlockDevice : public MountableBlockDevice {
 public:
  explicit MockMountableBlockDevice(std::string b_block_path)
      : MountableBlockDevice(b_block_path) { }

  MOCK_METHOD0(GetInUseSectors, std::shared_ptr<const SectorSet>());
  MOCK_METHOD0(Freeze, void());
  MOCK_METHOD0(Thaw, void());
};

class MockUnsyncedSectorManager : public UnsyncedSectorManager {
 public:
  MockUnsyncedSectorManager() {}
  MOCK_CONST_METHOD1(IsTracing, bool(const BlockDevice &));
  MOCK_METHOD1(FlushTracer, void(const BlockDevice &));
  MOCK_METHOD1(GetStore,
               std::shared_ptr<UnsyncedSectorStore>(const BlockDevice &));
};

class MockUnsyncedSectorStore : public UnsyncedSectorStore {
 public:
  MockUnsyncedSectorStore() : UnsyncedSectorStore(10) {}
  MOCK_METHOD2(AddInterval, void(const SectorInterval &, const time_t epoch));
  MOCK_METHOD1(AddNonVolatileInterval, void(const SectorInterval &));
  MOCK_METHOD1(RemoveInterval, void(const SectorInterval &));
  MOCK_METHOD0(ClearIntervals, void());
  MOCK_CONST_METHOD2(GetInterval, bool(SectorInterval *const output,
                                  const time_t epoch));
  MOCK_CONST_METHOD0(UnsyncedSectorCount, uint64_t());
};

class DeviceSynchronizerTest : public ::testing::Test {
 protected:
  DeviceSynchronizerTest() {
    source_loop = std::make_shared<LoopDevice>();
    destination_loop = std::make_shared<LoopDevice>();

    source_device =
        std::make_shared<MockMountableBlockDevice>(source_loop->path());

    source_manager = std::make_shared<StrictMock<MockUnsyncedSectorManager>>();

    destination_device =
        std::make_shared<MockMountableBlockDevice>(destination_loop->path());

    is_source = [&](const BlockDevice &b) {
      return b.dev_t() == source_device->dev_t();
    };
  }

  void ConstructSynchronizer() {
    device_synchronizer = std::make_shared<DeviceSynchronizer>(
                              source_device,
                              source_manager,
                              destination_device);
  }

  // Order matters here, things will be destructed in opposite order
  // of declaration

  std::shared_ptr<LoopDevice> source_loop;
  std::shared_ptr<MockMountableBlockDevice> source_device;
  std::shared_ptr<MockUnsyncedSectorManager> source_manager;

  std::function<bool(const BlockDevice&)> is_source;

  std::shared_ptr<LoopDevice> destination_loop;
  std::shared_ptr<MockMountableBlockDevice> destination_device;

  std::shared_ptr<DeviceSynchronizer> device_synchronizer;
};

} // anonymous namespace

namespace boost { namespace icl {
// This needs to be in global namespace because of difficulties resolving
// the print method for SectorInterval
void PrintTo(const SectorInterval &si, ::std::ostream *os) {
  *os << si;
}
}}


TEST_F(DeviceSynchronizerTest, CanConstruct) {
  // Make sure there is something to sync
  ConstructSynchronizer();
  EXPECT_NE(nullptr, device_synchronizer);
}

TEST_F(DeviceSynchronizerTest, ConstructFailure) {
  source_device = destination_device;

  try {
    ConstructSynchronizer();
    FAIL() << "Construction succeeded but the devices were the same";
  } catch (const std::runtime_error &e) {
    // good
  }
}

TEST_F(DeviceSynchronizerTest, SimpleSyncTest) {
  ConstructSynchronizer();

  auto coordinator = std::make_shared<MockBackupCoordinator>();
  auto count_handler = std::make_shared<NiceMock<MockSyncCountHandler>>();
  auto mock_store = std::make_shared<MockUnsyncedSectorStore>();

  uint64_t unsynced_count = 10;
  SectorInterval interval_to_sync(0, 10);

  EXPECT_CALL(*source_device, Thaw())
      .Times(AtLeast(1));

  EXPECT_CALL(*source_device, Freeze())
      .Times(AtLeast(1));

  EXPECT_CALL(*mock_store, UnsyncedSectorCount())
      .Times(AtLeast(1))
      .WillRepeatedly(ReturnPointee(&unsynced_count));

  EXPECT_CALL(*mock_store, RemoveInterval(interval_to_sync))
      .Times(1)
      .WillOnce(DoAll(Assign(&unsynced_count, 0),
                      Assign(&interval_to_sync, SectorInterval(0, 0))));

  EXPECT_CALL(*mock_store, GetInterval(_, _))
      .Times(AtLeast(1))
      .WillRepeatedly(DoAll(SetArgPointee<0>(interval_to_sync),
                            Return(true)));

  EXPECT_CALL(*source_manager, GetStore(Truly(is_source)))
      .Times(AtLeast(1))
      .WillRepeatedly(Return(mock_store));

  EXPECT_CALL(*source_manager, IsTracing(_))
      .WillRepeatedly(Return(true));

  EXPECT_CALL(*coordinator, IsCancelled())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*coordinator, SignalFinished());
  EXPECT_CALL(*coordinator, SignalMoreWorkToDo())
      .Times(0);
  EXPECT_CALL(*coordinator, WaitUntilFinished(_))
      .WillRepeatedly(Return(true));

  device_synchronizer->DoSync(coordinator, count_handler);
}

// This uses mostly real versions of things
TEST_F(DeviceSynchronizerTest, SyncTest) {
  // Write garbage to the first 4k block then sync (which should overwrite it)
  // from source_device
  int source_fd = source_device->Open();

  int urandom_fd = open("/dev/urandom", O_RDONLY);
  std::array<char, 4096> buf;

  // TODO Remove this once the block size is no longer hard coded
  ASSERT_EQ(4096UL, source_device->BlockSizeBytes());

  auto real_source_device =
      std::make_shared<NiceMock<MockMountableBlockDevice>>(
          source_loop->path());
  auto real_source_manager = std::make_shared<UnsyncedSectorManager>();
  auto real_destination_device =
      std::make_shared<BlockDevice>(destination_loop->path());
  auto source_store = real_source_manager->GetStore(*real_source_device);

  for (int i = 0; i < 5; i += 1) {
    if (read(urandom_fd, buf.data(), 4096) == -1) {
      FAIL() << "Failed reading from urandom";
    }

    if (write(source_fd, buf.data(), 4096) == -1) {
      FAIL() << "Failed writing to source";
    }
    // Only mark every other block to sync
    if (i % 2 == 0) {
      source_store->AddNonVolatileInterval(SectorInterval(i * 8, (i + 1) * 8));
    }
  }

  std::array<char, 4096> zero_array;
  // Make sure we wrote successfully
  lseek(source_fd, 0, SEEK_SET);
  for (int i = 0; i < 5; i += 1) {
    if (read(source_fd, buf.data(), 4096) == -1) {
      FAIL() << "Failed reading source";
    }
    ASSERT_NE(buf, zero_array);
  }

  close(urandom_fd);
  source_device->Close();

  // Do the Sync
  device_synchronizer = std::make_shared<DeviceSynchronizer>(
                            real_source_device,
                            real_source_manager,
                            destination_device);

  // Run the sync
  auto count_handler = std::make_shared<NiceMock<MockSyncCountHandler>>();
  auto coordinator = std::make_shared<NiceMock<MockBackupCoordinator>>();
  EXPECT_CALL(*coordinator, IsCancelled())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*coordinator, SignalFinished());
  EXPECT_CALL(*coordinator, SignalMoreWorkToDo())
      .Times(0);
  EXPECT_CALL(*coordinator, WaitUntilFinished(_))
      .WillRepeatedly(Return(true));

  real_source_manager->StartTracer(*real_source_device);

  device_synchronizer->DoSync(coordinator, count_handler);

  // Make sure it worked
  source_fd = source_device->Open();
  int destination_fd = destination_device->Open();
  std::array<char, 4096> source_buf;
  std::array<char, 4096> destination_buf;

  // 0, 2, 4 should be synced, while 1, 3 should not
  for (int i = 0; i < 5; i += 1) {
    if (read(source_fd, source_buf.data(), 4096) == -1) {
      PLOG(ERROR) << "Read";
      FAIL() << "Failed reading source";
    }
    if (read(destination_fd, destination_buf.data(), 4096) == -1) {
      PLOG(ERROR) << "Read";
      FAIL() << "Failed reading destination";
    }

    if (i % 2 == 0) {
      EXPECT_EQ(source_buf, destination_buf) << "i is " << i;
    } else {
      EXPECT_NE(source_buf, destination_buf) << "i is " << i;
    }
  }
}
