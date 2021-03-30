// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/developer/memory/metrics/bucket_match.h"

#include <lib/syslog/cpp/macros.h>
#include <lib/trace/event.h>

#include <filesystem>
#include <optional>
#include <regex>

#include "third_party/rapidjson/include/rapidjson/document.h"
#include "third_party/rapidjson/include/rapidjson/ostreamwrapper.h"
#include "third_party/rapidjson/include/rapidjson/rapidjson.h"
#include "third_party/rapidjson/include/rapidjson/writer.h"

namespace memory {

BucketMatch::BucketMatch(const std::string& name, const std::string& process,
                         const std::string& vmo, std::optional<int64_t> event_code)
    : name_(name), process_(process), vmo_(vmo), event_code_(event_code) {}

bool BucketMatch::ProcessMatch(const std::string& process) {
  const auto& pi = process_match_.find(process);
  if (pi != process_match_.end()) {
    return pi->second;
  }
  bool match = std::regex_match(process, process_);
  process_match_.emplace(process, match);
  return match;
}

bool BucketMatch::VmoMatch(const Vmo& vmo) {
  const auto& vi = vmo_match_.find(vmo.name);
  if (vi != vmo_match_.end()) {
    return vi->second;
  }
  bool match = std::regex_match(vmo.name, vmo_);
  vmo_match_.emplace(vmo.name, match);
  return match;
}

std::optional<std::vector<BucketMatch>> BucketMatch::ReadBucketMatchesFromConfig(
    const std::string& config_string) {
  std::vector<BucketMatch> result;
  rapidjson::Document doc;
  doc.Parse(config_string);
  if (!doc.IsArray()) {
    FX_LOGS(WARNING) << "Configuration is not valid JSON";
    return std::nullopt;
  }

  result.reserve(doc.GetArray().Size());

  for (const auto& v : doc.GetArray()) {
    if (!(v.HasMember("name") && v["name"].IsString() && v.HasMember("process") &&
          v["process"].IsString() && v.HasMember("vmo") && v["vmo"].IsString())) {
      FX_LOGS(WARNING) << "Missing member";
      return std::nullopt;
    }
    std::string name(v["name"].GetString(), v["name"].GetStringLength());
    std::string process(v["process"].GetString(), v["process"].GetStringLength());
    std::string vmo(v["vmo"].GetString(), v["vmo"].GetStringLength());
    std::optional<int64_t> event_code;
    if (v.HasMember("event_code") && v["event_code"].IsInt64()) {
      event_code = v["event_code"].GetInt64();
    }
    result.emplace_back(name, process, vmo, event_code);
  }
  return result;
}

}  // namespace memory