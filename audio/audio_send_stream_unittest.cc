/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string>
#include <vector>

#include "webrtc/audio/audio_send_stream.h"
#include "webrtc/audio/audio_state.h"
#include "webrtc/audio/conversion.h"
#include "webrtc/base/task_queue.h"
#include "webrtc/call/mock/mock_rtc_event_log.h"
#include "webrtc/modules/congestion_controller/include/congestion_controller.h"
#include "webrtc/modules/congestion_controller/include/mock/mock_congestion_controller.h"
#include "webrtc/modules/pacing/paced_sender.h"
#include "webrtc/modules/remote_bitrate_estimator/include/mock/mock_remote_bitrate_estimator.h"
#include "webrtc/test/gtest.h"
#include "webrtc/test/mock_voe_channel_proxy.h"
#include "webrtc/test/mock_voice_engine.h"

namespace webrtc {
namespace test {
namespace {

using testing::_;
using testing::Return;

const int kChannelId = 1;
const uint32_t kSsrc = 1234;
const char* kCName = "foo_name";
const int kAudioLevelId = 2;
const int kAbsSendTimeId = 3;
const int kTransportSequenceNumberId = 4;
const int kEchoDelayMedian = 254;
const int kEchoDelayStdDev = -3;
const int kEchoReturnLoss = -65;
const int kEchoReturnLossEnhancement = 101;
const unsigned int kSpeechInputLevel = 96;
const CallStatistics kCallStats = {
    1345,  1678,  1901, 1234,  112, 13456, 17890, 1567, -1890, -1123};
const ReportBlock kReportBlock = {456, 780, 123, 567, 890, 132, 143, 13354};
const int kTelephoneEventPayloadType = 123;
const int kTelephoneEventCode = 45;
const int kTelephoneEventDuration = 6789;
const CodecInst kIsacCodec = {103, "isac", 16000, 320, 1, 32000};

class MockLimitObserver : public BitrateAllocator::LimitObserver {
 public:
  MOCK_METHOD2(OnAllocationLimitsChanged,
               void(uint32_t min_send_bitrate_bps,
                    uint32_t max_padding_bitrate_bps));
};

struct ConfigHelper {
  ConfigHelper()
      : simulated_clock_(123456),
        stream_config_(nullptr),
        congestion_controller_(&simulated_clock_,
                               &bitrate_observer_,
                               &remote_bitrate_observer_,
                               &event_log_),
        bitrate_allocator_(&limit_observer_),
        worker_queue_("ConfigHelper_worker_queue") {
    using testing::Invoke;
    using testing::StrEq;

    EXPECT_CALL(voice_engine_,
        RegisterVoiceEngineObserver(_)).WillOnce(Return(0));
    EXPECT_CALL(voice_engine_,
        DeRegisterVoiceEngineObserver()).WillOnce(Return(0));
    AudioState::Config config;
    config.voice_engine = &voice_engine_;
    audio_state_ = AudioState::Create(config);

    EXPECT_CALL(voice_engine_, ChannelProxyFactory(kChannelId))
        .WillOnce(Invoke([this](int channel_id) {
          EXPECT_FALSE(channel_proxy_);
          channel_proxy_ = new testing::StrictMock<MockVoEChannelProxy>();
          EXPECT_CALL(*channel_proxy_, SetRTCPStatus(true)).Times(1);
          EXPECT_CALL(*channel_proxy_, SetLocalSSRC(kSsrc)).Times(1);
          EXPECT_CALL(*channel_proxy_, SetRTCP_CNAME(StrEq(kCName))).Times(1);
          EXPECT_CALL(*channel_proxy_, SetNACKStatus(true, 10)).Times(1);
          EXPECT_CALL(*channel_proxy_,
              SetSendAbsoluteSenderTimeStatus(true, kAbsSendTimeId)).Times(1);
          EXPECT_CALL(*channel_proxy_,
              SetSendAudioLevelIndicationStatus(true, kAudioLevelId)).Times(1);
          EXPECT_CALL(*channel_proxy_, EnableSendTransportSequenceNumber(
                                           kTransportSequenceNumberId))
              .Times(1);
          EXPECT_CALL(*channel_proxy_,
                      RegisterSenderCongestionControlObjects(
                          congestion_controller_.pacer(),
                          congestion_controller_.GetTransportFeedbackObserver(),
                          congestion_controller_.packet_router()))
              .Times(1);
          EXPECT_CALL(*channel_proxy_, ResetCongestionControlObjects())
              .Times(1);
          EXPECT_CALL(*channel_proxy_, RegisterExternalTransport(nullptr))
              .Times(1);
          EXPECT_CALL(*channel_proxy_, DeRegisterExternalTransport())
              .Times(1);
          EXPECT_CALL(*channel_proxy_, SetRtcEventLog(testing::NotNull()))
              .Times(1);
          EXPECT_CALL(*channel_proxy_, SetRtcEventLog(testing::IsNull()))
              .Times(1);  // Destructor resets the event log
          return channel_proxy_;
        }));
    SetupMockForSetupSendCodec();
    stream_config_.voe_channel_id = kChannelId;
    stream_config_.rtp.ssrc = kSsrc;
    stream_config_.rtp.nack.rtp_history_ms = 200;
    stream_config_.rtp.c_name = kCName;
    stream_config_.rtp.extensions.push_back(
        RtpExtension(RtpExtension::kAudioLevelUri, kAudioLevelId));
    stream_config_.rtp.extensions.push_back(
        RtpExtension(RtpExtension::kAbsSendTimeUri, kAbsSendTimeId));
    stream_config_.rtp.extensions.push_back(RtpExtension(
        RtpExtension::kTransportSequenceNumberUri, kTransportSequenceNumberId));
    // Use ISAC as default codec so as to prevent unnecessary |voice_engine_|
    // calls from the default ctor behavior.
    stream_config_.send_codec_spec.codec_inst = kIsacCodec;
  }

  AudioSendStream::Config& config() { return stream_config_; }
  rtc::scoped_refptr<AudioState> audio_state() { return audio_state_; }
  MockVoEChannelProxy* channel_proxy() { return channel_proxy_; }
  CongestionController* congestion_controller() {
    return &congestion_controller_;
  }
  BitrateAllocator* bitrate_allocator() { return &bitrate_allocator_; }
  rtc::TaskQueue* worker_queue() { return &worker_queue_; }
  RtcEventLog* event_log() { return &event_log_; }
  MockVoiceEngine* voice_engine() { return &voice_engine_; }

  void SetupMockForSetupSendCodec() {
    EXPECT_CALL(voice_engine_, SetVADStatus(kChannelId, false, _, _))
        .WillOnce(Return(0));
    EXPECT_CALL(voice_engine_, SetFECStatus(kChannelId, false))
        .WillOnce(Return(0));
    // Let |GetSendCodec| return -1 for the first time to indicate that no send
    // codec has been set.
    EXPECT_CALL(voice_engine_, GetSendCodec(kChannelId, _))
        .WillOnce(Return(-1));
    EXPECT_CALL(voice_engine_, SetSendCodec(kChannelId, _)).WillOnce(Return(0));
  }

  void SetupMockForSendTelephoneEvent() {
    EXPECT_TRUE(channel_proxy_);
    EXPECT_CALL(*channel_proxy_,
        SetSendTelephoneEventPayloadType(kTelephoneEventPayloadType))
            .WillOnce(Return(true));
    EXPECT_CALL(*channel_proxy_,
        SendTelephoneEventOutband(kTelephoneEventCode, kTelephoneEventDuration))
            .WillOnce(Return(true));
  }

  void SetupMockForGetStats() {
    using testing::DoAll;
    using testing::SetArgReferee;

    std::vector<ReportBlock> report_blocks;
    webrtc::ReportBlock block = kReportBlock;
    report_blocks.push_back(block);  // Has wrong SSRC.
    block.source_SSRC = kSsrc;
    report_blocks.push_back(block);  // Correct block.
    block.fraction_lost = 0;
    report_blocks.push_back(block);  // Duplicate SSRC, bad fraction_lost.

    EXPECT_TRUE(channel_proxy_);
    EXPECT_CALL(*channel_proxy_, GetRTCPStatistics())
        .WillRepeatedly(Return(kCallStats));
    EXPECT_CALL(*channel_proxy_, GetRemoteRTCPReportBlocks())
        .WillRepeatedly(Return(report_blocks));

    EXPECT_CALL(voice_engine_, GetSendCodec(kChannelId, _))
        .WillRepeatedly(DoAll(SetArgReferee<1>(kIsacCodec), Return(0)));
    EXPECT_CALL(voice_engine_, GetSpeechInputLevelFullRange(_))
        .WillRepeatedly(DoAll(SetArgReferee<0>(kSpeechInputLevel), Return(0)));
    EXPECT_CALL(voice_engine_, GetEcMetricsStatus(_))
        .WillRepeatedly(DoAll(SetArgReferee<0>(true), Return(0)));
    EXPECT_CALL(voice_engine_, GetEchoMetrics(_, _, _, _))
        .WillRepeatedly(DoAll(SetArgReferee<0>(kEchoReturnLoss),
                        SetArgReferee<1>(kEchoReturnLossEnhancement),
                        Return(0)));
    EXPECT_CALL(voice_engine_, GetEcDelayMetrics(_, _, _))
        .WillRepeatedly(DoAll(SetArgReferee<0>(kEchoDelayMedian),
                        SetArgReferee<1>(kEchoDelayStdDev), Return(0)));
  }

 private:
  SimulatedClock simulated_clock_;
  testing::StrictMock<MockVoiceEngine> voice_engine_;
  rtc::scoped_refptr<AudioState> audio_state_;
  AudioSendStream::Config stream_config_;
  testing::StrictMock<MockVoEChannelProxy>* channel_proxy_ = nullptr;
  testing::NiceMock<MockCongestionObserver> bitrate_observer_;
  testing::NiceMock<MockRemoteBitrateObserver> remote_bitrate_observer_;
  CongestionController congestion_controller_;
  MockRtcEventLog event_log_;
  testing::NiceMock<MockLimitObserver> limit_observer_;
  BitrateAllocator bitrate_allocator_;
  // |worker_queue| is defined last to ensure all pending tasks are cancelled
  // and deleted before any other members.
  rtc::TaskQueue worker_queue_;
};
}  // namespace

TEST(AudioSendStreamTest, ConfigToString) {
  AudioSendStream::Config config(nullptr);
  config.rtp.ssrc = kSsrc;
  config.rtp.extensions.push_back(
      RtpExtension(RtpExtension::kAbsSendTimeUri, kAbsSendTimeId));
  config.rtp.c_name = kCName;
  config.voe_channel_id = kChannelId;
  config.send_codec_spec.cng_payload_type = 42;
  EXPECT_EQ(
      "{rtp: {ssrc: 1234, extensions: [{uri: "
      "http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time, id: 3}], "
      "nack: {rtp_history_ms: 0}, c_name: foo_name}, voe_channel_id: 1, "
      "cng_payload_type: 42}",
      config.ToString());
}

TEST(AudioSendStreamTest, ConstructDestruct) {
  ConfigHelper helper;
  internal::AudioSendStream send_stream(
      helper.config(), helper.audio_state(), helper.worker_queue(),
      helper.congestion_controller(), helper.bitrate_allocator(),
      helper.event_log());
}

TEST(AudioSendStreamTest, SendTelephoneEvent) {
  ConfigHelper helper;
  internal::AudioSendStream send_stream(
      helper.config(), helper.audio_state(), helper.worker_queue(),
      helper.congestion_controller(), helper.bitrate_allocator(),
      helper.event_log());
  helper.SetupMockForSendTelephoneEvent();
  EXPECT_TRUE(send_stream.SendTelephoneEvent(kTelephoneEventPayloadType,
      kTelephoneEventCode, kTelephoneEventDuration));
}

TEST(AudioSendStreamTest, SetMuted) {
  ConfigHelper helper;
  internal::AudioSendStream send_stream(
      helper.config(), helper.audio_state(), helper.worker_queue(),
      helper.congestion_controller(), helper.bitrate_allocator(),
      helper.event_log());
  EXPECT_CALL(*helper.channel_proxy(), SetInputMute(true));
  send_stream.SetMuted(true);
}

TEST(AudioSendStreamTest, GetStats) {
  ConfigHelper helper;
  internal::AudioSendStream send_stream(
      helper.config(), helper.audio_state(), helper.worker_queue(),
      helper.congestion_controller(), helper.bitrate_allocator(),
      helper.event_log());
  helper.SetupMockForGetStats();
  AudioSendStream::Stats stats = send_stream.GetStats();
  EXPECT_EQ(kSsrc, stats.local_ssrc);
  EXPECT_EQ(static_cast<int64_t>(kCallStats.bytesSent), stats.bytes_sent);
  EXPECT_EQ(kCallStats.packetsSent, stats.packets_sent);
  EXPECT_EQ(static_cast<int32_t>(kReportBlock.cumulative_num_packets_lost),
            stats.packets_lost);
  EXPECT_EQ(Q8ToFloat(kReportBlock.fraction_lost), stats.fraction_lost);
  EXPECT_EQ(std::string(kIsacCodec.plname), stats.codec_name);
  EXPECT_EQ(static_cast<int32_t>(kReportBlock.extended_highest_sequence_number),
            stats.ext_seqnum);
  EXPECT_EQ(static_cast<int32_t>(kReportBlock.interarrival_jitter /
                                 (kIsacCodec.plfreq / 1000)),
            stats.jitter_ms);
  EXPECT_EQ(kCallStats.rttMs, stats.rtt_ms);
  EXPECT_EQ(static_cast<int32_t>(kSpeechInputLevel), stats.audio_level);
  EXPECT_EQ(-1, stats.aec_quality_min);
  EXPECT_EQ(kEchoDelayMedian, stats.echo_delay_median_ms);
  EXPECT_EQ(kEchoDelayStdDev, stats.echo_delay_std_ms);
  EXPECT_EQ(kEchoReturnLoss, stats.echo_return_loss);
  EXPECT_EQ(kEchoReturnLossEnhancement, stats.echo_return_loss_enhancement);
  EXPECT_FALSE(stats.typing_noise_detected);
}

TEST(AudioSendStreamTest, GetStatsTypingNoiseDetected) {
  ConfigHelper helper;
  internal::AudioSendStream send_stream(
      helper.config(), helper.audio_state(), helper.worker_queue(),
      helper.congestion_controller(), helper.bitrate_allocator(),
      helper.event_log());
  helper.SetupMockForGetStats();
  EXPECT_FALSE(send_stream.GetStats().typing_noise_detected);

  internal::AudioState* internal_audio_state =
      static_cast<internal::AudioState*>(helper.audio_state().get());
  VoiceEngineObserver* voe_observer =
      static_cast<VoiceEngineObserver*>(internal_audio_state);
  voe_observer->CallbackOnError(-1, VE_TYPING_NOISE_WARNING);
  EXPECT_TRUE(send_stream.GetStats().typing_noise_detected);
  voe_observer->CallbackOnError(-1, VE_TYPING_NOISE_OFF_WARNING);
  EXPECT_FALSE(send_stream.GetStats().typing_noise_detected);
}

TEST(AudioSendStreamTest, SendCodecAppliesConfigParams) {
  ConfigHelper helper;
  auto stream_config = helper.config();
  const CodecInst kOpusCodec = {111, "opus", 48000, 960, 2, 64000};
  stream_config.send_codec_spec.codec_inst = kOpusCodec;
  stream_config.send_codec_spec.enable_codec_fec = true;
  stream_config.send_codec_spec.enable_opus_dtx = true;
  stream_config.send_codec_spec.opus_max_playback_rate = 12345;
  stream_config.send_codec_spec.cng_plfreq = 16000;
  stream_config.send_codec_spec.cng_payload_type = 105;
  EXPECT_CALL(*helper.voice_engine(), SetFECStatus(kChannelId, true))
      .WillOnce(Return(0));
  EXPECT_CALL(
      *helper.voice_engine(),
      SetOpusDtx(kChannelId, stream_config.send_codec_spec.enable_opus_dtx))
      .WillOnce(Return(0));
  EXPECT_CALL(
      *helper.voice_engine(),
      SetOpusMaxPlaybackRate(
          kChannelId, stream_config.send_codec_spec.opus_max_playback_rate))
      .WillOnce(Return(0));
  EXPECT_CALL(*helper.voice_engine(),
              SetSendCNPayloadType(
                  kChannelId, stream_config.send_codec_spec.cng_payload_type,
                  webrtc::kFreq16000Hz))
      .WillOnce(Return(0));
  internal::AudioSendStream send_stream(
      stream_config, helper.audio_state(), helper.worker_queue(),
      helper.congestion_controller(), helper.bitrate_allocator(),
      helper.event_log());
}

// VAD is applied when codec is mono and the CNG frequency matches the codec
// sample rate.
TEST(AudioSendStreamTest, SendCodecCanApplyVad) {
  ConfigHelper helper;
  auto stream_config = helper.config();
  const CodecInst kG722Codec = {9, "g722", 8000, 160, 1, 16000};
  stream_config.send_codec_spec.codec_inst = kG722Codec;
  stream_config.send_codec_spec.cng_plfreq = 8000;
  stream_config.send_codec_spec.cng_payload_type = 105;
  EXPECT_CALL(*helper.voice_engine(), SetVADStatus(kChannelId, true, _, _))
      .WillOnce(Return(0));
  internal::AudioSendStream send_stream(
      stream_config, helper.audio_state(), helper.worker_queue(),
      helper.congestion_controller(), helper.bitrate_allocator(),
      helper.event_log());
}

}  // namespace test
}  // namespace webrtc
