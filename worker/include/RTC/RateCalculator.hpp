#ifndef MS_RTC_RATE_CALCULATOR_HPP
#define MS_RTC_RATE_CALCULATOR_HPP

#include "common.hpp"
#include "DepLibUV.hpp"
#include "RTC/RtpPacket.hpp"
#include <optional>
#include <vector>

/**
 * RateCalculator
 * 功能:
 * 	实时计算数据库速率 (比特率、包率)
 * 	基于滑动时间窗口算法，避免瞬时波动影响
 * 	支持自定义统计窗口
 * 
 */

 /**
  * 设计亮点:
  * 	A. 高效滑动窗口
  * 		时间复杂度O(1) 添加、查询均为常数时间
  * 		空间优化 固定大小环形缓冲区(无动态内存分配)
  * 	B. 动态窗口维护
  * 	C. 避免重复计算
  */

namespace RTC
{
	// It is considered that the time source increases monotonically.
	// ie: the current timestamp can never be minor than a timestamp in the past.
	class RateCalculator
	{
	public:
		static constexpr size_t DefaultWindowSize{ 1000u };
		static constexpr float DefaultBpsScale{ 8000.0f };
		static constexpr uint16_t DefaultWindowItems{ 100u };

	public:
		explicit RateCalculator(
		  size_t windowSizeMs  = DefaultWindowSize,
		  float scale          = DefaultBpsScale,
		  uint16_t windowItems = DefaultWindowItems);
			// 新增数据到当前时间桶
		void Update(size_t size, uint64_t nowMs);
			// 计算当前速率
		uint32_t GetRate(uint64_t nowMs);

		size_t GetBytes() const
		{
			return this->bytes;
		}

		void Reset();

	private:
		// 淘汰过期数据(维护滑动窗口)
		void RemoveOldData(uint64_t nowMs);

	private:
		struct BufferItem
		{
			size_t count{ 0u };  // 本时间段内的数据量 如字节数
			uint64_t time{ 0u };	// 时间段起始时间戳
		};

	private:
		// Window Size (in milliseconds).
		size_t windowSizeMs{ DefaultWindowSize };
		// Scale in which the rate is represented.
		float scale{ DefaultBpsScale };
		// Window Size (number of items).
		uint16_t windowItems{ DefaultWindowItems };  // 时间窗口分割 将总窗口(如1000ms)分割为多个桶(如100个桶，每个桶10ms)
		// Item Size (in milliseconds), calculated as: windowSizeMs / windowItems.
		size_t itemSizeMs{ 0u };
		// Buffer to keep data.
		std::vector<BufferItem> buffer;   // 环形缓冲区
		// Time (in milliseconds) for last item in the time window.
		std::optional<uint64_t> newestItemStartTime{ std::nullopt };
		// Index for the last item in the time window.
		int32_t newestItemIndex{ -1 };
		// Time (in milliseconds) for oldest item in the time window.
		std::optional<uint64_t> oldestItemStartTime{ std::nullopt };
		// Index for the oldest item in the time window.
		int32_t oldestItemIndex{ -1 };
		// Total count in the time window.
		size_t totalCount{ 0u };
		// Total bytes transmitted.
		size_t bytes{ 0u };
		// Last value calculated by GetRate().
		uint32_t lastRate{ 0u };
		// Last time GetRate() was called.
		std::optional<uint64_t> lastTime{ std::nullopt };
	};

	/***
	 * RtpDataCounter类
	 * 功能:
	 *  专为RTP流量定制计数器
	 * 	过滤无效数据 可选忽略纯填充包 (padding-only packet)
	 * 	集成速率计算
	 */
	class RtpDataCounter
	{
	public:
		explicit RtpDataCounter(bool ignorePaddingOnlyPackets, size_t windowSizeMs = 2500)
		  : ignorePaddingOnlyPackets(ignorePaddingOnlyPackets), rate(windowSizeMs)
		{
		}

	public:
		void Update(RTC::RtpPacket* packet);

		uint32_t GetBitrate(uint64_t nowMs)
		{
			return this->rate.GetRate(nowMs);
		}

		size_t GetPacketCount() const
		{
			return this->packets;
		}

		size_t GetBytes() const
		{
			return this->rate.GetBytes();
		}

	private:
		// Whether the size of padding only RTP packets should not be taken into
		// account
		bool ignorePaddingOnlyPackets{ false };
		RateCalculator rate;
		size_t packets{ 0u };
	};
} // namespace RTC

#endif
