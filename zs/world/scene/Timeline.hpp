#pragma once
#include <limits>

namespace zs {

  using TimeCode = double;
  inline TimeCode g_default_timecode() noexcept {
    return std::numeric_limits<TimeCode>::quiet_NaN();
  }
  enum class sequencer_component_e {
    start = 0,
    end,
    fps,
    tcps, /*timecode per second*/
    cur_tc
  };

  struct TimelineEvent {
    sequencer_component_e target{sequencer_component_e::start};
    TimeCode newVal;
  };

  struct Timeline {
    TimeCode getCurrentTimeCode() const noexcept { return _curTimeCode; }
    TimeCode getStartTimeCode() const noexcept { return _start; }
    TimeCode getEndTimeCode() const noexcept { return _end; }
    TimeCode getFps() const noexcept { return _fps; }
    TimeCode getTcps() const noexcept { return _tcps; }

    void setCurrentTimeCode(TimeCode tc = g_default_timecode()) noexcept { _curTimeCode = tc; }
    void setStartTimeCode(TimeCode tc) noexcept { _start = tc; }
    void setEndTimeCode(TimeCode tc) noexcept { _end = tc; }
    void setFps(TimeCode fps) noexcept { _fps = fps; }
    void setTcps(TimeCode tcps) noexcept { _tcps = tcps; }

  protected:
    TimeCode _start{g_default_timecode()}, _end{g_default_timecode()};
    TimeCode _fps{g_default_timecode()}, _tcps{g_default_timecode()};
    TimeCode _curTimeCode{g_default_timecode()};
  };

}  // namespace zs