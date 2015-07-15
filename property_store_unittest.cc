// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/property_store.h"

#include "shill/property_store_unittest.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include <base/macros.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus-c++/dbus.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "shill/dbus_adaptor.h"
#include "shill/error.h"
#include "shill/event_dispatcher.h"
#include "shill/manager.h"
#include "shill/mock_control.h"
#include "shill/property_accessor.h"

using base::Bind;
using base::Unretained;
using std::map;
using std::string;
using std::vector;
using ::testing::_;
using ::testing::Return;
using ::testing::Values;

namespace shill {

// static
const ::DBus::Variant PropertyStoreTest::kBoolV = DBusAdaptor::BoolToVariant(0);
// static
const ::DBus::Variant PropertyStoreTest::kByteV = DBusAdaptor::ByteToVariant(0);
// static
const ::DBus::Variant PropertyStoreTest::kInt16V =
    DBusAdaptor::Int16ToVariant(0);
// static
const ::DBus::Variant PropertyStoreTest::kInt32V =
    DBusAdaptor::Int32ToVariant(0);
// static
const ::DBus::Variant PropertyStoreTest::kKeyValueStoreV =
    DBusAdaptor::KeyValueStoreToVariant(KeyValueStore());
// static
const ::DBus::Variant PropertyStoreTest::kStringV =
    DBusAdaptor::StringToVariant("");
// static
const ::DBus::Variant PropertyStoreTest::kStringmapV =
    DBusAdaptor::StringmapToVariant(Stringmap());
// static
const ::DBus::Variant PropertyStoreTest::kStringmapsV =
    DBusAdaptor::StringmapsToVariant(Stringmaps());
// static
const ::DBus::Variant PropertyStoreTest::kStringsV =
    DBusAdaptor::StringsToVariant(Strings(1, ""));
// static
const ::DBus::Variant PropertyStoreTest::kUint16V =
    DBusAdaptor::Uint16ToVariant(0);
// static
const ::DBus::Variant PropertyStoreTest::kUint16sV =
    DBusAdaptor::Uint16sToVariant(Uint16s{0});
// static
const ::DBus::Variant PropertyStoreTest::kUint32V =
    DBusAdaptor::Uint32ToVariant(0);
// static
const ::DBus::Variant PropertyStoreTest::kUint64V =
    DBusAdaptor::Uint64ToVariant(0);

PropertyStoreTest::PropertyStoreTest()
    : internal_error_(kErrorResultInternalError),
      invalid_args_(kErrorResultInvalidArguments),
      invalid_prop_(kErrorResultInvalidProperty),
      path_(dir_.CreateUniqueTempDir() ? dir_.path().value() : ""),
      metrics_(dispatcher()),
      default_technology_order_{Technology::kVPN,
                                Technology::kEthernet,
                                Technology::kWifi,
                                Technology::kWiMax,
                                Technology::kCellular},
      manager_(control_interface(),
               dispatcher(),
               metrics(),
               glib(),
               run_path(),
               storage_path(),
               string()) {
}

PropertyStoreTest::~PropertyStoreTest() {}

void PropertyStoreTest::SetUp() {
  ASSERT_FALSE(run_path().empty());
  ASSERT_FALSE(storage_path().empty());
}

TEST_P(PropertyStoreTest, SetPropertyNonexistent) {
  // Ensure that an attempt to write unknown properties returns
  // InvalidProperty, and does not yield a PropertyChange callback.
  PropertyStore store(Bind(&PropertyStoreTest::TestCallback,
                           Unretained(this)));
  ::DBus::Error error;
  EXPECT_CALL(*this, TestCallback(_)).Times(0);
  EXPECT_FALSE(DBusAdaptor::SetProperty(&store, "", GetParam(), &error));
  EXPECT_EQ(invalid_prop(), error.name());
}

INSTANTIATE_TEST_CASE_P(
    PropertyStoreTestInstance,
    PropertyStoreTest,
    Values(PropertyStoreTest::kBoolV,
           PropertyStoreTest::kByteV,
           PropertyStoreTest::kInt16V,
           PropertyStoreTest::kInt32V,
           PropertyStoreTest::kStringV,
           PropertyStoreTest::kStringmapV,
           PropertyStoreTest::kStringsV,
           PropertyStoreTest::kUint16V,
           PropertyStoreTest::kUint16sV,
           PropertyStoreTest::kUint32V,
           PropertyStoreTest::kUint64V));

template <typename T>
class PropertyStoreTypedTest : public PropertyStoreTest {
 protected:
  bool SetProperty(
      PropertyStore* store, const string& name, Error* error);
};

TYPED_TEST_CASE(PropertyStoreTypedTest, PropertyStoreTest::PropertyTypes);

TYPED_TEST(PropertyStoreTypedTest, RegisterProperty) {
  PropertyStore store(Bind(&PropertyStoreTest::TestCallback,
                           Unretained(this)));
  Error error;
  TypeParam property;
  PropertyStoreTest::RegisterProperty(&store, "some property", &property);
  EXPECT_TRUE(store.Contains("some property"));
}

TYPED_TEST(PropertyStoreTypedTest, GetProperty) {
  PropertyStore store(Bind(&PropertyStoreTest::TestCallback,
                           Unretained(this)));
  Error error;
  TypeParam property{};  // value-initialize primitives
  PropertyStoreTest::RegisterProperty(&store, "some property", &property);

  TypeParam read_value;
  EXPECT_CALL(*this, TestCallback(_)).Times(0);
  EXPECT_TRUE(PropertyStoreTest::GetProperty(
      store, "some property", &read_value, &error));
  EXPECT_EQ(property, read_value);
}

TYPED_TEST(PropertyStoreTypedTest, ClearProperty) {
  PropertyStore store(Bind(&PropertyStoreTest::TestCallback,
                           Unretained(this)));
  Error error;
  TypeParam property;
  PropertyStoreTest::RegisterProperty(&store, "some property", &property);
  EXPECT_CALL(*this, TestCallback(_));
  EXPECT_TRUE(store.ClearProperty("some property", &error));
}

TYPED_TEST(PropertyStoreTypedTest, SetProperty) {
  PropertyStore store(Bind(&PropertyStoreTest::TestCallback,
                           Unretained(this)));
  Error error;
  TypeParam property{};  // value-initialize primitives
  PropertyStoreTest::RegisterProperty(&store, "some property", &property);

  // Change the value from the default (initialized above).  Should
  // generate a change callback. The second SetProperty, however,
  // should not. Hence, we should get exactly one callback.
  EXPECT_CALL(*this, TestCallback(_)).Times(1);
  EXPECT_TRUE(this->SetProperty(&store, "some property", &error));
  EXPECT_FALSE(this->SetProperty(&store, "some property", &error));
}

template<> bool PropertyStoreTypedTest<bool>::SetProperty(
    PropertyStore* store, const string& name, Error* error) {
  bool new_value = true;
  return store->SetBoolProperty(name, new_value, error);
}

template<> bool PropertyStoreTypedTest<int16_t>::SetProperty(
    PropertyStore* store, const string& name, Error* error) {
  int16_t new_value = 1;
  return store->SetInt16Property(name, new_value, error);
}

template<> bool PropertyStoreTypedTest<int32_t>::SetProperty(
    PropertyStore* store, const string& name, Error* error) {
  int32_t new_value = 1;
  return store->SetInt32Property(name, new_value, error);
}

template<> bool PropertyStoreTypedTest<string>::SetProperty(
    PropertyStore* store, const string& name, Error* error) {
  string new_value = "new value";
  return store->SetStringProperty(name, new_value, error);
}

template<> bool PropertyStoreTypedTest<Stringmap>::SetProperty(
    PropertyStore* store, const string& name, Error* error) {
  Stringmap new_value;
  new_value["new key"] = "new value";
  return store->SetStringmapProperty(name, new_value, error);
}

template<> bool PropertyStoreTypedTest<Stringmaps>::SetProperty(
    PropertyStore* store, const string& name, Error* error) {
  Stringmaps new_value(1);
  new_value[0]["new key"] = "new value";
  return store->SetStringmapsProperty(name, new_value, error);
}

template<> bool PropertyStoreTypedTest<Strings>::SetProperty(
    PropertyStore* store, const string& name, Error* error) {
  Strings new_value(1);
  new_value[0] = "new value";
  return store->SetStringsProperty(name, new_value, error);
}

template<> bool PropertyStoreTypedTest<uint8_t>::SetProperty(
    PropertyStore* store, const string& name, Error* error) {
  uint8_t new_value = 1;
  return store->SetUint8Property(name, new_value, error);
}

template<> bool PropertyStoreTypedTest<uint16_t>::SetProperty(
    PropertyStore* store, const string& name, Error* error) {
  uint16_t new_value = 1;
  return store->SetUint16Property(name, new_value, error);
}

template<> bool PropertyStoreTypedTest<Uint16s>::SetProperty(
    PropertyStore* store, const string& name, Error* error) {
  Uint16s new_value{1};
  return store->SetUint16sProperty(name, new_value, error);
}

template<> bool PropertyStoreTypedTest<uint32_t>::SetProperty(
    PropertyStore* store, const string& name, Error* error) {
  uint32_t new_value = 1;
  return store->SetUint32Property(name, new_value, error);
}

TEST_F(PropertyStoreTest, ClearBoolProperty) {
  // We exercise both possibilities for the default value here,
  // to ensure that Clear actually resets the property based on
  // the property's initial value (rather than the language's
  // default value for the type).
  static const bool kDefaults[] = {true, false};
  for (size_t i = 0; i < arraysize(kDefaults); ++i) {
    PropertyStore store;
    Error error;

    const bool default_value = kDefaults[i];
    bool flag = default_value;
    store.RegisterBool("some bool", &flag);

    EXPECT_TRUE(store.ClearProperty("some bool", &error));
    EXPECT_EQ(default_value, flag);
  }
}

TEST_F(PropertyStoreTest, ClearPropertyNonexistent) {
  PropertyStore store(Bind(&PropertyStoreTest::TestCallback,
                           Unretained(this)));
  Error error;

  EXPECT_CALL(*this, TestCallback(_)).Times(0);
  EXPECT_FALSE(store.ClearProperty("", &error));
  EXPECT_EQ(Error::kInvalidProperty, error.type());
}

// Separate from SetPropertyNonexistent, because
// DBusAdaptor::SetProperty doesn't support Stringmaps.
TEST_F(PropertyStoreTest, SetStringmapsProperty) {
  PropertyStore store(Bind(&PropertyStoreTest::TestCallback,
                           Unretained(this)));
  ::DBus::Error error;
  EXPECT_CALL(*this, TestCallback(_)).Times(0);
  EXPECT_FALSE(DBusAdaptor::SetProperty(
      &store, "", PropertyStoreTest::kStringmapsV, &error));
  EXPECT_EQ(internal_error(), error.name());
}

// KeyValueStoreProperty is only defined for derived types so handle
// this case manually here.
TEST_F(PropertyStoreTest, KeyValueStorePropertyNonExistent) {
  PropertyStore store(Bind(&PropertyStoreTest::TestCallback,
                           Unretained(this)));
  ::DBus::Error error;
  EXPECT_CALL(*this, TestCallback(_)).Times(0);
  EXPECT_FALSE(DBusAdaptor::SetProperty(
      &store, "", PropertyStoreTest::kKeyValueStoreV, &error));
  EXPECT_EQ(invalid_prop(), error.name());
}

TEST_F(PropertyStoreTest, KeyValueStoreProperty) {
  PropertyStore store(Bind(&PropertyStoreTest::TestCallback,
                           Unretained(this)));
  const char kKey[] = "key";
  EXPECT_CALL(*this, GetKeyValueStoreCallback(_))
      .WillOnce(Return(KeyValueStore()));
  store.RegisterDerivedKeyValueStore(
      kKey,
      KeyValueStoreAccessor(
          new CustomAccessor<PropertyStoreTest, KeyValueStore>(
              this, &PropertyStoreTest::GetKeyValueStoreCallback,
              &PropertyStoreTest::SetKeyValueStoreCallback)));
  EXPECT_CALL(*this, TestCallback(_));
  EXPECT_CALL(*this, SetKeyValueStoreCallback(_, _)).WillOnce(Return(true));
  ::DBus::Error error;
  EXPECT_TRUE(DBusAdaptor::SetProperty(&store, kKey, kKeyValueStoreV, &error));
}

TEST_F(PropertyStoreTest, WriteOnlyProperties) {
  // Test that properties registered as write-only are not returned
  // when using Get*PropertiesIter().
  PropertyStore store;
  {
    const string keys[]  = {"boolp1", "boolp2"};
    bool values[] = {true, true};
    store.RegisterWriteOnlyBool(keys[0], &values[0]);
    store.RegisterBool(keys[1], &values[1]);

    ReadablePropertyConstIterator<bool> it = store.GetBoolPropertiesIter();
    EXPECT_FALSE(it.AtEnd());
    EXPECT_EQ(keys[1], it.Key());
    EXPECT_TRUE(values[1] == it.value());
    it.Advance();
    EXPECT_TRUE(it.AtEnd());

    Error errors[2];
    EXPECT_FALSE(store.GetBoolProperty(keys[0], nullptr, &errors[0]));
    EXPECT_EQ(Error::kPermissionDenied, errors[0].type());
    bool test_value;
    EXPECT_TRUE(store.GetBoolProperty(keys[1], &test_value, &errors[1]));
    EXPECT_TRUE(errors[1].IsSuccess());
    EXPECT_EQ(values[1], test_value);
  }
  {
    const string keys[] = {"int16p1", "int16p2"};
    int16_t values[] = {127, 128};
    store.RegisterWriteOnlyInt16(keys[0], &values[0]);
    store.RegisterInt16(keys[1], &values[1]);

    ReadablePropertyConstIterator<int16_t> it = store.GetInt16PropertiesIter();
    EXPECT_FALSE(it.AtEnd());
    EXPECT_EQ(keys[1], it.Key());
    EXPECT_EQ(values[1], it.value());
    it.Advance();
    EXPECT_TRUE(it.AtEnd());

    Error errors[2];
    EXPECT_FALSE(store.GetInt16Property(keys[0], nullptr, &errors[0]));
    EXPECT_EQ(Error::kPermissionDenied, errors[0].type());
    int16_t test_value;
    EXPECT_TRUE(store.GetInt16Property(keys[1], &test_value, &errors[1]));
    EXPECT_TRUE(errors[1].IsSuccess());
    EXPECT_EQ(values[1], test_value);
  }
  {
    const string keys[] = {"int32p1", "int32p2"};
    int32_t values[] = {127, 128};
    store.RegisterWriteOnlyInt32(keys[0], &values[0]);
    store.RegisterInt32(keys[1], &values[1]);

    ReadablePropertyConstIterator<int32_t> it = store.GetInt32PropertiesIter();
    EXPECT_FALSE(it.AtEnd());
    EXPECT_EQ(keys[1], it.Key());
    EXPECT_EQ(values[1], it.value());
    it.Advance();
    EXPECT_TRUE(it.AtEnd());

    Error errors[2];
    EXPECT_FALSE(store.GetInt32Property(keys[0], nullptr, &errors[0]));
    EXPECT_EQ(Error::kPermissionDenied, errors[0].type());
    int32_t test_value;
    EXPECT_TRUE(store.GetInt32Property(keys[1], &test_value, &errors[1]));
    EXPECT_TRUE(errors[1].IsSuccess());
    EXPECT_EQ(values[1], test_value);
  }
  {
    const string keys[] = {"stringp1", "stringp2"};
    string values[] = {"noooo", "yesss"};
    store.RegisterWriteOnlyString(keys[0], &values[0]);
    store.RegisterString(keys[1], &values[1]);

    ReadablePropertyConstIterator<string> it = store.GetStringPropertiesIter();
    EXPECT_FALSE(it.AtEnd());
    EXPECT_EQ(keys[1], it.Key());
    EXPECT_EQ(values[1], it.value());
    it.Advance();
    EXPECT_TRUE(it.AtEnd());

    Error errors[2];
    EXPECT_FALSE(store.GetStringProperty(keys[0], nullptr, &errors[0]));
    EXPECT_EQ(Error::kPermissionDenied, errors[0].type());
    string test_value;
    EXPECT_TRUE(store.GetStringProperty(keys[1], &test_value, &errors[1]));
    EXPECT_TRUE(errors[1].IsSuccess());
    EXPECT_EQ(values[1], test_value);
  }
  {
    const string keys[] = {"stringmapp1", "stringmapp2"};
    Stringmap values[2];
    values[0]["noooo"] = "yesss";
    values[1]["yesss"] = "noooo";
    store.RegisterWriteOnlyStringmap(keys[0], &values[0]);
    store.RegisterStringmap(keys[1], &values[1]);

    ReadablePropertyConstIterator<Stringmap> it =
        store.GetStringmapPropertiesIter();
    EXPECT_FALSE(it.AtEnd());
    EXPECT_EQ(keys[1], it.Key());
    EXPECT_TRUE(values[1] == it.value());
    it.Advance();
    EXPECT_TRUE(it.AtEnd());

    Error errors[2];
    EXPECT_FALSE(store.GetStringmapProperty(keys[0], nullptr, &errors[0]));
    EXPECT_EQ(Error::kPermissionDenied, errors[0].type());
    Stringmap test_value;
    EXPECT_TRUE(store.GetStringmapProperty(keys[1], &test_value, &errors[1]));
    EXPECT_TRUE(errors[1].IsSuccess());
    EXPECT_TRUE(values[1] == test_value);
  }
  {
    const string keys[] = {"stringmapsp1", "stringmapsp2"};
    Stringmaps values[2];
    Stringmap element;
    element["noooo"] = "yesss";
    values[0].push_back(element);
    element["yesss"] = "noooo";
    values[1].push_back(element);

    store.RegisterWriteOnlyStringmaps(keys[0], &values[0]);
    store.RegisterStringmaps(keys[1], &values[1]);

    ReadablePropertyConstIterator<Stringmaps> it =
        store.GetStringmapsPropertiesIter();
    EXPECT_FALSE(it.AtEnd());
    EXPECT_EQ(keys[1], it.Key());
    EXPECT_TRUE(values[1] == it.value());
    it.Advance();
    EXPECT_TRUE(it.AtEnd());

    Error errors[2];
    EXPECT_FALSE(store.GetStringmapsProperty(keys[0], nullptr, &errors[0]));
    EXPECT_EQ(Error::kPermissionDenied, errors[0].type());
    Stringmaps test_value;
    EXPECT_TRUE(store.GetStringmapsProperty(keys[1], &test_value, &errors[1]));
    EXPECT_TRUE(errors[1].IsSuccess());
    EXPECT_TRUE(values[1] == test_value);
  }
  {
    const string keys[] = {"stringsp1", "stringsp2"};
    Strings values[2];
    string element;
    element = "noooo";
    values[0].push_back(element);
    element = "yesss";
    values[1].push_back(element);
    store.RegisterWriteOnlyStrings(keys[0], &values[0]);
    store.RegisterStrings(keys[1], &values[1]);

    ReadablePropertyConstIterator<Strings> it =
        store.GetStringsPropertiesIter();
    EXPECT_FALSE(it.AtEnd());
    EXPECT_EQ(keys[1], it.Key());
    EXPECT_TRUE(values[1] == it.value());
    it.Advance();
    EXPECT_TRUE(it.AtEnd());

    Error errors[2];
    EXPECT_FALSE(store.GetStringsProperty(keys[0], nullptr, &errors[0]));
    EXPECT_EQ(Error::kPermissionDenied, errors[0].type());
    Strings test_value;
    EXPECT_TRUE(store.GetStringsProperty(keys[1], &test_value, &errors[1]));
    EXPECT_TRUE(errors[1].IsSuccess());
    EXPECT_TRUE(values[1] == test_value);
  }
  {
    const string keys[] = {"uint8p1", "uint8p2"};
    uint8_t values[] = {127, 128};
    store.RegisterWriteOnlyUint8(keys[0], &values[0]);
    store.RegisterUint8(keys[1], &values[1]);

    ReadablePropertyConstIterator<uint8_t> it = store.GetUint8PropertiesIter();
    EXPECT_FALSE(it.AtEnd());
    EXPECT_EQ(keys[1], it.Key());
    EXPECT_EQ(values[1], it.value());
    it.Advance();
    EXPECT_TRUE(it.AtEnd());

    Error errors[2];
    EXPECT_FALSE(store.GetUint8Property(keys[0], nullptr, &errors[0]));
    EXPECT_EQ(Error::kPermissionDenied, errors[0].type());
    uint8_t test_value;
    EXPECT_TRUE(store.GetUint8Property(keys[1], &test_value, &errors[1]));
    EXPECT_TRUE(errors[1].IsSuccess());
    EXPECT_EQ(values[1], test_value);
  }
  {
    const string keys[] = {"uint16p", "uint16p1"};
    uint16_t values[] = {127, 128};
    store.RegisterWriteOnlyUint16(keys[0], &values[0]);
    store.RegisterUint16(keys[1], &values[1]);

    ReadablePropertyConstIterator<uint16_t> it =
        store.GetUint16PropertiesIter();
    EXPECT_FALSE(it.AtEnd());
    EXPECT_EQ(keys[1], it.Key());
    EXPECT_EQ(values[1], it.value());
    it.Advance();
    EXPECT_TRUE(it.AtEnd());

    Error errors[2];
    EXPECT_FALSE(store.GetUint16Property(keys[0], nullptr, &errors[0]));
    EXPECT_EQ(Error::kPermissionDenied, errors[0].type());
    uint16_t test_value;
    EXPECT_TRUE(store.GetUint16Property(keys[1], &test_value, &errors[1]));
    EXPECT_TRUE(errors[1].IsSuccess());
    EXPECT_EQ(values[1], test_value);
  }
}

TEST_F(PropertyStoreTest, SetAnyProperty) {
  // Test that registered properties can be set using chromeos::Any variant
  // type.
  PropertyStore store;
  {
    // Register property value.
    const string key = "boolp";
    bool value = true;
    store.RegisterBool(key, &value);

    // Verify property value.
    bool test_value;
    Error error;
    EXPECT_TRUE(store.GetBoolProperty(key, &test_value, &error));
    EXPECT_EQ(value, test_value);

    // Set property using chromeos::Any variant type.
    bool new_value = false;
    EXPECT_TRUE(store.SetAnyProperty(key, chromeos::Any(new_value), &error));
    EXPECT_TRUE(store.GetBoolProperty(key, &test_value, &error));
    EXPECT_EQ(new_value, test_value);
  }
  {
    // Register property value.
    const string key = "int16p";
    int16_t value = 127;
    store.RegisterInt16(key, &value);

    // Verify property value.
    int16_t test_value;
    Error error;
    EXPECT_TRUE(store.GetInt16Property(key, &test_value, &error));
    EXPECT_EQ(value, test_value);

    // Set property using chromeos::Any variant type.
    int16_t new_value = 128;
    EXPECT_TRUE(store.SetAnyProperty(key, chromeos::Any(new_value), &error));
    EXPECT_TRUE(store.GetInt16Property(key, &test_value, &error));
    EXPECT_EQ(new_value, test_value);
  }
  {
    // Register property value.
    const string key = "int32p";
    int32_t value = 127;
    store.RegisterInt32(key, &value);

    // Verify property value.
    int32_t test_value;
    Error error;
    EXPECT_TRUE(store.GetInt32Property(key, &test_value, &error));
    EXPECT_EQ(value, test_value);

    // Set property using chromeos::Any variant type.
    int32_t new_value = 128;
    EXPECT_TRUE(store.SetAnyProperty(key, chromeos::Any(new_value), &error));
    EXPECT_TRUE(store.GetInt32Property(key, &test_value, &error));
    EXPECT_EQ(new_value, test_value);
  }
  {
    // Register property value.
    const string key = "stringp";
    string value = "noooo";
    store.RegisterString(key, &value);

    // Verify property value.
    string test_value;
    Error error;
    EXPECT_TRUE(store.GetStringProperty(key, &test_value, &error));
    EXPECT_EQ(value, test_value);

    // Set property using chromeos::Any variant type.
    string new_value = "yesss";
    EXPECT_TRUE(store.SetAnyProperty(key, chromeos::Any(new_value), &error));
    EXPECT_TRUE(store.GetStringProperty(key, &test_value, &error));
    EXPECT_EQ(new_value, test_value);
  }
  {
    // Register property value.
    const string key = "stringmapp";
    Stringmap value;
    value["noooo"] = "yesss";
    store.RegisterStringmap(key, &value);

    // Verify property value.
    Stringmap test_value;
    Error error;
    EXPECT_TRUE(store.GetStringmapProperty(key, &test_value, &error));
    EXPECT_TRUE(value == test_value);

    // Set property using chromeos::Any variant type.
    Stringmap new_value;
    new_value["yesss"] = "noooo";
    EXPECT_TRUE(store.SetAnyProperty(key, chromeos::Any(new_value), &error));
    EXPECT_TRUE(store.GetStringmapProperty(key, &test_value, &error));
    EXPECT_TRUE(new_value == test_value);
  }
  {
    // Register property value.
    const string key = "stringsp";
    Strings value;
    string element;
    element = "noooo";
    value.push_back(element);
    store.RegisterStrings(key, &value);

    // Verify property value.
    Strings test_value;
    Error error;
    EXPECT_TRUE(store.GetStringsProperty(key, &test_value, &error));
    EXPECT_TRUE(value == test_value);

    // Set property using chromeos::Any variant type.
    Strings new_value;
    string new_element;
    new_element = "yesss";
    new_value.push_back(new_element);
    EXPECT_TRUE(store.SetAnyProperty(key, chromeos::Any(new_value), &error));
    EXPECT_TRUE(store.GetStringsProperty(key, &test_value, &error));
    EXPECT_TRUE(new_value == test_value);
  }
  {
    // Register property value.
    const string key = "uint8p";
    uint8_t value = 127;
    store.RegisterUint8(key, &value);

    // Verify property value.
    uint8_t test_value;
    Error error;
    EXPECT_TRUE(store.GetUint8Property(key, &test_value, &error));
    EXPECT_EQ(value, test_value);

    // Set property using chromeos::Any variant type.
    uint8_t new_value = 128;
    EXPECT_TRUE(store.SetAnyProperty(key, chromeos::Any(new_value), &error));
    EXPECT_TRUE(store.GetUint8Property(key, &test_value, &error));
    EXPECT_EQ(new_value, test_value);
  }
  {
    // Register property value.
    const string key = "uint16p";
    uint16_t value = 127;
    store.RegisterUint16(key, &value);

    // Verify property value.
    uint16_t test_value;
    Error error;
    EXPECT_TRUE(store.GetUint16Property(key, &test_value, &error));
    EXPECT_EQ(value, test_value);

    // Set property using chromeos::Any variant type.
    uint16_t new_value = 128;
    EXPECT_TRUE(store.SetAnyProperty(key, chromeos::Any(new_value), &error));
    EXPECT_TRUE(store.GetUint16Property(key, &test_value, &error));
    EXPECT_EQ(new_value, test_value);
  }
  {
    // Register property value.
    const string key = "uint32p";
    uint32_t value = 127;
    store.RegisterUint32(key, &value);

    // Verify property value.
    uint32_t test_value;
    Error error;
    EXPECT_TRUE(store.GetUint32Property(key, &test_value, &error));
    EXPECT_EQ(value, test_value);

    // Set property using chromeos::Any variant type.
    uint32_t new_value = 128;
    EXPECT_TRUE(store.SetAnyProperty(key, chromeos::Any(new_value), &error));
    EXPECT_TRUE(store.GetUint32Property(key, &test_value, &error));
    EXPECT_EQ(new_value, test_value);
  }
  {
    // KeyValueStoreProperty is only defined for derived types so handle
    // this case manually here.
    const string key = "keyvaluestorep";
    EXPECT_CALL(*this, GetKeyValueStoreCallback(_))
        .WillOnce(Return(KeyValueStore()));
    store.RegisterDerivedKeyValueStore(
        key,
        KeyValueStoreAccessor(
            new CustomAccessor<PropertyStoreTest, KeyValueStore>(
                this, &PropertyStoreTest::GetKeyValueStoreCallback,
                &PropertyStoreTest::SetKeyValueStoreCallback)));

    chromeos::VariantDictionary value;
    EXPECT_CALL(*this, SetKeyValueStoreCallback(_, _)).WillOnce(Return(true));
    Error error;
    EXPECT_TRUE(store.SetAnyProperty(key, chromeos::Any(value), &error));
  }
}

TEST_F(PropertyStoreTest, SetAndGetProperties) {
  PropertyStore store;

  // Register properties.
  const string kBoolKey = "boolp";
  const string kKeyValueStoreKey = "keyvaluestorep";
  const string kInt16Key = "int16p";
  const string kInt32Key = "int32p";
  const string kStringKey = "stringp";
  const string kStringsKey = "stringsp";
  const string kStringmapKey = "stringmapp";
  const string kUint8Key = "uint8p";
  const string kUint16Key = "uint16p";
  const string kUint32Key = "uint32p";
  bool bool_value = true;
  int16_t int16_value = 16;
  int32_t int32_value = 32;
  string string_value = "string";
  Stringmap stringmap_value;
  stringmap_value["noooo"] = "yesss";
  Strings strings_value;
  strings_value.push_back("yesss");
  uint8_t uint8_value = 8;
  uint16_t uint16_value = 16;
  uint32_t uint32_value = 32;

  store.RegisterBool(kBoolKey, &bool_value);
  store.RegisterInt16(kInt16Key, &int16_value);
  store.RegisterInt32(kInt32Key, &int32_value);
  store.RegisterString(kStringKey, &string_value);
  store.RegisterStrings(kStringsKey, &strings_value);
  store.RegisterStringmap(kStringmapKey, &stringmap_value);
  store.RegisterUint8(kUint8Key, &uint8_value);
  store.RegisterUint16(kUint16Key, &uint16_value);
  store.RegisterUint32(kUint32Key, &uint32_value);

  // Special handling for KeyValueStore property.
  EXPECT_CALL(*this, GetKeyValueStoreCallback(_))
      .WillOnce(Return(KeyValueStore()));
  store.RegisterDerivedKeyValueStore(
      kKeyValueStoreKey,
      KeyValueStoreAccessor(
          new CustomAccessor<PropertyStoreTest, KeyValueStore>(
              this, &PropertyStoreTest::GetKeyValueStoreCallback,
              &PropertyStoreTest::SetKeyValueStoreCallback)));

  // Update properties.
  bool new_bool_value = false;
  chromeos::VariantDictionary new_key_value_store_value;
  int16_t new_int16_value = 17;
  int32_t new_int32_value = 33;
  string new_string_value = "strings";
  Stringmap new_stringmap_value;
  new_stringmap_value["yesss"] = "noooo";
  Strings new_strings_value;
  new_strings_value.push_back("noooo");
  uint8_t new_uint8_value = 9;
  uint16_t new_uint16_value = 17;
  uint32_t new_uint32_value = 33;

  chromeos::VariantDictionary dict;
  dict.insert(std::make_pair(kBoolKey, chromeos::Any(new_bool_value)));
  dict.insert(std::make_pair(kKeyValueStoreKey,
                             chromeos::Any(new_key_value_store_value)));
  dict.insert(std::make_pair(kInt16Key, chromeos::Any(new_int16_value)));
  dict.insert(std::make_pair(kInt32Key, chromeos::Any(new_int32_value)));
  dict.insert(std::make_pair(kStringKey, chromeos::Any(new_string_value)));
  dict.insert(std::make_pair(kStringmapKey,
                             chromeos::Any(new_stringmap_value)));
  dict.insert(std::make_pair(kStringsKey, chromeos::Any(new_strings_value)));
  dict.insert(std::make_pair(kUint8Key, chromeos::Any(new_uint8_value)));
  dict.insert(std::make_pair(kUint16Key, chromeos::Any(new_uint16_value)));
  dict.insert(std::make_pair(kUint32Key, chromeos::Any(new_uint32_value)));

  EXPECT_CALL(*this, SetKeyValueStoreCallback(_, _)).WillOnce(Return(true));
  Error error;
  EXPECT_TRUE(store.SetProperties(dict, &error));

  // Retrieve properties.
  EXPECT_CALL(*this, GetKeyValueStoreCallback(_))
      .WillOnce(Return(KeyValueStore()));
  chromeos::VariantDictionary result_dict;
  EXPECT_TRUE(store.GetProperties(&result_dict, &error));

  // Verify property values.
  EXPECT_EQ(new_bool_value, result_dict[kBoolKey].Get<bool>());
  EXPECT_EQ(new_int16_value, result_dict[kInt16Key].Get<int16_t>());
  EXPECT_EQ(new_int32_value, result_dict[kInt32Key].Get<int32_t>());
  EXPECT_EQ(new_string_value, result_dict[kStringKey].Get<string>());
  EXPECT_TRUE(
      new_stringmap_value == result_dict[kStringmapKey].Get<Stringmap>());
  EXPECT_TRUE(new_strings_value == result_dict[kStringsKey].Get<Strings>());
  EXPECT_EQ(new_uint8_value, result_dict[kUint8Key].Get<uint8_t>());
  EXPECT_EQ(new_uint16_value, result_dict[kUint16Key].Get<uint16_t>());
  EXPECT_EQ(new_uint32_value, result_dict[kUint32Key].Get<uint32_t>());
}

TEST_F(PropertyStoreTest, VariantDictionaryToKeyValueStore) {
  chromeos::VariantDictionary dict;
  KeyValueStore store;
  Error error;

  const bool kBool = true;
  const char kBoolKey[] = "bool_arg";
  const int32_t kInt32 = 123;
  const char kInt32Key[] = "int32_arg";
  const string kString = "string";
  const char kStringKey[] = "string_arg";
  const map<string, string> kStringmap{ { "key0", "value0" } };
  const char kStringmapKey[] = "stringmap_key";
  const vector<string> kStrings{ "string0", "string1" };
  const char kStringsKey[] = "strings_key";
  chromeos::VariantDictionary variant_dict;
  const char kVariantDictSubKey[] = "dict_sub_key";
  variant_dict[kVariantDictSubKey] = chromeos::Any(true);
  const char kVariantDictKey[] = "dict_key";

  dict.insert(std::make_pair(kBoolKey, chromeos::Any(kBool)));
  dict.insert(std::make_pair(kInt32Key, chromeos::Any(kInt32)));
  dict.insert(std::make_pair(kStringKey, chromeos::Any(kString)));
  dict.insert(std::make_pair(kStringmapKey, chromeos::Any(kStringmap)));
  dict.insert(std::make_pair(kStringsKey, chromeos::Any(kStrings)));
  dict.insert(std::make_pair(kVariantDictKey, chromeos::Any(variant_dict)));

  PropertyStore::VariantDictionaryToKeyValueStore(dict, &store, &error);
  EXPECT_TRUE(error.IsSuccess());
  EXPECT_EQ(kBool, store.GetBool(kBoolKey));
  EXPECT_EQ(kInt32, store.GetInt(kInt32Key));
  EXPECT_EQ(kString, store.GetString(kStringKey));
  EXPECT_EQ(kStringmap, store.GetStringmap(kStringmapKey));
  EXPECT_EQ(kStrings, store.GetStrings(kStringsKey));
  KeyValueStore property_map;
  property_map.SetBool(kVariantDictSubKey, true);
  EXPECT_EQ(property_map, store.GetKeyValueStore(kVariantDictKey));
}

TEST_F(PropertyStoreTest, KeyValueStoreToVariantDictionary) {
  chromeos::VariantDictionary dict;
  KeyValueStore store;

  const bool kBool = true;
  const char kBoolKey[] = "bool_arg";
  const int32_t kInt32 = 123;
  const char kInt32Key[] = "int32_arg";
  const string kString = "string";
  const char kStringKey[] = "string_arg";
  const map<string, string> kStringmap{ { "key0", "value0" } };
  const char kStringmapKey[] = "stringmap_key";
  const vector<string> kStrings{ "string0", "string1" };
  const char kStringsKey[] = "strings_key";
  const char kVariantDictKey[] = "dict_key";
  const char kVariantDictSubKey[] = "dict_sub_key";
  KeyValueStore variant_store;
  variant_store.SetBool(kVariantDictSubKey, true);

  store.SetBool(kBoolKey, kBool);
  store.SetInt(kInt32Key, kInt32);
  store.SetString(kStringKey, kString);
  store.SetStringmap(kStringmapKey, kStringmap);
  store.SetStrings(kStringsKey, kStrings);
  store.SetKeyValueStore(kVariantDictKey, variant_store);

  PropertyStore::KeyValueStoreToVariantDictionary(store, &dict);
  EXPECT_EQ(kBool, dict[kBoolKey].Get<bool>());
  EXPECT_EQ(kInt32, dict[kInt32Key].Get<int32_t>());
  EXPECT_EQ(kString, dict[kStringKey].Get<string>());
  EXPECT_EQ(kStringmap, dict[kStringmapKey].Get<Stringmap>());
  EXPECT_EQ(kStrings, dict[kStringsKey].Get<Strings>());
  chromeos::VariantDictionary variant_dict;
  variant_dict = dict[kVariantDictKey].Get<chromeos::VariantDictionary>();
  EXPECT_TRUE(variant_dict[kVariantDictSubKey].Get<bool>());
}

}  // namespace shill
