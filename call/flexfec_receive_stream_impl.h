/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_CALL_FLEXFEC_RECEIVE_STREAM_IMPL_H_
#define WEBRTC_CALL_FLEXFEC_RECEIVE_STREAM_IMPL_H_

#include <atomic>
#include <memory>

#include "webrtc/call/flexfec_receive_stream.h"
#include "webrtc/call/rtp_packet_sink_interface.h"
#include "webrtc/rtc_base/criticalsection.h"

namespace webrtc {

class FlexfecReceiver;
class ProcessThread;
class ReceiveStatistics;
class RecoveredPacketReceiver;
class RtcpRttStats;
class RtpPacketReceived;
class RtpRtcp;
class RtpStreamReceiverControllerInterface;
class RtpStreamReceiverInterface;

class FlexfecReceiveStreamImpl : public FlexfecReceiveStream {
 public:
  FlexfecReceiveStreamImpl(
      RtpStreamReceiverControllerInterface* receiver_controller,
      const Config& config,
      RecoveredPacketReceiver* recovered_packet_receiver,
      RtcpRttStats* rtt_stats,
      ProcessThread* process_thread);
  ~FlexfecReceiveStreamImpl() override;

  // RtpPacketSinkInterface.
  void OnRtpPacket(const RtpPacketReceived& packet) override;

  // Implements FlexfecReceiveStream.
  void Start() override;
  void Stop() override;
  Stats GetStats() const override;
  const Config& GetConfig() const override;

 private:
  // Config.
  const Config config_;
  std::atomic<bool> started_;

  // Erasure code interfacing.
  const std::unique_ptr<FlexfecReceiver> receiver_;

  // RTCP reporting.
  const std::unique_ptr<ReceiveStatistics> rtp_receive_statistics_;
  const std::unique_ptr<RtpRtcp> rtp_rtcp_;
  ProcessThread* process_thread_;

  std::unique_ptr<RtpStreamReceiverInterface> rtp_stream_receiver_;
};

}  // namespace webrtc

#endif  // WEBRTC_CALL_FLEXFEC_RECEIVE_STREAM_IMPL_H_
