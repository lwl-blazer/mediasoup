#ifndef MS_RTC_RTP_STREAM_SEND_HPP
#define MS_RTC_RTP_STREAM_SEND_HPP

#include "RTC/RateCalculator.hpp"
#include "RTC/RtpRetransmissionBuffer.hpp"
#include "RTC/RtpStream.hpp"
#include "RTC/SharedRtpPacket.hpp"


/**
 * Recv  	专注接收质量分析
 * Send		专注发送优化
 * 
 * Producer / Consumer 中介者协调， 避免Recv/Send直接依赖
 * 
 * 把Producer比作'水龙头'，接收水源(媒体流)
 * Router是‘水管分配器’
 * Consumer则是‘分水阀’ 控制水流向哪个终端
 * * */


/**
 * RtpStreamSend
 * 每个出去的媒体流（Consumer)都有一个对应的RtpStreamSend实例
 * 它负责:
 * 	管理发送队列
 * 	重传队列
 * 	实现平滑发送的逻辑
 *  */

/**
 * 关于WebRtc中平滑发送(Pacer)的原理:
 * 核心逻辑: RTCP反馈 --> 拥塞控制算法 --> 目标码率 --> Pacer平滑发送执行
 * 
 * 根据哪些RTCP报文:
 * Pacer 的直接输入是​​目标发送码率（Target Bitrate）​​，而这个码率是由​​拥塞控制算法​​计算得出的。拥塞控制算法的​​主要输入​​来自于以下
 * 	1.RR -- 最重要的输入
 * 		字段：fraction lost（丢包率）和 cumulative number of packets lost（累计丢包数）​
 * 			这是判断网络是否发生​​拥塞​​（即拥塞信号）的最主要依据。基于丢包的拥塞控制算法（如 WebRTC 使用的 GoogCC 算法家族）会根据丢包率来调整目标码率。
 * 			丢包率高，说明网络可能拥塞，算法会降低目标码率；反之则会提高。
 * 		interarrival jitter（到达间隔抖动）​
 * 			作为网络拥塞和健康状况的辅助判断指标。抖动增大也通常意味着网络状况不佳或排队延迟增加。
 * 	2.RTPFB
 * 		NACK
 * 		 接收端通过 NACK 明确告知发送端哪些包丢失了。虽然 Pacer 不直接处理 NACK，但​​重传这些丢失的包会产生额外的数据量，这部分数据也需要被 Pacer 调度和发送​​。
 * 		Pacer 需要将这些重传包纳入其发送预算中，可能会挤占一部分媒体数据的可用带宽
 * 	3.PSFB
 * 		REMB / TMMBR
 * 			这是​​基于延迟的拥塞控制​​算法（如 GoogCC）的一个重要​​远端带宽估计​​输入。接收端根据自己的接收情况（如排队延迟、接收速率等）估算出一个它认为可用的最大带宽，并通过 REMB 报文告知发送端。
 * 			发送端的拥塞控制算法会​​参考​​这个值来最终确定自己的目标码率
 * 拥塞控制 (GoogCcNetworkController)
 * ​​Pacer 模块 (PacingController)
 */

namespace RTC
{
	class RtpStreamSend : public RTC::RtpStream
	{
	public:
		// Maximum retransmission buffer size for video (ms).
		const static uint32_t MaxRetransmissionDelayForVideoMs;
		// Maximum retransmission buffer size for audio (ms).
		const static uint32_t MaxRetransmissionDelayForAudioMs;

	public:
		enum class ReceivePacketResult
		{
			DISCARDED               = 0,
			ACCEPTED_AND_NOT_STORED = 1,
			ACCEPTED_AND_STORED
		};

	public:
		class Listener : public RTC::RtpStream::Listener
		{
		public:
			virtual void OnRtpStreamRetransmitRtpPacket(
			  RTC::RtpStreamSend* rtpStream, RTC::RtpPacket* packet) = 0;
		};

	public:
		RtpStreamSend(
		  RTC::RtpStreamSend::Listener* listener, RTC::RtpStream::Params& params, std::string& mid);
		~RtpStreamSend() override;

		flatbuffers::Offset<FBS::RtpStream::Stats> FillBufferStats(
		  flatbuffers::FlatBufferBuilder& builder) override;
		void SetRtx(uint8_t payloadType, uint32_t ssrc) override;
		ReceivePacketResult ReceivePacket(RTC::RtpPacket* packet, const RTC::SharedRtpPacket& sharedPacket);
		void ReceiveNack(RTC::RTCP::FeedbackRtpNackPacket* nackPacket);
		void ReceiveKeyFrameRequest(RTC::RTCP::FeedbackPs::MessageType messageType);
		void ReceiveRtcpReceiverReport(RTC::RTCP::ReceiverReport* report);
		void ReceiveRtcpXrReceiverReferenceTime(RTC::RTCP::ReceiverReferenceTime* report);
		RTC::RTCP::SenderReport* GetRtcpSenderReport(uint64_t nowMs);
		RTC::RTCP::DelaySinceLastRr::SsrcInfo* GetRtcpXrDelaySinceLastRrSsrcInfo(uint64_t nowMs);
		RTC::RTCP::SdesChunk* GetRtcpSdesChunk();
		void Pause() override;
		void Resume() override;
		uint32_t GetBitrate(uint64_t nowMs) override
		{
			return this->transmissionCounter.GetBitrate(nowMs);
		}
		uint32_t GetBitrate(uint64_t nowMs, uint8_t spatialLayer, uint8_t temporalLayer) override;
		uint32_t GetSpatialLayerBitrate(uint64_t nowMs, uint8_t spatialLayer) override;
		uint32_t GetLayerBitrate(uint64_t nowMs, uint8_t spatialLayer, uint8_t temporalLayer) override;

	private:
		bool StorePacket(RTC::RtpPacket* packet, const RTC::SharedRtpPacket& sharedPacket);
		void FillRetransmissionContainer(uint16_t seq, uint16_t bitmask);
		void UpdateScore(RTC::RTCP::ReceiverReport* report);

		/* Pure virtual methods inherited from RTC::RtpStream. */
	public:
		void UserOnSequenceNumberReset() override;

	private:
		// Packets lost at last interval for score calculation.
		uint32_t lostPriorScore{ 0u };
		// Packets sent at last interval for score calculation.
		uint32_t sentPriorScore{ 0u };
		std::string mid;
		uint16_t rtxSeq{ 0u };
		RTC::RtpDataCounter transmissionCounter;
		RTC::RtpRetransmissionBuffer* retransmissionBuffer{ nullptr };
		// The middle 32 bits out of 64 in the NTP timestamp received in the most
		// recent receiver reference timestamp.
		uint32_t lastRrTimestamp{ 0u };
		// Wallclock time representing the most recent receiver reference timestamp
		// arrival.
		uint64_t lastRrReceivedMs{ 0u };
	};
} // namespace RTC

#endif
