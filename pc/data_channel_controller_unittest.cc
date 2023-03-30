/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/data_channel_controller.h"

#include <memory>

#include "pc/peer_connection_internal.h"
#include "pc/sctp_data_channel.h"
#include "pc/test/mock_peer_connection_internal.h"
#include "rtc_base/null_socket_server.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/run_loop.h"

namespace webrtc {

namespace {

using ::testing::NiceMock;
using ::testing::Return;

class MockDataChannelTransport : public webrtc::DataChannelTransportInterface {
 public:
  ~MockDataChannelTransport() override {}

  MOCK_METHOD(RTCError, OpenChannel, (int channel_id), (override));
  MOCK_METHOD(RTCError,
              SendData,
              (int channel_id,
               const SendDataParams& params,
               const rtc::CopyOnWriteBuffer& buffer),
              (override));
  MOCK_METHOD(RTCError, CloseChannel, (int channel_id), (override));
  MOCK_METHOD(void, SetDataSink, (DataChannelSink * sink), (override));
  MOCK_METHOD(bool, IsReadyToSend, (), (const, override));
};

class DataChannelControllerTest : public ::testing::Test {
 protected:
  DataChannelControllerTest()
      : network_thread_(std::make_unique<rtc::NullSocketServer>()) {
    network_thread_.Start();
    pc_ = rtc::make_ref_counted<NiceMock<MockPeerConnectionInternal>>();
    ON_CALL(*pc_, signaling_thread)
        .WillByDefault(Return(rtc::Thread::Current()));
    ON_CALL(*pc_, network_thread).WillByDefault(Return(&network_thread_));
  }

  ~DataChannelControllerTest() override {
    run_loop_.Flush();
    network_thread_.Stop();
  }

  test::RunLoop run_loop_;
  rtc::Thread network_thread_;
  rtc::scoped_refptr<NiceMock<MockPeerConnectionInternal>> pc_;
};

TEST_F(DataChannelControllerTest, CreateAndDestroy) {
  DataChannelController dcc(pc_.get());
}

TEST_F(DataChannelControllerTest, CreateDataChannelEarlyRelease) {
  DataChannelController dcc(pc_.get());
  auto ret = dcc.InternalCreateDataChannelWithProxy(
      "label", InternalDataChannelInit(DataChannelInit()));
  ASSERT_TRUE(ret.ok());
  auto channel = ret.MoveValue();
  // DCC still holds a reference to the channel. Release this reference early.
  channel = nullptr;
}

TEST_F(DataChannelControllerTest, CreateDataChannelEarlyClose) {
  DataChannelController dcc(pc_.get());
  EXPECT_FALSE(dcc.HasDataChannelsForTest());
  EXPECT_FALSE(dcc.HasUsedDataChannels());
  auto ret = dcc.InternalCreateDataChannelWithProxy(
      "label", InternalDataChannelInit(DataChannelInit()));
  ASSERT_TRUE(ret.ok());
  auto channel = ret.MoveValue();
  EXPECT_TRUE(dcc.HasDataChannelsForTest());
  EXPECT_TRUE(dcc.HasUsedDataChannels());
  channel->Close();
  EXPECT_FALSE(dcc.HasDataChannelsForTest());
  EXPECT_TRUE(dcc.HasUsedDataChannels());
}

TEST_F(DataChannelControllerTest, CreateDataChannelLateRelease) {
  auto dcc = std::make_unique<DataChannelController>(pc_.get());
  auto ret = dcc->InternalCreateDataChannelWithProxy(
      "label", InternalDataChannelInit(DataChannelInit()));
  ASSERT_TRUE(ret.ok());
  auto channel = ret.MoveValue();
  dcc.reset();
  channel = nullptr;
}

TEST_F(DataChannelControllerTest, CloseAfterControllerDestroyed) {
  auto dcc = std::make_unique<DataChannelController>(pc_.get());
  auto ret = dcc->InternalCreateDataChannelWithProxy(
      "label", InternalDataChannelInit(DataChannelInit()));
  ASSERT_TRUE(ret.ok());
  auto channel = ret.MoveValue();
  dcc.reset();
  channel->Close();
}

TEST_F(DataChannelControllerTest, AsyncChannelCloseTeardown) {
  DataChannelController dcc(pc_.get());
  auto ret = dcc.InternalCreateDataChannelWithProxy(
      "label", InternalDataChannelInit(DataChannelInit()));
  ASSERT_TRUE(ret.ok());
  auto channel = ret.MoveValue();
  SctpDataChannel* inner_channel =
      DowncastProxiedDataChannelInterfaceToSctpDataChannelForTesting(
          channel.get());
  // Grab a reference for testing purposes.
  inner_channel->AddRef();

  channel = nullptr;  // dcc still holds a reference to `channel`.
  EXPECT_TRUE(dcc.HasDataChannelsForTest());

  // Trigger a Close() for the channel. This will send events back to dcc,
  // eventually reaching `OnSctpDataChannelClosed` where dcc removes
  // the channel from the internal list of data channels, but does not release
  // the reference synchronously since that reference might be the last one.
  inner_channel->Close();
  // Now there should be no tracked data channels.
  EXPECT_FALSE(dcc.HasDataChannelsForTest());
  // But there should be an async operation queued that still holds a reference.
  // That means that the test reference, must not be the last one.
  ASSERT_NE(inner_channel->Release(),
            rtc::RefCountReleaseStatus::kDroppedLastRef);
  // Grab a reference again (using the pointer is safe since the object still
  // exists and we control the single-threaded environment manually).
  inner_channel->AddRef();
  // Now run the queued up async operations on the signaling (current) thread.
  // This time, the reference formerly owned by dcc, should be release and the
  // truly last reference is now held by the test.
  run_loop_.Flush();
  // Check that this is the last reference.
  EXPECT_EQ(inner_channel->Release(),
            rtc::RefCountReleaseStatus::kDroppedLastRef);
}

// Allocate the maximum number of data channels and then one more.
// The last allocation should fail.
TEST_F(DataChannelControllerTest, MaxChannels) {
  NiceMock<MockDataChannelTransport> transport;
  int channel_id = 0;

  ON_CALL(*pc_, GetSctpSslRole_n).WillByDefault([&]() {
    return absl::optional<rtc::SSLRole>((channel_id & 1) ? rtc::SSL_SERVER
                                                         : rtc::SSL_CLIENT);
  });

  DataChannelController dcc(pc_.get());
  pc_->network_thread()->BlockingCall(
      [&] { dcc.set_data_channel_transport(&transport); });

  // Allocate the maximum number of channels + 1. Inside the loop, the creation
  // process will allocate a stream id for each channel.
  for (channel_id = 0; channel_id <= cricket::kMaxSctpStreams; ++channel_id) {
    auto ret = dcc.InternalCreateDataChannelWithProxy(
        "label", InternalDataChannelInit(DataChannelInit()));
    if (channel_id == cricket::kMaxSctpStreams) {
      // We've reached the maximum and the previous call should have failed.
      EXPECT_FALSE(ret.ok());
    } else {
      // We're still working on saturating the pool. Things should be working.
      EXPECT_TRUE(ret.ok());
    }
  }
}

// Test that while a data channel is in the `kClosing` state, its StreamId does
// not get re-used for new channels. Only once the state reaches `kClosed`
// should a StreamId be available again for allocation.
TEST_F(DataChannelControllerTest, NoStreamIdReuseWhileClosing) {
  ON_CALL(*pc_, GetSctpSslRole_n).WillByDefault([&]() {
    return rtc::SSL_CLIENT;
  });

  DataChannelController dcc(pc_.get());
  NiceMock<MockDataChannelTransport> transport;
  pc_->network_thread()->BlockingCall(
      [&] { dcc.set_data_channel_transport(&transport); });

  // Create the first channel and check that we got the expected, first sid.
  auto channel1 = dcc.InternalCreateDataChannelWithProxy(
                         "label", InternalDataChannelInit(DataChannelInit()))
                      .MoveValue();
  ASSERT_EQ(channel1->id(), 0);

  // Start closing the channel and make sure its state is `kClosing`
  channel1->Close();
  ASSERT_EQ(channel1->state(), DataChannelInterface::DataState::kClosing);

  // Create a second channel and make sure we get a new StreamId, not the same
  // as that of channel1.
  auto channel2 = dcc.InternalCreateDataChannelWithProxy(
                         "label2", InternalDataChannelInit(DataChannelInit()))
                      .MoveValue();
  ASSERT_NE(channel2->id(), channel1->id());  // In practice the id will be 2.

  // Simulate the acknowledgement of the channel closing from the transport.
  // This completes the closing operation of channel1.
  pc_->network_thread()->BlockingCall([&] { dcc.OnChannelClosed(0); });
  run_loop_.Flush();
  ASSERT_EQ(channel1->state(), DataChannelInterface::DataState::kClosed);

  // Now create a third channel. This time, the id of the first channel should
  // be available again and therefore the ids of the first and third channels
  // should be the same.
  auto channel3 = dcc.InternalCreateDataChannelWithProxy(
                         "label3", InternalDataChannelInit(DataChannelInit()))
                      .MoveValue();
  EXPECT_EQ(channel3->id(), channel1->id());
}

}  // namespace
}  // namespace webrtc
