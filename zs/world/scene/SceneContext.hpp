#pragma once
#include "../node/Context.hpp"
#include "Timeline.hpp"
//
#include <deque>
#include <map>

#include "Primitive.hpp"
#include "zensim/ZpcMeta.hpp"

namespace zs {

  struct ZS_WORLD_EXPORT SceneContext {
    Weak<ZsPrimitive> getPrimitive(std::string_view label);
    Weak<ZsPrimitive> getPrimitiveByIdRecurse(PrimIndex id_);
    Weak<ZsPrimitive> getPrimitiveByPath(const std::vector<std::string> &path);
    std::vector<std::string> getPrimitiveLabels() const;
    std::vector<Weak<ZsPrimitive>> getPrimitives();
    bool registerPrimitive(std::string_view label, Shared<ZsPrimitive> prim);

    struct Entry {
      std::string label;
      Weak<ZsPrimitive> prim;
      Entry(std::string_view l, Weak<ZsPrimitive> p) : label{l}, prim{p} {}
    };

    auto begin() noexcept { return _orderedPrims.begin(); }
    auto end() noexcept { return _orderedPrims.end(); }
    auto begin() const noexcept { return _orderedPrims.cbegin(); }
    auto end() const noexcept { return _orderedPrims.cend(); }

    ZsPrimitive *getPrimByIndex(int i) noexcept;
    const ZsPrimitive *getPrimByIndex(int i) const noexcept;
    std::string_view getPrimLabelByIndex(int i) const noexcept;
    size_t numPrimitives() const noexcept { return _primitives.size(); }

    auto &onTimelineSetupChanged() { return _timelineChanged; }

    TimeCode getCurrentTimeCode() const noexcept { return _timeline.getCurrentTimeCode(); }
    TimeCode getStartTimeCode() const noexcept { return _timeline.getStartTimeCode(); }
    TimeCode getEndTimeCode() const noexcept { return _timeline.getEndTimeCode(); }
    TimeCode getFps() const noexcept { return _timeline.getFps(); }
    TimeCode getTcps() const noexcept { return _timeline.getTcps(); }

    /// @note might be triggered by sequencer widget's signal
    void setCurrentTimeCode(TimeCode tc = g_default_timecode()) {
      _timeline.setCurrentTimeCode(tc);
      onTimelineSetupChanged().emit({TimelineEvent{sequencer_component_e::cur_tc, tc}});
    }
    /// @note timeline setup changed actions
    void setStartTimeCode(TimeCode tc) {
      _timeline.setStartTimeCode(tc);
      onTimelineSetupChanged().emit({TimelineEvent{sequencer_component_e::start, tc}});
    }
    void setEndTimeCode(TimeCode tc) {
      _timeline.setEndTimeCode(tc);
      onTimelineSetupChanged().emit({TimelineEvent{sequencer_component_e::end, tc}});
    }
    void setFps(TimeCode fps) {
      _timeline.setFps(fps);
      onTimelineSetupChanged().emit({TimelineEvent{sequencer_component_e::fps, fps}});
    }
    void setTcps(TimeCode tcps) {
      _timeline.setTcps(tcps);
      onTimelineSetupChanged().emit({TimelineEvent{sequencer_component_e::tcps, tcps}});
    }

  protected:
    std::map<std::string, Shared<ZsPrimitive>, std::less<>> _primitives;
    std::map<PrimIndex, Weak<ZsPrimitive>> _idToPrim;
    std::deque<Entry> _orderedPrims;

    // timeline
    Timeline _timeline;
    Signal<void(const std::vector<TimelineEvent> &events)> _timelineChanged;

    ///
    int _focusId{-1};    // corresponds to _orderedPrims
    int _hoveredId{-1};  // corresponds to _orderedPrims
  };

}  // namespace zs