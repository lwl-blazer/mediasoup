#ifndef MS_RTC_RTCP_FEEDBACK_HPP
#define MS_RTC_RTCP_FEEDBACK_HPP

#include "common.hpp"
#include "RTC/RTCP/Packet.hpp"
#include <absl/container/flat_hash_map.h>

namespace RTC
{
	namespace RTCP
	{
		template<typename T>
		class FeedbackPacket : public Packet
		{
		public:
			/* Struct for RTP Feedback message. */
			struct Header
			{
				uint32_t senderSsrc;
				uint32_t mediaSsrc;
			};

		public:
			static const size_t HeaderSize{ 8 };
			static RTCP::Type rtcpType;
			static FeedbackPacket<T>* Parse(const uint8_t* data, size_t len);
			static const std::string& MessageType2String(typename T::MessageType type);

		private:
			static absl::flat_hash_map<typename T::MessageType, std::string> type2String;

		public:
			typename T::MessageType GetMessageType() const
			{
				return this->messageType;
			}
			uint32_t GetSenderSsrc() const
			{
				return uint32_t{ ntohl(this->header->senderSsrc) };
			}
			void SetSenderSsrc(uint32_t ssrc)
			{
				this->header->senderSsrc = uint32_t{ htonl(ssrc) };
			}
			uint32_t GetMediaSsrc() const
			{
				return uint32_t{ ntohl(this->header->mediaSsrc) };
			}
			void SetMediaSsrc(uint32_t ssrc)
			{
				this->header->mediaSsrc = uint32_t{ htonl(ssrc) };
			}

			/* Pure virtual methods inherited from Packet. */
		public:
			void Dump() const override;
			size_t Serialize(uint8_t* buffer) override;
			size_t GetCount() const override
			{
				return static_cast<size_t>(GetMessageType());
			}
			size_t GetSize() const override
			{
				return Packet::CommonHeaderSize + HeaderSize;
			}

		protected:
			explicit FeedbackPacket(CommonHeader* commonHeader);
			FeedbackPacket(typename T::MessageType messageType, uint32_t senderSsrc, uint32_t mediaSsrc);
			~FeedbackPacket() override;

		private:
			Header* header{ nullptr };
			uint8_t* raw{ nullptr };
			typename T::MessageType messageType;
		};
		/*** 
		 * PSFB ---- Payload-Specific Feedback
		 * 负载特定反馈
		 * 应用/编解码器层
		 * 主要针对 媒体流的生成过程 （如编码器)
		 * 常见类型:
		 * PLI -- 图像丢失指示
		 * 	接收端通知发送端它丢失了完整的视频帧或无法解码
		 * 	发送端通常响应以一个完整的关键帧（I-Frame) 
		 * 	在视频会议中，新加入者请求一个关键帧来开始解码或者网络丢包导致解码失败时
		 * 
		 * SLI -- 分片丢失指示
		 * 	丢失了某一个部分 （例如宏块）
		 * 	只重传丢失的的那部分，而不是整个帧
		 * 
		 * RPSI -- 参考帧选择指示
		 * 	
		 * AFB -- 应用层反馈
		 * 	一个通用容器 可以携带任何应用自定义的反馈信息 最著名的例子:REMB  
		 * 	REMB示例：接收端根据自身接收情况和网络状况，估算出一个它能处理最大的码率，并通过AFB报文告知发送端，发送端据此动态调整视频编码码率
		 *  
		 */
		class FeedbackPs
		{
		public:
			enum class MessageType : uint8_t
			{
				PLI   = 1,
				SLI   = 2,
				RPSI  = 3,
				FIR   = 4,
				TSTR  = 5,
				TSTN  = 6,
				VBCM  = 7,
				PSLEI = 8,
				ROI   = 9,
				AFB   = 15,
				EXT   = 31
			};
		};
		/** RTPFB -- Transport Layer Feedback 
		 * 传输层反馈
		 * 主要关注RTP包在网络中传输的状态，如丢包、延迟、抖动等，用于管理传过程本身
		 * 常见报文类型:
		 * 	NACK --- 否定确认
		 * 		接收端明确通知发送端哪些特定的RTP包丢失了(通过序列号标识)
		 * 		发送端收到后，会重传指定的数据包
		 * 	TMMBR -- 临时最大媒体流码率请求
		 * 		接收端请求发送端临时限制其发送某路媒体流的码率上限
		 * 		与REMB的区别：
		 * 			TMMBR 是​​命令式​​的（“你必须将码率降到 X”），而 REMB 是​​建议式​​的（“我估计我能处理 Y 的码率”）。TMMBR 通常由 MCU（多点控制单元）等中间设备发出
		 * 	TMMBN --- 临时最大媒体流码率通知
		 * 		发送端用来响应TMMBR,通知所有接收者它当前正在遵守的码率上限是多少
		 **/
		class FeedbackRtp
		{
		public:
			enum class MessageType : uint8_t
			{
				NACK   = 1,
				TMMBR  = 3,
				TMMBN  = 4,
				SR_REQ = 5,
				RAMS   = 6,
				TLLEI  = 7,
				ECN    = 8,
				PS     = 9,
				TCC    = 15,
				EXT    = 31
			};
		};

		using FeedbackPsPacket  = FeedbackPacket<RTC::RTCP::FeedbackPs>;
		using FeedbackRtpPacket = FeedbackPacket<RTC::RTCP::FeedbackRtp>;
	} // namespace RTCP
} // namespace RTC

#endif
