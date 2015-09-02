//
// Copyright (C) 2014 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "shill/dbus_name_watcher.h"

#include <base/bind.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "shill/dbus_manager.h"

using base::Bind;
using base::Unretained;
using std::string;
using std::unique_ptr;
using testing::_;

namespace shill {

namespace {

const char kName[] = "org.chromium.Service";
const char kOwner[] = ":1.10";

}  // namespace

class DBusNameWatcherCallbackObserver {
 public:
  DBusNameWatcherCallbackObserver()
      : name_appeared_callback_(
            Bind(&DBusNameWatcherCallbackObserver::OnNameAppeared,
                 Unretained(this))),
        name_vanished_callback_(
            Bind(&DBusNameWatcherCallbackObserver::OnNameVanished,
                 Unretained(this))) {}

  virtual ~DBusNameWatcherCallbackObserver() {}

  MOCK_CONST_METHOD2(OnNameAppeared, void(const string& name,
                                          const string& owner));
  MOCK_CONST_METHOD1(OnNameVanished, void(const string& name));

  const DBusNameWatcher::NameAppearedCallback& name_appeared_callback()
      const {
    return name_appeared_callback_;
  }

  const DBusNameWatcher::NameVanishedCallback& name_vanished_callback()
      const {
    return name_vanished_callback_;
  }

 private:
  DBusNameWatcher::NameAppearedCallback name_appeared_callback_;
  DBusNameWatcher::NameVanishedCallback name_vanished_callback_;

  DISALLOW_COPY_AND_ASSIGN(DBusNameWatcherCallbackObserver);
};

class DBusNameWatcherTest : public testing::Test {
 protected:
  DBusNameWatcherTest() : dbus_manager_(new DBusManager(nullptr)) {}

  unique_ptr<DBusManager> dbus_manager_;
  unique_ptr<DBusNameWatcher> watcher_;
};

TEST_F(DBusNameWatcherTest, DestructAfterDBusManager) {
  watcher_.reset(new DBusNameWatcher(dbus_manager_.get(),
                                     kName,
                                     DBusNameWatcher::NameAppearedCallback(),
                                     DBusNameWatcher::NameVanishedCallback()));
  dbus_manager_.reset();
  // Ensure no crash if |dbus_manager_| is destructed before |watcher_| is
  // destructed.
  watcher_.reset();
}

TEST_F(DBusNameWatcherTest, DestructBeforeDBusManager) {
  watcher_.reset(new DBusNameWatcher(dbus_manager_.get(),
                                     kName,
                                     DBusNameWatcher::NameAppearedCallback(),
                                     DBusNameWatcher::NameVanishedCallback()));
  watcher_.reset();
  dbus_manager_.reset();
}

TEST_F(DBusNameWatcherTest, OnNameAppearedOrVanished) {
  DBusNameWatcherCallbackObserver observer;
  watcher_.reset(new DBusNameWatcher(dbus_manager_.get(),
                                     kName,
                                     observer.name_appeared_callback(),
                                     observer.name_vanished_callback()));
  EXPECT_CALL(observer, OnNameAppeared(kName, kOwner));
  watcher_->OnNameOwnerChanged(kOwner);
  EXPECT_CALL(observer, OnNameVanished(kName));
  watcher_->OnNameOwnerChanged(string());
}

TEST_F(DBusNameWatcherTest, OnNameAppearedOrVanishedWithoutCallback) {
  watcher_.reset(new DBusNameWatcher(dbus_manager_.get(),
                                     kName,
                                     DBusNameWatcher::NameAppearedCallback(),
                                     DBusNameWatcher::NameVanishedCallback()));
  watcher_->OnNameOwnerChanged(kOwner);
  watcher_->OnNameOwnerChanged(string());
}

}  // namespace shill