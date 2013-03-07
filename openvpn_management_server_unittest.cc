// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/openvpn_management_server.h"

#include <netinet/in.h>

#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest.h>

#include "shill/glib.h"
#include "shill/key_value_store.h"
#include "shill/mock_event_dispatcher.h"
#include "shill/mock_openvpn_driver.h"
#include "shill/mock_sockets.h"

using base::Bind;
using base::Unretained;
using std::string;
using std::vector;
using testing::_;
using testing::Assign;
using testing::InSequence;
using testing::Return;
using testing::ReturnNew;

namespace shill {

namespace {
MATCHER_P(VoidStringEq, value, "") {
  return value == reinterpret_cast<const char *>(arg);
}
}  // namespace {}

class OpenVPNManagementServerTest : public testing::Test {
 public:
  OpenVPNManagementServerTest()
      : server_(&driver_, &glib_) {}

  virtual ~OpenVPNManagementServerTest() {}

 protected:
  static const int kConnectedSocket;

  void SetSockets() { server_.sockets_ = &sockets_; }
  void SetDispatcher() { server_.dispatcher_ = &dispatcher_; }
  void ExpectNotStarted() { EXPECT_FALSE(server_.IsStarted()); }

  void SetConnectedSocket() {
    server_.connected_socket_ = kConnectedSocket;
    SetSockets();
  }

  void ExpectSend(const string &value) {
    EXPECT_CALL(sockets_,
                Send(kConnectedSocket, VoidStringEq(value), value.size(), 0))
        .WillOnce(Return(value.size()));
  }

  void ExpectStaticChallengeResponse() {
    driver_.args()->SetString(flimflam::kOpenVPNUserProperty, "jojo");
    driver_.args()->SetString(flimflam::kOpenVPNPasswordProperty, "yoyo");
    driver_.args()->SetString(flimflam::kOpenVPNOTPProperty, "123456");
    SetConnectedSocket();
    ExpectSend("username \"Auth\" jojo\n");
    ExpectSend("password \"Auth\" \"SCRV1:eW95bw==:MTIzNDU2\"\n");
  }

  void ExpectAuthenticationResponse() {
    driver_.args()->SetString(flimflam::kOpenVPNUserProperty, "jojo");
    driver_.args()->SetString(flimflam::kOpenVPNPasswordProperty, "yoyo");
    SetConnectedSocket();
    ExpectSend("username \"Auth\" jojo\n");
    ExpectSend("password \"Auth\" \"yoyo\"\n");
  }

  void ExpectPINResponse() {
    driver_.args()->SetString(flimflam::kOpenVPNPinProperty, "987654");
    SetConnectedSocket();
    ExpectSend("password \"User-Specific TPM Token FOO\" \"987654\"\n");
  }

  void ExpectHoldRelease() {
    SetConnectedSocket();
    ExpectSend("hold release\n");
  }

  void ExpectRestart() {
    SetConnectedSocket();
    ExpectSend("signal SIGUSR1\n");
  }

  InputData CreateInputDataFromString(const string &str) {
    InputData data(
        reinterpret_cast<unsigned char *>(const_cast<char *>(str.data())),
        str.size());
    return data;
  }

  void SendSignal(const string &signal) {
    server_.SendSignal(signal);
  }

  void OnInput(InputData *data) {
    server_.OnInput(data);
  }

  void ProcessMessage(const string &message) {
    server_.ProcessMessage(message);
  }

  bool ProcessSuccessMessage(const string &message) {
    return server_.ProcessSuccessMessage(message);
  }

  bool ProcessStateMessage(const string &message) {
    return server_.ProcessStateMessage(message);
  }

  bool ProcessAuthTokenMessage(const string &message) {
    return server_.ProcessAuthTokenMessage(message);
  }

  bool GetHoldWaiting() { return server_.hold_waiting_; }

  GLib glib_;
  MockOpenVPNDriver driver_;
  OpenVPNManagementServer server_;
  MockSockets sockets_;
  MockEventDispatcher dispatcher_;
};

// static
const int OpenVPNManagementServerTest::kConnectedSocket = 555;

TEST_F(OpenVPNManagementServerTest, StartStarted) {
  SetSockets();
  EXPECT_TRUE(server_.Start(NULL, NULL, NULL));
}

TEST_F(OpenVPNManagementServerTest, StartSocketFail) {
  EXPECT_CALL(sockets_, Socket(AF_INET, SOCK_STREAM, IPPROTO_TCP))
      .WillOnce(Return(-1));
  EXPECT_FALSE(server_.Start(NULL, &sockets_, NULL));
  ExpectNotStarted();
}

TEST_F(OpenVPNManagementServerTest, StartGetSockNameFail) {
  const int kSocket = 123;
  EXPECT_CALL(sockets_, Socket(AF_INET, SOCK_STREAM, IPPROTO_TCP))
      .WillOnce(Return(kSocket));
  EXPECT_CALL(sockets_, Bind(kSocket, _, _)).WillOnce(Return(0));
  EXPECT_CALL(sockets_, Listen(kSocket, 1)).WillOnce(Return(0));
  EXPECT_CALL(sockets_, GetSockName(kSocket, _, _)).WillOnce(Return(-1));
  EXPECT_CALL(sockets_, Close(kSocket)).WillOnce(Return(0));
  EXPECT_FALSE(server_.Start(NULL, &sockets_, NULL));
  ExpectNotStarted();
}

TEST_F(OpenVPNManagementServerTest, Start) {
  const int kSocket = 123;
  EXPECT_CALL(sockets_, Socket(AF_INET, SOCK_STREAM, IPPROTO_TCP))
      .WillOnce(Return(kSocket));
  EXPECT_CALL(sockets_, Bind(kSocket, _, _)).WillOnce(Return(0));
  EXPECT_CALL(sockets_, Listen(kSocket, 1)).WillOnce(Return(0));
  EXPECT_CALL(sockets_, GetSockName(kSocket, _, _)).WillOnce(Return(0));
  EXPECT_CALL(dispatcher_,
              CreateReadyHandler(kSocket, IOHandler::kModeInput, _))
      .WillOnce(ReturnNew<IOHandler>());
  vector<string> options;
  EXPECT_TRUE(server_.Start(&dispatcher_, &sockets_, &options));
  EXPECT_EQ(&sockets_, server_.sockets_);
  EXPECT_EQ(kSocket, server_.socket_);
  EXPECT_TRUE(server_.ready_handler_.get());
  EXPECT_EQ(&dispatcher_, server_.dispatcher_);
  EXPECT_FALSE(options.empty());
}

TEST_F(OpenVPNManagementServerTest, Stop) {
  SetSockets();
  server_.input_handler_.reset(new IOHandler());
  const int kConnectedSocket = 234;
  server_.connected_socket_ = kConnectedSocket;
  EXPECT_CALL(sockets_, Close(kConnectedSocket)).WillOnce(Return(0));
  SetDispatcher();
  server_.ready_handler_.reset(new IOHandler());
  const int kSocket = 345;
  server_.socket_ = kSocket;
  EXPECT_CALL(sockets_, Close(kSocket)).WillOnce(Return(0));
  server_.Stop();
  EXPECT_FALSE(server_.input_handler_.get());
  EXPECT_EQ(-1, server_.connected_socket_);
  EXPECT_FALSE(server_.dispatcher_);
  EXPECT_FALSE(server_.ready_handler_.get());
  EXPECT_EQ(-1, server_.socket_);
  ExpectNotStarted();
}

TEST_F(OpenVPNManagementServerTest, OnReadyAcceptFail) {
  const int kSocket = 333;
  SetSockets();
  EXPECT_CALL(sockets_, Accept(kSocket, NULL, NULL)).WillOnce(Return(-1));
  server_.OnReady(kSocket);
  EXPECT_EQ(-1, server_.connected_socket_);
}

TEST_F(OpenVPNManagementServerTest, OnReady) {
  const int kSocket = 111;
  SetConnectedSocket();
  SetDispatcher();
  EXPECT_CALL(sockets_, Accept(kSocket, NULL, NULL))
      .WillOnce(Return(kConnectedSocket));
  server_.ready_handler_.reset(new IOHandler());
  EXPECT_CALL(dispatcher_, CreateInputHandler(kConnectedSocket, _, _))
      .WillOnce(ReturnNew<IOHandler>());
  ExpectSend("state on\n");
  server_.OnReady(kSocket);
  EXPECT_EQ(kConnectedSocket, server_.connected_socket_);
  EXPECT_FALSE(server_.ready_handler_.get());
  EXPECT_TRUE(server_.input_handler_.get());
}

TEST_F(OpenVPNManagementServerTest, OnInput) {
  {
    string s;
    InputData data = CreateInputDataFromString(s);
    OnInput(&data);
  }
  {
    string s = "foo\n"
        ">INFO:...\n"
        ">PASSWORD:Need 'Auth' SC:user/password/otp\n"
        ">PASSWORD:Need 'User-Specific TPM Token FOO' ...\n"
        ">PASSWORD:Verification Failed: .\n"
        ">PASSWORD:Auth-Token:ToKeN==\n"
        ">STATE:123,RECONNECTING,detail,...,...\n"
        ">HOLD:Waiting for hold release\n"
        "SUCCESS: Hold released.";
    InputData data = CreateInputDataFromString(s);
    ExpectStaticChallengeResponse();
    ExpectPINResponse();
    EXPECT_CALL(driver_, Cleanup(Service::kStateFailure));
    EXPECT_CALL(driver_, OnReconnecting(_));
    EXPECT_FALSE(GetHoldWaiting());
    OnInput(&data);
    EXPECT_TRUE(GetHoldWaiting());
  }
}

TEST_F(OpenVPNManagementServerTest, OnInputStop) {
  string s =
      ">PASSWORD:Verification Failed: .\n"
      ">STATE:123,RECONNECTING,detail,...,...";
  InputData data = CreateInputDataFromString(s);
  SetSockets();
  // Stops the server after the first message is processed.
  EXPECT_CALL(driver_, Cleanup(Service::kStateFailure))
      .WillOnce(Assign(&server_.sockets_, reinterpret_cast<Sockets *>(NULL)));
  // The second message should not be processed.
  EXPECT_CALL(driver_, OnReconnecting(_)).Times(0);
  OnInput(&data);
}

TEST_F(OpenVPNManagementServerTest, ProcessMessage) {
  ProcessMessage("foo");
  ProcessMessage(">INFO:");

  EXPECT_CALL(driver_, OnReconnecting(_));
  ProcessMessage(">STATE:123,RECONNECTING,detail,...,...");
}

TEST_F(OpenVPNManagementServerTest, ProcessSuccessMessage) {
  EXPECT_FALSE(ProcessSuccessMessage("foo"));
  EXPECT_TRUE(ProcessSuccessMessage("SUCCESS: foo"));
}

TEST_F(OpenVPNManagementServerTest, ProcessInfoMessage) {
  EXPECT_FALSE(server_.ProcessInfoMessage("foo"));
  EXPECT_TRUE(server_.ProcessInfoMessage(">INFO:foo"));
}

TEST_F(OpenVPNManagementServerTest, ProcessStateMessage) {
  EXPECT_FALSE(ProcessStateMessage("foo"));
  EXPECT_TRUE(ProcessStateMessage(">STATE:123,WAIT,detail,...,..."));
  {
    InSequence seq;
    EXPECT_CALL(driver_,
                OnReconnecting(OpenVPNDriver::kReconnectReasonUnknown));
    EXPECT_CALL(driver_,
                OnReconnecting(OpenVPNDriver::kReconnectReasonTLSError));
  }
  EXPECT_TRUE(ProcessStateMessage(">STATE:123,RECONNECTING,detail,...,..."));
  EXPECT_TRUE(ProcessStateMessage(">STATE:123,RECONNECTING,tls-error,...,..."));
}

TEST_F(OpenVPNManagementServerTest, ProcessNeedPasswordMessageAuthSC) {
  ExpectStaticChallengeResponse();
  EXPECT_TRUE(
      server_.ProcessNeedPasswordMessage(
          ">PASSWORD:Need 'Auth' SC:user/password/otp"));
  EXPECT_FALSE(driver_.args()->ContainsString(flimflam::kOpenVPNOTPProperty));
}

TEST_F(OpenVPNManagementServerTest, ProcessNeedPasswordMessageAuth) {
  ExpectAuthenticationResponse();
  EXPECT_TRUE(
      server_.ProcessNeedPasswordMessage(
          ">PASSWORD:Need 'Auth' username/password"));
}

TEST_F(OpenVPNManagementServerTest, ProcessNeedPasswordMessageTPMToken) {
  ExpectPINResponse();
  EXPECT_TRUE(
      server_.ProcessNeedPasswordMessage(
          ">PASSWORD:Need 'User-Specific TPM Token FOO' ..."));
}

TEST_F(OpenVPNManagementServerTest, ProcessNeedPasswordMessageUnknown) {
  EXPECT_FALSE(server_.ProcessNeedPasswordMessage("foo"));
}

TEST_F(OpenVPNManagementServerTest, ParseNeedPasswordTag) {
  EXPECT_EQ("", OpenVPNManagementServer::ParseNeedPasswordTag(""));
  EXPECT_EQ("", OpenVPNManagementServer::ParseNeedPasswordTag(" "));
  EXPECT_EQ("", OpenVPNManagementServer::ParseNeedPasswordTag("'"));
  EXPECT_EQ("", OpenVPNManagementServer::ParseNeedPasswordTag("''"));
  EXPECT_EQ("bar",
            OpenVPNManagementServer::ParseNeedPasswordTag("foo'bar'zoo"));
  EXPECT_EQ("bar", OpenVPNManagementServer::ParseNeedPasswordTag("foo'bar'"));
  EXPECT_EQ("bar", OpenVPNManagementServer::ParseNeedPasswordTag("'bar'zoo"));
  EXPECT_EQ("bar",
            OpenVPNManagementServer::ParseNeedPasswordTag("foo'bar'zoo'moo"));
}

TEST_F(OpenVPNManagementServerTest, PerformStaticChallengeNoCreds) {
  EXPECT_CALL(driver_, Cleanup(Service::kStateFailure)).Times(3);
  server_.PerformStaticChallenge("Auth");
  driver_.args()->SetString(flimflam::kOpenVPNUserProperty, "jojo");
  server_.PerformStaticChallenge("Auth");
  driver_.args()->SetString(flimflam::kOpenVPNPasswordProperty, "yoyo");
  server_.PerformStaticChallenge("Auth");
}

TEST_F(OpenVPNManagementServerTest, PerformStaticChallenge) {
  ExpectStaticChallengeResponse();
  server_.PerformStaticChallenge("Auth");
  EXPECT_FALSE(driver_.args()->ContainsString(flimflam::kOpenVPNOTPProperty));
}

TEST_F(OpenVPNManagementServerTest, PerformAuthenticationNoCreds) {
  EXPECT_CALL(driver_, Cleanup(Service::kStateFailure)).Times(2);
  server_.PerformAuthentication("Auth");
  driver_.args()->SetString(flimflam::kOpenVPNUserProperty, "jojo");
  server_.PerformAuthentication("Auth");
}

TEST_F(OpenVPNManagementServerTest, PerformAuthentication) {
  ExpectAuthenticationResponse();
  server_.PerformAuthentication("Auth");
}

TEST_F(OpenVPNManagementServerTest, ProcessHoldMessage) {
  EXPECT_FALSE(server_.hold_release_);
  EXPECT_FALSE(server_.hold_waiting_);

  EXPECT_FALSE(server_.ProcessHoldMessage("foo"));

  EXPECT_TRUE(server_.ProcessHoldMessage(">HOLD:Waiting for hold release"));
  EXPECT_FALSE(server_.hold_release_);
  EXPECT_TRUE(server_.hold_waiting_);

  ExpectHoldRelease();
  server_.hold_release_ = true;
  server_.hold_waiting_ = false;
  EXPECT_TRUE(server_.ProcessHoldMessage(">HOLD:Waiting for hold release"));
  EXPECT_TRUE(server_.hold_release_);
  EXPECT_FALSE(server_.hold_waiting_);
}

TEST_F(OpenVPNManagementServerTest, SupplyTPMTokenNoPIN) {
  EXPECT_CALL(driver_, Cleanup(Service::kStateFailure));
  server_.SupplyTPMToken("User-Specific TPM Token FOO");
}

TEST_F(OpenVPNManagementServerTest, SupplyTPMToken) {
  ExpectPINResponse();
  server_.SupplyTPMToken("User-Specific TPM Token FOO");
}

TEST_F(OpenVPNManagementServerTest, Send) {
  const char kMessage[] = "foo\n";
  SetConnectedSocket();
  ExpectSend(kMessage);
  server_.Send(kMessage);
}

TEST_F(OpenVPNManagementServerTest, SendState) {
  SetConnectedSocket();
  ExpectSend("state off\n");
  server_.SendState("off");
}

TEST_F(OpenVPNManagementServerTest, SendUsername) {
  SetConnectedSocket();
  ExpectSend("username \"Auth\" joesmith\n");
  server_.SendUsername("Auth", "joesmith");
}

TEST_F(OpenVPNManagementServerTest, SendPassword) {
  SetConnectedSocket();
  ExpectSend("password \"Auth\" \"foo\\\"bar\"\n");
  server_.SendPassword("Auth", "foo\"bar");
}

TEST_F(OpenVPNManagementServerTest, ProcessFailedPasswordMessage) {
  EXPECT_FALSE(server_.ProcessFailedPasswordMessage("foo"));
  EXPECT_CALL(driver_, Cleanup(Service::kStateFailure));
  EXPECT_TRUE(
      server_.ProcessFailedPasswordMessage(">PASSWORD:Verification Failed: ."));
}

TEST_F(OpenVPNManagementServerTest, ProcessAuthTokenMessage) {
  EXPECT_FALSE(ProcessAuthTokenMessage("foo"));
  EXPECT_TRUE(ProcessAuthTokenMessage(">PASSWORD:Auth-Token:ToKeN=="));
}

TEST_F(OpenVPNManagementServerTest, SendSignal) {
  SetConnectedSocket();
  ExpectSend("signal SIGUSR2\n");
  SendSignal("SIGUSR2");
}

TEST_F(OpenVPNManagementServerTest, Restart) {
  ExpectRestart();
  server_.Restart();
}

TEST_F(OpenVPNManagementServerTest, SendHoldRelease) {
  ExpectHoldRelease();
  server_.SendHoldRelease();
}

TEST_F(OpenVPNManagementServerTest, Hold) {
  EXPECT_FALSE(server_.hold_release_);
  EXPECT_FALSE(server_.hold_waiting_);

  server_.ReleaseHold();
  EXPECT_TRUE(server_.hold_release_);
  EXPECT_FALSE(server_.hold_waiting_);

  server_.Hold();
  EXPECT_FALSE(server_.hold_release_);
  EXPECT_FALSE(server_.hold_waiting_);

  server_.hold_waiting_ = true;
  ExpectHoldRelease();
  server_.ReleaseHold();
  EXPECT_TRUE(server_.hold_release_);
  EXPECT_FALSE(server_.hold_waiting_);
}

TEST_F(OpenVPNManagementServerTest, EscapeToQuote) {
  EXPECT_EQ("", OpenVPNManagementServer::EscapeToQuote(""));
  EXPECT_EQ("foo './", OpenVPNManagementServer::EscapeToQuote("foo './"));
  EXPECT_EQ("\\\\", OpenVPNManagementServer::EscapeToQuote("\\"));
  EXPECT_EQ("\\\"", OpenVPNManagementServer::EscapeToQuote("\""));
  EXPECT_EQ("\\\\\\\"foo\\\\bar\\\"",
            OpenVPNManagementServer::EscapeToQuote("\\\"foo\\bar\""));
}

}  // namespace shill
