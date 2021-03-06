/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_VIE_RECEIVER_H_
#define WEBRTC_VIDEO_VIE_RECEIVER_H_

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "webrtc/base/criticalsection.h"
#include "webrtc/engine_configurations.h"
#include "webrtc/modules/rtp_rtcp/include/receive_statistics.h"
#include "webrtc/modules/rtp_rtcp/include/remote_ntp_time_estimator.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_payload_registry.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class FecReceiver;
class PacedSender;
class PacketRouter;
class RemoteNtpTimeEstimator;
class ReceiveStatistics;
class RemoteBitrateEstimator;
class RtcpRttStats;
class RtpHeaderParser;
class RTPPayloadRegistry;
class RtpReceiver;
class Transport;

namespace vcm {
class VideoReceiver;
}  // namespace vcm

class ViEReceiver : public RtpData {
 public:
  ViEReceiver(vcm::VideoReceiver* video_receiver,
              RemoteBitrateEstimator* remote_bitrate_estimator,
              RtpFeedback* rtp_feedback,
              Transport* transport,
              RtcpRttStats* rtt_stats,
              PacedSender* paced_sender,
              PacketRouter* packet_router);
  ~ViEReceiver();

  bool SetReceiveCodec(const VideoCodec& video_codec);

  void SetNackStatus(bool enable, int max_nack_reordering_threshold);
  void SetRtxPayloadType(int payload_type, int associated_payload_type);
  // If set to true, the RTX payload type mapping supplied in
  // |SetRtxPayloadType| will be used when restoring RTX packets. Without it,
  // RTX packets will always be restored to the last non-RTX packet payload type
  // received.
  void SetUseRtxPayloadMappingOnRestore(bool val);
  void SetRtxSsrc(uint32_t ssrc);
  bool GetRtxSsrc(uint32_t* ssrc) const;

  bool IsFecEnabled() const;

  uint32_t GetRemoteSsrc() const;
  int GetCsrcs(uint32_t* csrcs) const;

  RtpReceiver* GetRtpReceiver() const;
  RtpRtcp* rtp_rtcp() const { return rtp_rtcp_.get(); }

  void EnableReceiveRtpHeaderExtension(const std::string& extension, int id);
  void RegisterRtcpPacketTypeCounterObserver(
      RtcpPacketTypeCounterObserver* observer);

  void StartReceive();
  void StopReceive();

  bool DeliverRtp(const uint8_t* rtp_packet,
                  size_t rtp_packet_length,
                  const PacketTime& packet_time);
  bool DeliverRtcp(const uint8_t* rtcp_packet, size_t rtcp_packet_length);

  // Implements RtpData.
  int32_t OnReceivedPayloadData(const uint8_t* payload_data,
                                const size_t payload_size,
                                const WebRtcRTPHeader* rtp_header) override;
  bool OnRecoveredPacket(const uint8_t* packet, size_t packet_length) override;

  ReceiveStatistics* GetReceiveStatistics() const;

  template <class T>
  class RegisterableCallback : public T {
   public:
    RegisterableCallback() : callback_(nullptr) {}

    void Set(T* callback) {
      rtc::CritScope lock(&critsect_);
      callback_ = callback;
    }

   protected:
    // Note: this should be implemented with a RW-lock to allow simultaneous
    // calls into the callback. However that doesn't seem to be needed for the
    // current type of callbacks covered by this class.
    rtc::CriticalSection critsect_;
    T* callback_ GUARDED_BY(critsect_);

   private:
    RTC_DISALLOW_COPY_AND_ASSIGN(RegisterableCallback);
  };

  class RegisterableRtcpPacketTypeCounterObserver
      : public RegisterableCallback<RtcpPacketTypeCounterObserver> {
   public:
    void RtcpPacketTypesCounterUpdated(
        uint32_t ssrc,
        const RtcpPacketTypeCounter& packet_counter) override {
      rtc::CritScope lock(&critsect_);
      if (callback_)
        callback_->RtcpPacketTypesCounterUpdated(ssrc, packet_counter);
    }

   private:
  } rtcp_packet_type_counter_observer_;

 private:
  bool ReceivePacket(const uint8_t* packet,
                     size_t packet_length,
                     const RTPHeader& header,
                     bool in_order);
  // Parses and handles for instance RTX and RED headers.
  // This function assumes that it's being called from only one thread.
  bool ParseAndHandleEncapsulatingHeader(const uint8_t* packet,
                                         size_t packet_length,
                                         const RTPHeader& header);
  void NotifyReceiverOfFecPacket(const RTPHeader& header);
  bool IsPacketInOrder(const RTPHeader& header) const;
  bool IsPacketRetransmitted(const RTPHeader& header, bool in_order) const;
  void UpdateHistograms();

  Clock* const clock_;
  vcm::VideoReceiver* const video_receiver_;
  RemoteBitrateEstimator* const remote_bitrate_estimator_;
  PacketRouter* const packet_router_;

  RemoteNtpTimeEstimator ntp_estimator_;
  RTPPayloadRegistry rtp_payload_registry_;

  const std::unique_ptr<RtpHeaderParser> rtp_header_parser_;
  const std::unique_ptr<RtpReceiver> rtp_receiver_;
  const std::unique_ptr<ReceiveStatistics> rtp_receive_statistics_;
  std::unique_ptr<FecReceiver> fec_receiver_;

  rtc::CriticalSection receive_cs_;
  bool receiving_ GUARDED_BY(receive_cs_);
  uint8_t restored_packet_[IP_PACKET_SIZE] GUARDED_BY(receive_cs_);
  bool restored_packet_in_use_ GUARDED_BY(receive_cs_);
  int64_t last_packet_log_ms_ GUARDED_BY(receive_cs_);

  const std::unique_ptr<RtpRtcp> rtp_rtcp_;
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_VIE_RECEIVER_H_
