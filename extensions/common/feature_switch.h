// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_FEATURE_SWITCH_H_
#define EXTENSIONS_COMMON_FEATURE_SWITCH_H_

#include <string>

#include "base/basictypes.h"

class CommandLine;

namespace extensions {

// A switch that can turn a feature on or off. Typically controlled via
// command-line switches but can be overridden, e.g., for testing.
class FeatureSwitch {
 public:
  static FeatureSwitch* easy_off_store_install();
  static FeatureSwitch* force_dev_mode_highlighting();
  static FeatureSwitch* global_commands();
  static FeatureSwitch* prompt_for_external_extensions();
  static FeatureSwitch* error_console();
  static FeatureSwitch* enable_override_bookmarks_ui();

  enum DefaultValue {
    DEFAULT_ENABLED,
    DEFAULT_DISABLED
  };

  enum OverrideValue {
    OVERRIDE_NONE,
    OVERRIDE_ENABLED,
    OVERRIDE_DISABLED
  };

  // A temporary override for the switch value.
  class ScopedOverride {
   public:
    ScopedOverride(FeatureSwitch* feature, bool override_value);
    ~ScopedOverride();
   private:
    FeatureSwitch* feature_;
    FeatureSwitch::OverrideValue previous_value_;
    DISALLOW_COPY_AND_ASSIGN(ScopedOverride);
  };

  FeatureSwitch(const char* switch_name,
                DefaultValue default_value);
  FeatureSwitch(const CommandLine* command_line,
                const char* switch_name,
                DefaultValue default_value);

  // Consider using ScopedOverride instead.
  void SetOverrideValue(OverrideValue value);
  OverrideValue GetOverrideValue() const;

  bool IsEnabled() const;

  std::string GetLegacyEnableFlag() const;
  std::string GetLegacyDisableFlag() const;

 private:
  void Init(const CommandLine* command_line,
            const char* switch_name,
            DefaultValue default_value);

  const CommandLine* command_line_;
  const char* switch_name_;
  bool default_value_;
  OverrideValue override_value_;

  DISALLOW_COPY_AND_ASSIGN(FeatureSwitch);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FEATURE_SWITCH_H_
