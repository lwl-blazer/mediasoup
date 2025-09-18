#ifndef MS_RTC_RTCP_FEEDBACK_HPP
#define MS_RTC_RTCP_FEEDBACK_HPP

#include "common.hpp"
#include "RTC/RTCP/Packet.hpp"
#include <absl/container/flat_hash_map.h>
/*** 总结:
 * 1.错误恢复:通过NACK和PLI处理包丢失和编解码错误
 * 2.拥塞控制:通过TMMBR/TMMBN和TCC实现
 * 3.质量优化:通过SLI,RPSI等优化视频质量
 * 4.低延迟支持:通过PSLEI和TLLEI优化实时性能
 * 
 * 
 * TMMBR/TMMBN
 * 接收端计算可用带宽   -> 发送TMMBR请求限制发送端比特率  -> 发送端通知接收端当前遵守的比特率限制
 * 基于本地可观测的指标来评估网络健康状况，并决定是否要发送TMMBR请求，其主要计算依据是丢包率
 * 1.关键指标测量：
 *      接收端会持续跟踪两个核心指标:
 *      .累计预期包数: 根据RTP序列号回绕计算出的应该收到的包总数
 *      .累计实际接收包数: 实际成功接收并处理的包数
 * 2.决策逻辑：是否发送TMMBR 
 *      .接收端有一个简单的基于丢包率的阈值决策逻辑
 * 3.计算建议的比特率
 *      当决定要发送TMMBR后，接收端需要计算一个具体的比特率值，一个常见的方法是基于接收速率和丢包率进行反向核算
 * 
 * TCC使用传输范围内的包到达时间信息进行拥塞控制
 * GCC是WebRTC使用的核心拥塞控制算法，结合了基于延迟和基于丢包的两种控制策略 都运行在发送端
 *  基于TCC反馈解析 和RR的报文解析    做带宽估计 ,码率调整决策，编码器参数调整
 * 
 * 1.基于延迟的控制
 *      延迟梯度计算 状态机处理 带宽估计更新 
 * 2.基于丢包的控制(接收端->发送端)
 *     根据丢包率调速带宽    
 * 3.最终带宽决策
 *      结合基于延迟和基于丢包的估计       
 */


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
