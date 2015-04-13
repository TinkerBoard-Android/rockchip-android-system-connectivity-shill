// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/net/rtnl_handler.h"

#include <string>

#include <gtest/gtest.h>
#include <net/if.h>
#include <sys/socket.h>
#include <linux/netlink.h>  // Needs typedefs from sys/socket.h.
#include <linux/rtnetlink.h>
#include <sys/ioctl.h>

#include <base/bind.h>

#include "shill/net/mock_io_handler_factory.h"
#include "shill/net/mock_sockets.h"
#include "shill/net/rtnl_message.h"

using base::Bind;
using base::Callback;
using base::Unretained;
using std::string;
using testing::_;
using testing::A;
using testing::DoAll;
using testing::Return;
using testing::StrictMock;
using testing::Test;

namespace shill {

namespace {

const int kTestInterfaceIndex = 4;

ACTION(SetInterfaceIndex) {
  if (arg2) {
    reinterpret_cast<struct ifreq *>(arg2)->ifr_ifindex = kTestInterfaceIndex;
  }
}

MATCHER_P(MessageType, message_type, "") {
  return std::get<0>(arg).type() == message_type;
}

}  // namespace

class RTNLHandlerTest : public Test {
 public:
  RTNLHandlerTest()
      : sockets_(new StrictMock<MockSockets>()),
        callback_(Bind(&RTNLHandlerTest::HandlerCallback, Unretained(this))) {
  }

  virtual void SetUp() {
    RTNLHandler::GetInstance()->io_handler_factory_ = &io_handler_factory_;
    RTNLHandler::GetInstance()->sockets_.reset(sockets_);
  }

  virtual void TearDown() {
    RTNLHandler::GetInstance()->Stop();
  }

  MOCK_METHOD1(HandlerCallback, void(const RTNLMessage &));

 protected:
  static const int kTestSocket;
  static const int kTestDeviceIndex;
  static const char kTestDeviceName[];

  void AddLink();
  void AddNeighbor();
  void StartRTNLHandler();
  void StopRTNLHandler();

  MockSockets *sockets_;
  StrictMock<MockIOHandlerFactory> io_handler_factory_;
  Callback<void(const RTNLMessage &)> callback_;
};

const int RTNLHandlerTest::kTestSocket = 123;
const int RTNLHandlerTest::kTestDeviceIndex = 123456;
const char RTNLHandlerTest::kTestDeviceName[] = "test-device";

void RTNLHandlerTest::StartRTNLHandler() {
  EXPECT_CALL(*sockets_, Socket(PF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE))
      .WillOnce(Return(kTestSocket));
  EXPECT_CALL(*sockets_, Bind(kTestSocket, _, sizeof(sockaddr_nl)))
      .WillOnce(Return(0));
  EXPECT_CALL(*sockets_, SetReceiveBuffer(kTestSocket, _)).WillOnce(Return(0));
  EXPECT_CALL(io_handler_factory_, CreateIOInputHandler(kTestSocket, _, _));
  RTNLHandler::GetInstance()->Start(0);
}

void RTNLHandlerTest::StopRTNLHandler() {
  EXPECT_CALL(*sockets_, Close(kTestSocket)).WillOnce(Return(0));
  RTNLHandler::GetInstance()->Stop();
}

void RTNLHandlerTest::AddLink() {
  RTNLMessage message(RTNLMessage::kTypeLink,
                      RTNLMessage::kModeAdd,
                      0,
                      0,
                      0,
                      kTestDeviceIndex,
                      IPAddress::kFamilyIPv4);
  message.SetAttribute(static_cast<uint16_t>(IFLA_IFNAME),
                       ByteString(string(kTestDeviceName), true));
  ByteString b(message.Encode());
  InputData data(b.GetData(), b.GetLength());
  RTNLHandler::GetInstance()->ParseRTNL(&data);
}

void RTNLHandlerTest::AddNeighbor() {
  RTNLMessage message(RTNLMessage::kTypeNeighbor,
                      RTNLMessage::kModeAdd,
                      0,
                      0,
                      0,
                      kTestDeviceIndex,
                      IPAddress::kFamilyIPv4);
  ByteString encoded(message.Encode());
  InputData data(encoded.GetData(), encoded.GetLength());
  RTNLHandler::GetInstance()->ParseRTNL(&data);
}

TEST_F(RTNLHandlerTest, ListenersInvoked) {
  StartRTNLHandler();

  std::unique_ptr<RTNLListener> link_listener(
      new RTNLListener(RTNLHandler::kRequestLink, callback_));
  std::unique_ptr<RTNLListener> neighbor_listener(
      new RTNLListener(RTNLHandler::kRequestNeighbor, callback_));

  EXPECT_CALL(*this, HandlerCallback(A<const RTNLMessage &>()))
      .With(MessageType(RTNLMessage::kTypeLink));
  EXPECT_CALL(*this, HandlerCallback(A<const RTNLMessage &>()))
      .With(MessageType(RTNLMessage::kTypeNeighbor));

  AddLink();
  AddNeighbor();

  StopRTNLHandler();
}

TEST_F(RTNLHandlerTest, GetInterfaceName) {
  EXPECT_EQ(-1, RTNLHandler::GetInstance()->GetInterfaceIndex(""));
  {
    struct ifreq ifr;
    string name(sizeof(ifr.ifr_name), 'x');
    EXPECT_EQ(-1, RTNLHandler::GetInstance()->GetInterfaceIndex(name));
  }

  const int kTestSocket = 123;
  EXPECT_CALL(*sockets_, Socket(PF_INET, SOCK_DGRAM, 0))
      .Times(3)
      .WillOnce(Return(-1))
      .WillRepeatedly(Return(kTestSocket));
  EXPECT_CALL(*sockets_, Ioctl(kTestSocket, SIOCGIFINDEX, _))
      .WillOnce(Return(-1))
      .WillOnce(DoAll(SetInterfaceIndex(), Return(0)));
  EXPECT_CALL(*sockets_, Close(kTestSocket))
      .Times(2)
      .WillRepeatedly(Return(0));
  EXPECT_EQ(-1, RTNLHandler::GetInstance()->GetInterfaceIndex("eth0"));
  EXPECT_EQ(-1, RTNLHandler::GetInstance()->GetInterfaceIndex("wlan0"));
  EXPECT_EQ(kTestInterfaceIndex,
            RTNLHandler::GetInstance()->GetInterfaceIndex("usb0"));
}

}  // namespace shill
