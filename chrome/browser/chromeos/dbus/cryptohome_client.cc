// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/cryptohome_client.h"

#include "base/bind.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/chromeos/system/runtime_environment.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

// A convenient macro to initialize a dbus::MethodCall while checking the dbus
// method name matches to the C++ method name.
#define INITIALIZE_METHOD_CALL(method_call_name, method_name)         \
  DCHECK_EQ(std::string(method_name), __FUNCTION__);                  \
  dbus::MethodCall method_call_name(cryptohome::kCryptohomeInterface, \
                                    method_name);

// Calls PopBool with |reader|.  This method is used to create callbacks.
bool PopBool(bool* output, dbus::MessageReader* reader) {
  return reader->PopBool(output);
}

// Calls PopString with |reader|.  This method is used to create callbacks.
bool PopString(std::string* output, dbus::MessageReader* reader) {
  return reader->PopString(output);
}

// Get array of bytes as vector.
// This method is used to create callbacks.
bool PopArrayOfBytesAsVector(std::vector<uint8>* output,
                             dbus::MessageReader* reader) {
  uint8* bytes = NULL;
  size_t length = 0;
  if (!reader->PopArrayOfBytes(&bytes, &length))
    return false;
  output->assign(bytes, bytes + length);
  return true;
}

// Does nothing.
// This method is used to create callbacks to call functions without result.
bool PopNothing(dbus::MessageReader*) {
  return true;
}

// Runs |popper1| and |popper2|.  This method is used to create callbacks to
// call functions with multiple results.
bool PopTwoValues(base::Callback<bool(dbus::MessageReader* reader)> popper1,
                  base::Callback<bool(dbus::MessageReader* reader)> popper2,
                  dbus::MessageReader* reader) {
  return popper1.Run(reader) && popper2.Run(reader);
}

// The CryptohomeClient implementation.
class CryptohomeClientImpl : public CryptohomeClient {
 public:
  explicit CryptohomeClientImpl(dbus::Bus* bus)
      : bus_(bus),
        proxy_(bus->GetObjectProxy(
            cryptohome::kCryptohomeServiceName,
            dbus::ObjectPath(cryptohome::kCryptohomeServicePath))),
        weak_ptr_factory_(this),
        on_blocking_method_call_(false /* manual_reset */,
                                 false /* initially_signaled */) {
    proxy_->ConnectToSignal(
        cryptohome::kCryptohomeInterface,
        cryptohome::kSignalAsyncCallStatus,
        base::Bind(&CryptohomeClientImpl::OnAsyncCallStatus,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&CryptohomeClientImpl::OnSignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  // CryptohomeClient override.
  virtual void SetAsyncCallStatusHandler(AsyncCallStatusHandler handler)
      OVERRIDE {
    async_call_status_handler_ = handler;
  }

  // CryptohomeClient override.
  virtual void ResetAsyncCallStatusHandler() OVERRIDE {
    async_call_status_handler_.Reset();
  }

  // CryptohomeClient override.
  virtual bool IsMounted(bool* is_mounted) OVERRIDE {
    INITIALIZE_METHOD_CALL(method_call, cryptohome::kCryptohomeIsMounted);
    return CallMethodAndBlock(&method_call, base::Bind(&PopBool, is_mounted));
  }

  // CryptohomeClient override.
  virtual void AsyncCheckKey(const std::string& username,
                             const std::string& key,
                             base::Callback<void(int async_id)> callback)
      OVERRIDE {
    INITIALIZE_METHOD_CALL(method_call, cryptohome::kCryptohomeAsyncCheckKey);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(username);
    writer.AppendString(key);
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::Bind(&CryptohomeClientImpl::OnAsyncMethodCall,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  callback));
  }

  // CryptohomeClient override.
  virtual void AsyncMigrateKey(const std::string& username,
                               const std::string& from_key,
                               const std::string& to_key,
                               base::Callback<void(int async_id)> callback)
      OVERRIDE {
    INITIALIZE_METHOD_CALL(method_call, cryptohome::kCryptohomeAsyncMigrateKey);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(username);
    writer.AppendString(from_key);
    writer.AppendString(to_key);
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::Bind(&CryptohomeClientImpl::OnAsyncMethodCall,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  callback));
  }

  // CryptohomeClient override.
  virtual void AsyncRemove(const std::string& username,
                           base::Callback<void(int async_id)> callback)
      OVERRIDE {
    INITIALIZE_METHOD_CALL(method_call, cryptohome::kCryptohomeAsyncRemove);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(username);
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::Bind(&CryptohomeClientImpl::OnAsyncMethodCall,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  callback));
  }

  // CryptohomeClient override.
  virtual bool GetSystemSalt(std::vector<uint8>* salt) OVERRIDE {
    INITIALIZE_METHOD_CALL(method_call, cryptohome::kCryptohomeGetSystemSalt);
    return CallMethodAndBlock(&method_call,
                              base::Bind(&PopArrayOfBytesAsVector, salt));
  }

  // CryptohomeClient override.
  virtual void AsyncMount(const std::string& username,
                          const std::string& key,
                          const bool create_if_missing,
                          base::Callback<void(int async_id)> callback)
      OVERRIDE {
    INITIALIZE_METHOD_CALL(method_call, cryptohome::kCryptohomeAsyncMount);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(username);
    writer.AppendString(key);
    writer.AppendBool(create_if_missing);
    writer.AppendBool(false);  // deprecated_replace_tracked_subdirectories
    // deprecated_tracked_subdirectories
    writer.AppendArrayOfStrings(std::vector<std::string>());
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::Bind(&CryptohomeClientImpl::OnAsyncMethodCall,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  callback));
  }

  // CryptohomeClient override.
  virtual void AsyncMountGuest(
      base::Callback<void(int async_id)> callback) OVERRIDE {
    INITIALIZE_METHOD_CALL(method_call, cryptohome::kCryptohomeAsyncMountGuest);
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::Bind(&CryptohomeClientImpl::OnAsyncMethodCall,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  callback));
  }

  // CryptohomeClient override.
  virtual bool TpmIsReady(bool* ready) OVERRIDE {
    INITIALIZE_METHOD_CALL(method_call, cryptohome::kCryptohomeTpmIsReady);
    return CallMethodAndBlock(&method_call, base::Bind(&PopBool, ready));
  }

  // CryptohomeClient override.
  virtual bool TpmIsEnabled(bool* enabled) OVERRIDE {
    INITIALIZE_METHOD_CALL(method_call, cryptohome::kCryptohomeTpmIsEnabled);
    return CallMethodAndBlock(&method_call, base::Bind(&PopBool, enabled));
  }

  // CryptohomeClient override.
  virtual bool TpmGetPassword(std::string* password) OVERRIDE {
    INITIALIZE_METHOD_CALL(method_call, cryptohome::kCryptohomeTpmGetPassword);
    return CallMethodAndBlock(&method_call, base::Bind(&PopString, password));
  }

  // CryptohomeClient override.
  virtual bool TpmIsOwned(bool* owned) OVERRIDE {
    INITIALIZE_METHOD_CALL(method_call, cryptohome::kCryptohomeTpmIsOwned);
    return CallMethodAndBlock(&method_call, base::Bind(&PopBool, owned));
  }

  // CryptohomeClient override.
  virtual bool TpmIsBeingOwned(bool* owning) OVERRIDE {
    INITIALIZE_METHOD_CALL(method_call, cryptohome::kCryptohomeTpmIsBeingOwned);
    return CallMethodAndBlock(&method_call, base::Bind(&PopBool, owning));
  }

  // CryptohomeClient override.
  virtual bool TpmCanAttemptOwnership() OVERRIDE {
    INITIALIZE_METHOD_CALL(method_call,
                           cryptohome::kCryptohomeTpmCanAttemptOwnership);
    return CallMethodAndBlock(&method_call, base::Bind(&PopNothing));
  }

  // CryptohomeClient override.
  virtual bool TpmClearStoredPassword() OVERRIDE {
    INITIALIZE_METHOD_CALL(method_call,
                           cryptohome::kCryptohomeTpmClearStoredPassword);
    return CallMethodAndBlock(&method_call, base::Bind(&PopNothing));
  }

  // CryptohomeClient override.
  virtual bool Pkcs11IsTpmTokenReady(bool* ready) OVERRIDE {
    INITIALIZE_METHOD_CALL(method_call,
                           cryptohome::kCryptohomePkcs11IsTpmTokenReady);
    return CallMethodAndBlock(&method_call, base::Bind(&PopBool, ready));
  }

  // CryptohomeClient override.
  virtual bool Pkcs11GetTpmTokenInfo(std::string* label,
                                     std::string* user_pin) OVERRIDE {
    INITIALIZE_METHOD_CALL(method_call,
                           cryptohome::kCryptohomePkcs11GetTpmTokenInfo);
    if (!CallMethodAndBlock(&method_call,
                            base::Bind(&PopTwoValues,
                                       base::Bind(&PopString, label),
                                       base::Bind(&PopString, user_pin)))) {
      label->clear();
      user_pin->clear();
      return false;
    }
    return true;
  }

  // CryptohomeClient override.
  virtual bool InstallAttributesGet(const std::string& name,
                                    std::vector<uint8>* value,
                                    bool* successful) OVERRIDE {
    INITIALIZE_METHOD_CALL(method_call,
                           cryptohome::kCryptohomeInstallAttributesGet);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(name);
    if (!CallMethodAndBlock(
            &method_call,
            base::Bind(&PopTwoValues,
                       base::Bind(&PopArrayOfBytesAsVector, value),
                       base::Bind(&PopBool, successful)))) {
      value->clear();
      *successful = false;
      return false;
    }
    return true;
  }

  // CryptohomeClient override.
  virtual bool InstallAttributesSet(const std::string& name,
                                    const std::vector<uint8>& value,
                                    bool* successful) OVERRIDE {
    INITIALIZE_METHOD_CALL(method_call,
                           cryptohome::kCryptohomeInstallAttributesSet);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(name);
    writer.AppendArrayOfBytes(value.data(), value.size());
    return CallMethodAndBlock(&method_call, base::Bind(&PopBool, successful));
  }

  // CryptohomeClient override.
  virtual bool InstallAttributesFinalize(bool* successful) OVERRIDE {
    INITIALIZE_METHOD_CALL(method_call,
                           cryptohome::kCryptohomeInstallAttributesFinalize);
    return CallMethodAndBlock(&method_call, base::Bind(&PopBool, successful));
  }

  // CryptohomeClient override.
  virtual bool InstallAttributesIsReady(bool* is_ready) OVERRIDE {
    INITIALIZE_METHOD_CALL(method_call,
                           cryptohome::kCryptohomeInstallAttributesIsReady);
    return CallMethodAndBlock(&method_call, base::Bind(&PopBool, is_ready));
  }

  // CryptohomeClient override.
  virtual bool InstallAttributesIsInvalid(bool* is_invalid) OVERRIDE {
    INITIALIZE_METHOD_CALL(method_call,
                           cryptohome::kCryptohomeInstallAttributesIsInvalid);
    return CallMethodAndBlock(&method_call, base::Bind(&PopBool, is_invalid));
  }

  // CryptohomeClient override.
  virtual bool InstallAttributesIsFirstInstall(bool* is_first_install) OVERRIDE
  {
    INITIALIZE_METHOD_CALL(
        method_call, cryptohome::kCryptohomeInstallAttributesIsFirstInstall);
    return CallMethodAndBlock(&method_call,
                              base::Bind(&PopBool, is_first_install));
  }

 private:
  // A utility class to ensure the WaitableEvent is signaled.
  class WaitableEventSignaler {
   public:
    explicit WaitableEventSignaler(base::WaitableEvent* event) : event_(event) {
    }

    ~WaitableEventSignaler() {
      event_->Signal();
    }

   private:
    base::WaitableEvent* event_;
  };

  // Handles the result of AsyncXXX methods.
  void OnAsyncMethodCall(base::Callback<void(int async_id)> callback,
                         dbus::Response* response) {
    if (!response)
      return;
    dbus::MessageReader reader(response);
    int async_id = 0;
    if (!reader.PopInt32(&async_id)) {
      LOG(ERROR) << "Invalid response: " << response->ToString();
      return;
    }
    callback.Run(async_id);
  }

  // Calls the method and block until it returns.
  // |callback| is called with the result on the DBus thread.
  bool CallMethodAndBlock(dbus::MethodCall* method_call,
                          base::Callback<bool(dbus::MessageReader*)> callback) {
    WaitableEventSignaler* signaler =
        new WaitableEventSignaler(&on_blocking_method_call_);
    bool success = false;
    bus_->PostTaskToDBusThread(
        FROM_HERE,
        base::Bind(&CryptohomeClientImpl::CallMethodAndBlockInternal,
                   base::Unretained(this),
                   base::Owned(signaler),
                   &success,
                   method_call,
                   callback));
    on_blocking_method_call_.Wait();
    return success;
  }

  // This method is a part of CallMethodAndBlock implementation.
  void CallMethodAndBlockInternal(
      WaitableEventSignaler* signaler,
      bool* success,
      dbus::MethodCall* method_call,
      base::Callback<bool(dbus::MessageReader*)> callback) {
    dbus::Response* response = proxy_->CallMethodAndBlock(
        method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT);
    if (response) {
      dbus::MessageReader reader(response);
      *success = callback.Run(&reader);
    }
  }

  // Handles AsyncCallStatus signal.
  void OnAsyncCallStatus(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    int async_id = 0;
    bool return_status = false;
    int return_code = 0;
    if (!reader.PopInt32(&async_id) ||
        !reader.PopBool(&return_status) ||
        !reader.PopInt32(&return_code)) {
      LOG(ERROR) << "Invalid signal: " << signal->ToString();
      return;
    }
    if (!async_call_status_handler_.is_null())
      async_call_status_handler_.Run(async_id, return_status, return_code);
  }

  // Handles the result of signal connection setup.
  void OnSignalConnected(const std::string& interface,
                         const std::string& signal,
                         bool successed) {
    LOG_IF(ERROR, !successed) << "Connect to " << interface << " " <<
        signal << " failed.";
  }

  dbus::Bus* bus_;
  dbus::ObjectProxy* proxy_;
  base::WeakPtrFactory<CryptohomeClientImpl> weak_ptr_factory_;
  base::WaitableEvent on_blocking_method_call_;
  AsyncCallStatusHandler async_call_status_handler_;

  DISALLOW_COPY_AND_ASSIGN(CryptohomeClientImpl);
};

// A stub implementaion of CryptohomeClient.
class CryptohomeClientStubImpl : public CryptohomeClient {
 public:
  CryptohomeClientStubImpl() {}
  virtual ~CryptohomeClientStubImpl() {}

  // CryptohomeClient override.
  virtual void SetAsyncCallStatusHandler(AsyncCallStatusHandler handler)
      OVERRIDE {
  }

  // CryptohomeClient override.
  virtual void ResetAsyncCallStatusHandler() OVERRIDE {
  }

  // CryptohomeClient override.
  virtual bool IsMounted(bool* is_mounted) OVERRIDE {
    *is_mounted = true;
    return true;
  }

  // CryptohomeClient override.
  virtual void AsyncCheckKey(const std::string& username,
                             const std::string& key,
                             base::Callback<void(int async_id)> callback)
      OVERRIDE {
  }

  // CryptohomeClient override.
  virtual void AsyncMigrateKey(const std::string& username,
                               const std::string& from_key,
                               const std::string& to_key,
                               base::Callback<void(int async_id)> callback)
      OVERRIDE {
  }

  // CryptohomeClient override.
  virtual void AsyncRemove(const std::string& username,
                           base::Callback<void(int async_id)> callback)
      OVERRIDE {
  }

  // CryptohomeClient override.
  virtual bool GetSystemSalt(std::vector<uint8>* salt) OVERRIDE {
    const char kStubSystemSalt[] = "stub_system_salt";
    salt->assign(kStubSystemSalt,
                 kStubSystemSalt + arraysize(kStubSystemSalt));
    return true;
  }

  // CryptohomeClient override.
  virtual void AsyncMount(const std::string& username,
                          const std::string& key,
                          const bool create_if_missing,
                          base::Callback<void(int async_id)> callback)
      OVERRIDE {
  }

  // CryptohomeClient override.
  virtual void AsyncMountGuest(
      base::Callback<void(int async_id)> callback) OVERRIDE {
  }

  // CryptohomeClient override.
  virtual bool TpmIsReady(bool* ready) OVERRIDE {
    *ready = true;
    return true;
  }

  // CryptohomeClient override.
  virtual bool TpmIsEnabled(bool* enabled) OVERRIDE {
    *enabled = true;
    return true;
  }

  // CryptohomeClient override.
  virtual bool TpmGetPassword(std::string* password) OVERRIDE {
    const char kStubTpmPassword[] = "Stub-TPM-password";
    *password = kStubTpmPassword;
    return true;
  }

  // CryptohomeClient override.
  virtual bool TpmIsOwned(bool* owned) OVERRIDE {
    *owned = true;
    return true;
  }

  // CryptohomeClient override.
  virtual bool TpmIsBeingOwned(bool* owning) OVERRIDE {
    *owning = true;
    return true;
  }

  // CryptohomeClient override.
  virtual bool TpmCanAttemptOwnership() OVERRIDE { return true; }

  // CryptohomeClient override.
  virtual bool TpmClearStoredPassword() OVERRIDE { return true; }

  // CryptohomeClient override.
  virtual bool Pkcs11IsTpmTokenReady(bool* ready) OVERRIDE {
    *ready = true;
    return true;
  }

  // CryptohomeClient override.
  virtual bool Pkcs11GetTpmTokenInfo(std::string* label,
                                     std::string* user_pin) OVERRIDE {
    const char kStubLabel[] = "Stub TPM Token";
    const char kStubUserPin[] = "012345";
    *label = kStubLabel;
    *user_pin = kStubUserPin;
    return true;
  }

  // CryptohomeClient override.
  virtual bool InstallAttributesGet(const std::string& name,
                                    std::vector<uint8>* value,
                                    bool* successful) OVERRIDE {
    *successful = true;
    return true;
  }

  // CryptohomeClient override.
  virtual bool InstallAttributesSet(const std::string& name,
                                    const std::vector<uint8>& value,
                                    bool* successful) OVERRIDE {
    *successful = true;
    return true;
  }

  // CryptohomeClient override.
  virtual bool InstallAttributesFinalize(bool* successful) OVERRIDE {
    *successful = true;
    return true;
  }

  // CryptohomeClient override.
  virtual bool InstallAttributesIsReady(bool* is_ready) OVERRIDE {
    *is_ready = true;
    return true;
  }

  // CryptohomeClient override.
  virtual bool InstallAttributesIsInvalid(bool* is_invalid) OVERRIDE {
    *is_invalid = false;
    return true;
  }

  // CryptohomeClient override.
  virtual bool InstallAttributesIsFirstInstall(bool* is_first_install) OVERRIDE
  {
    *is_first_install = true;
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CryptohomeClientStubImpl);
};

} // namespace

////////////////////////////////////////////////////////////////////////////////
// CryptohomeClient

CryptohomeClient::CryptohomeClient() {}

CryptohomeClient::~CryptohomeClient() {}

// static
CryptohomeClient* CryptohomeClient::Create(dbus::Bus* bus) {
  if (system::runtime_environment::IsRunningOnChromeOS())
    return new CryptohomeClientImpl(bus);
  else
    return new CryptohomeClientStubImpl();
}

}  // namespace chromeos
