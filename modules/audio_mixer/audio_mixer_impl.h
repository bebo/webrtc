/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_MIXER_AUDIO_MIXER_IMPL_H_
#define WEBRTC_MODULES_AUDIO_MIXER_AUDIO_MIXER_IMPL_H_

#include <map>
#include <memory>
#include <vector>

#include "webrtc/base/thread_checker.h"
#include "webrtc/engine_configurations.h"
#include "webrtc/modules/audio_mixer/audio_mixer.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/voice_engine/level_indicator.h"

namespace webrtc {
class AudioProcessing;
class CriticalSectionWrapper;

typedef std::vector<AudioFrame*> AudioFrameList;
typedef std::vector<MixerAudioSource*> MixerAudioSourceList;

// Cheshire cat implementation of MixerAudioSource's non virtual functions.
class NewMixHistory {
 public:
  NewMixHistory();
  ~NewMixHistory();

  // Returns true if the audio source is being mixed.
  bool IsMixed() const;

  // Returns true if the audio source was mixed previous mix
  // iteration.
  bool WasMixed() const;

  // Updates the mixed status.
  int32_t SetIsMixed(bool mixed);

  void ResetMixedStatus();

 private:
  bool is_mixed_;
};

class AudioMixerImpl : public AudioMixer {
 public:
  // AudioProcessing only accepts 10 ms frames.
  static const int kFrameDurationInMs = 10;

  static std::unique_ptr<AudioMixer> Create(int id);

  ~AudioMixerImpl() override;

  // AudioMixer functions
  int32_t SetMixabilityStatus(MixerAudioSource* audio_source,
                              bool mixable) override;
  bool MixabilityStatus(const MixerAudioSource& audio_source) const override;
  int32_t SetAnonymousMixabilityStatus(MixerAudioSource* audio_source,
                                       bool mixable) override;
  void Mix(int sample_rate,
           size_t number_of_channels,
           AudioFrame* audio_frame_for_mixing) override;
  bool AnonymousMixabilityStatus(
      const MixerAudioSource& audio_source) const override;

 private:
  AudioMixerImpl(int id, std::unique_ptr<AudioProcessing> limiter);

  // Set/get mix frequency
  int32_t SetOutputFrequency(const Frequency& frequency);
  Frequency OutputFrequency() const;

  // Compute what audio sources to mix from audio_source_list_. Ramp
  // in and out. Update mixed status. Mixes up to
  // kMaximumAmountOfMixedAudioSources audio sources.
  AudioFrameList GetNonAnonymousAudio() const EXCLUSIVE_LOCKS_REQUIRED(crit_);

  // Return the lowest mixing frequency that can be used without having to
  // downsample any audio.
  int32_t GetLowestMixingFrequency() const;
  int32_t GetLowestMixingFrequencyFromList(
      const MixerAudioSourceList& mixList) const;

  // Return the AudioFrames that should be mixed anonymously. Ramp in
  // and out. Update mixed status.
  AudioFrameList GetAnonymousAudio() const EXCLUSIVE_LOCKS_REQUIRED(crit_);

  // This function returns true if it finds the MixerAudioSource in the
  // specified list of MixerAudioSources.
  bool IsAudioSourceInList(const MixerAudioSource& audio_source,
                           const MixerAudioSourceList& audioSourceList) const;

  // Add/remove the MixerAudioSource to the specified
  // MixerAudioSource list.
  bool AddAudioSourceToList(MixerAudioSource* audio_source,
                            MixerAudioSourceList* audioSourceList) const;
  bool RemoveAudioSourceFromList(MixerAudioSource* removeAudioSource,
                                 MixerAudioSourceList* audioSourceList) const;

  // Mix the AudioFrames stored in audioFrameList into mixedAudio.
  static int32_t MixFromList(AudioFrame* mixedAudio,
                             const AudioFrameList& audioFrameList,
                             int32_t id,
                             bool use_limiter);

  bool LimitMixedAudio(AudioFrame* mixedAudio) const;

  // Output level functions for VoEVolumeControl.
  int GetOutputAudioLevel() override;

  int GetOutputAudioLevelFullRange() override;

  rtc::CriticalSection crit_;

  const int32_t id_;

  // The current sample frequency and sample size when mixing.
  Frequency output_frequency_ ACCESS_ON(&thread_checker_);
  size_t sample_size_ ACCESS_ON(&thread_checker_);

  // List of all audio sources. Note all lists are disjunct
  MixerAudioSourceList audio_source_list_ GUARDED_BY(crit_);  // May be mixed.

  // Always mixed, anonomously.
  MixerAudioSourceList additional_audio_source_list_ GUARDED_BY(crit_);

  size_t num_mixed_audio_sources_ GUARDED_BY(crit_);
  // Determines if we will use a limiter for clipping protection during
  // mixing.
  bool use_limiter_ ACCESS_ON(&thread_checker_);

  uint32_t time_stamp_ ACCESS_ON(&thread_checker_);

  // Ensures that Mix is called from the same thread.
  rtc::ThreadChecker thread_checker_;

  // Used for inhibiting saturation in mixing.
  std::unique_ptr<AudioProcessing> limiter_ ACCESS_ON(&thread_checker_);

  // Measures audio level for the combined signal.
  voe::AudioLevel audio_level_ ACCESS_ON(&thread_checker_);
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_MIXER_AUDIO_MIXER_IMPL_H_
