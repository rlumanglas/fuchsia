// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_MEDIA_AUDIO_AUDIO_CORE_ROUTE_GRAPH_H_
#define SRC_MEDIA_AUDIO_AUDIO_CORE_ROUTE_GRAPH_H_

#include <fuchsia/media/cpp/fidl.h>
#include <lib/fit/bridge.h>

#include <array>
#include <deque>
#include <unordered_map>

#include <fbl/ref_ptr.h>

#include "src/media/audio/audio_core/audio_input.h"
#include "src/media/audio/audio_core/audio_output.h"
#include "src/media/audio/audio_core/device_config.h"
#include "src/media/audio/audio_core/link_matrix.h"
#include "src/media/audio/audio_core/threading_model.h"

namespace media::audio {

// |RoutingProfile| informs the |RouteGraph| on how or whether it should establish links for a
// particular input or output to the mixer.
struct RoutingProfile {
  bool routable = false;
  fuchsia::media::Usage usage = {};
};

// |RouteGraph| is responsible for managing connections between inputs and outputs of the mixer.
//
// |RouteGraph| owns user-level inputs and outputs (|AudioRenderer|s and |AudioCapturer|s).
//
// Renderers are routed by Usage to the most recently plugged output that supports their Usage.
//
// Capturers are routed to the most recently plugged input.
//
// Loopback capturers are routed to the most recently plugged output which supports loopback.
class RouteGraph {
 public:
  // Constructs a graph from the given config and link matrix. Each parameter must outlive the
  // RouteGraph.
  RouteGraph(const DeviceConfig& device_config, LinkMatrix* link_matrix);

  ~RouteGraph();

  // TODO(13339): Remove throttle_output_.
  // Sets a throttle output which is linked to all AudioRenderers to throttle the rate at which we
  // return packets to clients.
  void SetThrottleOutput(ThreadingModel* threading_model,
                         std::shared_ptr<AudioOutput> throttle_output);

  // Adds an |AudioOutput| to the route graph. An |AudioOutput| is allowed to receive
  // postmix samples from |AudioRenderer|s.
  void AddOutput(AudioDevice* output);

  // Removes an |AudioOutput| from the route graph. Any connected |AudioRenderer|s and loopback
  // |AudioCapturer|s will be rerouted.
  void RemoveOutput(AudioDevice* output);

  // Adds an |AudioInput| to the route graph. An audio input may be connected to transmit samples
  // to an |AudioCapturer|.
  void AddInput(AudioDevice* input);

  // Removes an |AudioInput| from the route graph. Any connected |AudioCapturer|s will be rerouted.
  void RemoveInput(AudioDevice* input);

  // Adds an |AudioRenderer| to the route graph. An |AudioRenderer| may be connected to
  // |AudioOutput|s.
  void AddRenderer(std::unique_ptr<AudioObject> renderer);

  // Sets the routing profile with which the route graph selects |AudioOutput|s for the
  // |AudioRenderer|.
  void SetRendererRoutingProfile(const AudioObject& renderer, RoutingProfile profile);

  void RemoveRenderer(const AudioObject& renderer);

  // Adds an |AudioCapturer| to the route graph. An |AudioCapturer| may be connected to
  // |AudioInput|s to receive samples from them.
  void AddCapturer(std::unique_ptr<AudioObject> capturer);

  // Sets the routing profile with which the route graph selects |AudioInput|s for the
  // |AudioCapturer|.
  void SetCapturerRoutingProfile(const AudioObject& capturer, RoutingProfile profile);

  void RemoveCapturer(const AudioObject& capturer);

  // Adds an |AudioCapturer| to the route graph which will receive the output mixed for the most
  // recently added output device.
  void AddLoopbackCapturer(std::unique_ptr<AudioObject> loopback_capturer);

  // Sets the routing profile with which the route graph selects |AudioOutput|s for the
  // loopback |AudioCapturer|.
  void SetLoopbackCapturerRoutingProfile(const AudioObject& loopback_capturer,
                                         RoutingProfile profile);

  void RemoveLoopbackCapturer(const AudioObject& loopback_capturer);

 private:
  struct RoutableOwnedObject {
    std::shared_ptr<AudioObject> ref;
    RoutingProfile profile;
  };

  struct Target {
    Target() {}

    Target(AudioDevice* _device, std::shared_ptr<LoudnessTransform> _transform)
        : device(_device), transform(_transform) {
      FX_DCHECK(!!device == !!transform);
    }

    bool is_linkable() const { return device; }

    AudioDevice* device = nullptr;
    std::shared_ptr<LoudnessTransform> transform = nullptr;
  };

  // Cached targets for linking renderers and capturers.
  struct Targets {
    std::array<Target, fuchsia::media::RENDER_USAGE_COUNT> render = {};
    Target loopback;
    Target capture;
  };

  // A command to unlink components of the graph.
  struct UnlinkCommand {
    // Iff renderers[usage] is true, renderers of that usage should be unlinked.
    std::array<bool, fuchsia::media::RENDER_USAGE_COUNT> renderers = {};
    // Iff true, loopback capturers should be unlinked.
    bool loopback_capturers = false;
    /// Iff true, capturers, should be unlinked.
    bool capturers = false;
  };

  const DeviceConfig::OutputDeviceProfile& OutputDeviceProfile(AudioDevice* device) const;

  void UpdateGraphForDeviceChange();

  // Calculate the new targets based on our routing policy and available devices.
  // Returns the new targets and an unlink command to unlink any out of date
  // routing links.
  std::pair<Targets, UnlinkCommand> CalculateTargets() const;

  void Unlink(UnlinkCommand unlink_command);

  Target OutputForUsage(const fuchsia::media::Usage& usage) const;

  LinkMatrix& link_matrix_;

  const DeviceConfig& device_config_;

  Targets targets_;

  // TODO(39624): convert to weak_ptr when ownership is explicit.
  std::deque<AudioDevice*> inputs_;
  std::deque<AudioDevice*> outputs_;

  std::unordered_map<const AudioObject*, RoutableOwnedObject> capturers_;
  std::unordered_map<const AudioObject*, RoutableOwnedObject> renderers_;
  std::unordered_map<const AudioObject*, RoutableOwnedObject> loopback_capturers_;

  // TODO(13339): Remove throttle_output_.
  std::optional<fit::completer<void, void>> throttle_release_fence_;
  std::shared_ptr<AudioOutput> throttle_output_;
};

}  // namespace media::audio

#endif  // SRC_MEDIA_AUDIO_AUDIO_CORE_ROUTE_GRAPH_H_
