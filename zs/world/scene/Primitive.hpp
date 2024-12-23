#pragma once
#include <deque>
#include <set>

#include "world/core/Concepts.hpp"
#include "../SceneInterface.hpp"
#include "world/core/Serialization.hpp"
#include "world/core/Signal.hpp"
#include "../WorldExport.hpp"
#include "../async/Coro.hpp"
#include "Timeline.hpp"
#include "zensim/container/Bvh.hpp"
#include "zensim/container/TileVector.hpp"
#include "zensim/container/Vector.hpp"
#include "zensim/types/ImplPattern.hpp"
#include "zensim/visitors/ObjectVisitor.hpp"

namespace zs {

  using PrimIndex = i32;
  using PrimTypeIndex = i32;

  constexpr auto prim_id_c = wrapt<PrimIndex>{};

/**
 * @brief reserved prefix
 * @note __f: entry reinterpreted as floating point (default)
 * @note __i: entry reinterpreted as int
 * @note __u: entry reinterpreted as unsigned int
 * @note zs_: indication of a preserved keyword
 */

/// @note used in [verts]
#define POINT_ID_TAG "__i_zs_pid"
/// @note used in [prims] (e.g. point/line/tri or poly)
#define ELEM_VERT_ID_TAG "__i_zs_ids"
#define POLY_OFFSET_TAG "__i_zs_offset"
#define POLY_SIZE_TAG "__i_zs_size"
/// @note used in simple [prims] (e.g. point/line/tri -> poly, i.e. belongs to)
#define TO_POLY_ID_TAG "__i_zs__map"

  enum class prim_attrib_owner_e : u32 { prim = 0, point, vert, face };
#define ATTRIB_POS_TAG "zs_pos"
#define ATTRIB_SKINNING_POS_TAG "zs_pos_skinning"
#define ATTRIB_NORMAL_TAG "zs_nrm"
#define ATTRIB_COLOR_TAG "zs_clr"
#define ATTRIB_TANGENT_TAG "zs_tan"
#define ATTRIB_TEXTURE_ID_TAG "__i_zs_texid"
#define ATTRIB_UV_TAG "zs_uv"

#define KEYFRAME_ATTRIB_POS_LABEL "zs_pos"
#define KEYFRAME_ATTRIB_FACE_INDEX_LABEL "zs_vert"
#define KEYFRAME_ATTRIB_FACE_LABEL "zs_poly"
#define KEYFRAME_ATTRIB_UV_LABEL "zs_uv"
#define KEYFRAME_ATTRIB_COLOR_LABEL "zs_color"
#define KEYFRAME_ATTRIB_NORMAL_LABEL "zs_normal"
#define KEYFRAME_ATTRIB_TANGENT_LABEL "zs_tangent"
#define KEYFRAME_ATTRIB_TRANSFORM_LABEL "zs_transform"

  struct ZsPrimitive;
  struct ZS_WORLD_EXPORT PrimHandle {
    inline operator PrimIndex() const noexcept;
    // void setId(PrimIndex id) noexcept { _id = id; }
    // PrimIndex getId(PrimIndex id) const noexcept { return _id; }

  private:
    Weak<ZsPrimitive> _primPtr;
    // PrimIndex _id{-1};
  };

  struct ZS_WORLD_EXPORT PackGeometry : PrimHandle {
    operator PrimIndex() const noexcept { return PrimHandle::operator PrimIndex(); }
  };

  /// group
  template <Container ContainerT> struct GeoGroup {
    static_assert(is_same_v<typename ContainerT::value_type, PrimIndex>,
                  "container value_type should be the same as PrimIndex (i.e. int)");
    ContainerT _ids;
  };

  /// attribute vector
  using String = Vector<char>;  // utf8 str

  struct ZS_WORLD_EXPORT AttrVector {
    using size_type = TileVector<f32>::size_type;
    // query
    size_type size() const noexcept { return _attr32.size(); }
    // modifiers
    template <typename Policy>
    void appendProperties32(Policy&& pol, const std::vector<PropertyTag>& tags,
                            const source_location& loc = source_location::current()) {
      _attr32.append_channels(pol, tags, loc);
    }
    template <typename Policy>
    void appendProperties64(Policy&& pol, const std::vector<PropertyTag>& tags,
                            const source_location& loc = source_location::current()) {
      _attr64.append_channels(pol, tags, loc);
    }
    void resize(size_t size) {
      _attr32.resize(size);
      _attr64.resize(size);
    }
    void clear() {
      resize(0);
      _strings.clear();
    }
    // lookup
    auto& attr32() noexcept { return _attr32; }
    auto& attr64() noexcept { return _attr64; }
    auto& strings() noexcept { return _strings; }
    const auto& attr32() const noexcept { return _attr32; }
    const auto& attr64() const noexcept { return _attr64; }
    const auto& strings() const noexcept { return _strings; }
    bool hasProperty(const SmallString& tag) const { return _attr32.hasProperty(tag); }
    bool hasProperty64(const SmallString& tag) const { return _attr64.hasProperty(tag); }
    auto getPropertyOffset(const SmallString& tag) const { return _attr32.getPropertyOffset(tag); }
    auto getPropertyOffset64(const SmallString& tag) const {
      return _attr64.getPropertyOffset(tag);
    }
    auto getPropertySize(const SmallString& tag) const { return _attr32.getPropertySize(tag); }
    auto getPropertySize64(const SmallString& tag) const { return _attr64.getPropertySize(tag); }
    auto getProperties() const { return _attr32.getPropertyTags(); }
    auto getProperties64() const { return _attr64.getPropertyTags(); }

    inline void printDbg(std::string_view msg) const;
    template <typename T> inline void printAttrib(const SmallString& prop, wrapt<T>);

    TileVector<f32> _attr32;
    TileVector<u64> _attr64;  // on x64 arch, store address handle here
    std::vector<String> _strings;
    prim_attrib_owner_e _owner{prim_attrib_owner_e::prim};
  };

#if ZS_ENABLE_SERIALIZATION
  template <typename S> void serialize(S& s, AttrVector& attrVector) {
    serialize(s, attrVector.attr32());
    serialize(s, attrVector.attr64());
    s.container(attrVector._strings, g_max_serialization_size_limit, [](S& s, String& string) {
      serialize(s, string);  // this is essentially a zs::Vector
    });
    // s.text1b(attrVector._strings, g_max_serialization_size_limit);
    // static_assert(sizeof(attrVector._owner) == 4, "owner enum should be 32-bit");
    // s.value4b(s, attrVector._owner);
    s.template value<sizeof(attrVector._owner)>(attrVector._owner);
  }
#endif

  template <execspace_e space, bool Base = !ZS_ENABLE_OFB_ACCESS_CHECK>
  inline auto view32(AttrVector& attr, wrapv<Base> base = {}) {
    return view<space>(attr.attr32(), base);
  }
  template <execspace_e space, bool Base = !ZS_ENABLE_OFB_ACCESS_CHECK>
  inline auto view32(const AttrVector& attr, wrapv<Base> base = {}) {
    return view<space>(attr.attr32(), base);
  }
  template <execspace_e space, bool Base = !ZS_ENABLE_OFB_ACCESS_CHECK>
  inline auto view32(const std::vector<SmallString>& tagNames, AttrVector& attr,
                     wrapv<Base> base = {}) {
    return view<space>(tagNames, attr.attr32(), base);
  }
  template <execspace_e space, bool Base = !ZS_ENABLE_OFB_ACCESS_CHECK>
  inline auto view32(const std::vector<SmallString>& tagNames, const AttrVector& attr,
                     wrapv<Base> base = {}) {
    return view<space>(tagNames, attr.attr32(), base);
  }

  template <typename Pol, typename... TimeCodes,
            enable_if_all<is_same_v<remove_cvref_t<TimeCodes>, std::vector<TimeCode>>...> = 0>
  std::vector<TimeCode> merge_timecodes(Pol&& pol, TimeCodes&&... tcs) {
    std::vector<u32> numTcs{static_cast<u32>(tcs.size())...};
    std::vector<u32> tcOffsets(sizeof...(tcs));  // i.e. tc (segment) offsets
    exclusive_scan(pol, zs::begin(numTcs), zs::end(numTcs), zs::begin(tcOffsets));
    // concatenate
    const auto numTotalEntries = (tcs.size() + ...);
    if (numTotalEntries == 0) return {};
    std::vector<const TimeCode*> tcSegments{tcs.data()...};
    std::vector<TimeCode> concatTcs(numTotalEntries);
    for (auto [tcs, sz, offset] : zip(tcSegments, numTcs, tcOffsets)) {
      pol(enumerate(range(tcs, tcs + sz)),
          [&, offset = offset](u32 i, TimeCode tc) { concatTcs[offset + i] = tc; });
    }
    // sort
    merge_sort(pol, zs::begin(concatTcs), zs::end(concatTcs));
    // mark
    std::vector<u32> distinctLocs(numTotalEntries);
    tcOffsets.resize(numTotalEntries, 0);
    pol(range(numTotalEntries),
        [&](u32 i) { distinctLocs[i] = (i == 0 || concatTcs[i] != concatTcs[i - 1]); });
    // scan
    exclusive_scan(pol, zs::begin(distinctLocs), zs::end(distinctLocs), zs::begin(tcOffsets));
    // gather
    std::vector<TimeCode> ret(tcOffsets.back() + distinctLocs.back());
    pol(range(numTotalEntries), [&](u32 i) {
      if (distinctLocs[i]) {
        auto dst = tcOffsets[i];
        ret[dst] = concatTcs[i];
      }
    });
    return ret;
  }

  template <typename Value> struct KeyFrames {
    KeyFrames() = default;
    ~KeyFrames() = default;
    KeyFrames(KeyFrames&&) noexcept = default;
    KeyFrames& operator=(KeyFrames&&) noexcept = default;
    KeyFrames(const KeyFrames&) = delete;
    KeyFrames& operator=(const KeyFrames&) = delete;

    // keyframe insertion
    bool emplace(TimeCode tc, Value* v) {
      auto ret = _keyframes.emplace(tc, Shared<Value>(v)).second;
      if (ret) _dirty = true;
      return ret;
    }
    bool emplace(TimeCode tc, Value&& v) {
      auto ret = _keyframes.emplace(tc, std::make_shared<Value>(zs::move(v))).second;
      if (ret) _dirty = true;
      return ret;
    }
    bool emplace(TimeCode tc, const Value& v) {
      auto ret = _keyframes.emplace(tc, std::make_shared<Value>(v)).second;
      if (ret) _dirty = true;
      return ret;
    }
    // default value init
    bool emplace(Value* v) {
      _defaultValue = Shared<Value>(v);
      return true;
    }
    bool emplace(Value&& v) {
      _defaultValue = std::make_shared<Value>(zs::move(v));
      return true;
    }
    bool emplace(const Value& v) {
      _defaultValue = std::make_shared<Value>(v);
      return true;
    }

    bool hasDefaultValue() const noexcept { return _defaultValue.get(); }
    bool isTimeDependent() const noexcept { return _keyframes.size() > 0; }
    inline void updateSequence();

    inline const std::vector<TimeCode>& getTimeCodes() const;
    inline TimeCode getSegmentTimeCode(int segmentNo) const;
    inline Weak<Value> getSegmentFrame(int segmentNo) const;
    inline int getTimeCodeSegmentIndex(TimeCode tc) const;
    inline int getNumFrames() const noexcept { return _keyframes.size(); }

    Weak<Value> getByTimeCode(TimeCode tc) const {
      return getSegmentFrame(getTimeCodeSegmentIndex(tc));
    }

    template <typename Float, typename T = Value,
              typename Ret = decltype((declval<T>() + declval<T>()) * (Float)0.5)>
    inline Ret getInterpolation(Float tc);

    auto& refDirty() noexcept { return _dirty; }
    auto& refDefaultValue() noexcept { return _defaultValue; }
    auto& refKeyframes() noexcept { return _keyframes; }

  protected:
    Shared<Value> _defaultValue;
    std::map<TimeCode, Shared<Value>> _keyframes{};
    /// @note auxiliary ascending entries for binary search
    std::vector<TimeCode> _orderedKeys{};
    std::vector<Weak<Value>> _orderedFrames{};
    bool _dirty{false};
  };

#if ZS_ENABLE_SERIALIZATION
  template <typename S, typename Value> void serialize(S& s, KeyFrames<Value>& keyframes) {
    if constexpr (is_fundamental_v<Value>)
      s.template ext<sizeof(Value)>(keyframes.refDefaultValue(), BitseryStdSmartPtr{});
    else
      s.ext(keyframes.refDefaultValue(), BitseryStdSmartPtr{});
    s.ext(keyframes.refKeyframes(), bitsery::ext::StdMap{g_max_serialization_size_limit},
          [](S& s, TimeCode& tc, Shared<Value>& v) {
            s.value8b(tc);
            if constexpr (is_fundamental_v<Value>)
              s.template ext<sizeof(Value)>(v, BitseryStdSmartPtr{});
            else
              s.ext(v, BitseryStdSmartPtr{});
          });
    keyframes.refDirty() = false;
  }
#endif
  struct PrimKeyFrames {
    /// @brief key frame insertion

    // bool emplacePrimKeyFrame(TimeCode tc, ZsPrimitive* prim) { return _prims.emplace(tc, prim); }
    bool emplaceAttribKeyFrame(const std::string& label, TimeCode tc, AttrVector&& attrib) {
      return _attribs[label].emplace(tc, zs::move(attrib));
    }
    bool emplaceAttribDefault(const std::string& label, AttrVector&& attrib) {
      return _attribs[label].emplace(zs::move(attrib));
    }
    bool emplaceVisibilityKeyFrame(TimeCode tc, bool v) { return _visibility.emplace(tc, v); }
    bool emplaceVisibilityDefault(bool v) { return _visibility.emplace(v); }
    /*
    bool emplaceTranslationKeyFrame(TimeCode tc, const glm::vec3& v) {
      return _translation.emplace(tc, v);
    }
    bool emplaceScaleKeyFrame(TimeCode tc, const glm::vec3& v) { return _scale.emplace(tc, v); }
    bool emplaceRotationKeyFrame(TimeCode tc, const glm::vec3& v) {
      return _rotation.emplace(tc, v);
    }
    */
    bool emplaceTransformKeyFrame(TimeCode tc) { return _transform.emplace(tc, {}); }

    /// @brief key frame query

    // Weak<ZsPrimitive> getPrimKeyFrame(TimeCode tc) { return _prims.getByTimeCode(tc); }
    Weak<AttrVector> getAttribKeyFrame(const std::string& label, TimeCode tc) const {
      // return const_cast<PrimKeyFrames*>(this)->_attribs[label].getByTimeCode(tc);
      return _attribs.at(label).getByTimeCode(tc);
    }
    Weak<bool> getVisibilityKeyFrame(TimeCode tc) { return _visibility.getByTimeCode(tc); }

    /// @brief timecodes query
    auto getAttribTimeCodes(const std::string& label) const {
      return _attribs.at(label).getTimeCodes();
    }

    /// @brief precise timecode query

    // Weak<ZsPrimitive> getPrim(TimeCode tc) { return getPrimKeyFrame(tc); }
    Weak<AttrVector> getAttrib(const std::string& label, TimeCode tc) const {
      return getAttribKeyFrame(label, tc);
    }
    bool getVisibility(TimeCode tc) {
      if (auto p = getVisibilityKeyFrame(tc).lock()) return *p;
      return false;
    }

    inline bool isTimeDependent() const noexcept;
    inline bool queryStartEndTimeCodes(TimeCode& start, TimeCode& end) const noexcept;
    bool hasAttrib(const std::string& label) const { return _attribs.contains(label); }

    int getNumPoints(TimeCode tc) const {
      auto ptsPtr = getAttribKeyFrame(KEYFRAME_ATTRIB_POS_LABEL, tc);
      if (ptsPtr.expired()) return 0;
      if (auto p = ptsPtr.lock()) return p->size();
      return 0;
    }
    int getNumVerts(TimeCode tc) const {
      auto vertPtr = getAttribKeyFrame(KEYFRAME_ATTRIB_FACE_INDEX_LABEL, tc);
      if (vertPtr.expired()) return 0;
      if (auto p = vertPtr.lock()) return p->size();
      return 0;
    }
    int getNumFaces(TimeCode tc) const {
      auto facePtr = getAttribKeyFrame(KEYFRAME_ATTRIB_FACE_LABEL, tc);
      if (facePtr.expired()) return 0;
      if (auto p = facePtr.lock()) return p->size();
      return 0;
    }

    void setSkelAnimTimeCodeInterval(TimeCode st, TimeCode ed) noexcept {
      _skelStartTimeCode = st;
      _skelEndTimeCode = ed;
    }
    auto getSkelAnimTimeCodeInterval() const noexcept {
      if (hasSkelAnim())
        return zs::make_tuple(*_skelStartTimeCode, *_skelEndTimeCode);
      else
        return zs::make_tuple(g_default_timecode(), g_default_timecode());
    }
    bool hasSkelAnim() const noexcept { return _skelStartTimeCode && _skelEndTimeCode; }

    void setGlobalTimeCodes(const std::vector<TimeCode>& tcs) {
      _globalTimeCodes.clear();
      for (auto tc : tcs) _globalTimeCodes.emplace_back(tc);
      if (hasSkelAnim()) {
        if (_globalTimeCodes.size() == 0) {
          _globalTimeCodes.push_front(*_skelStartTimeCode);
          _globalTimeCodes.push_back(*_skelEndTimeCode);
        } else {
          if (*_skelStartTimeCode + detail::deduce_numeric_epsilon<float>() < _globalTimeCodes[0])
            _globalTimeCodes.push_front(*_skelStartTimeCode);
          if (*_skelEndTimeCode > _globalTimeCodes.back() + detail::deduce_numeric_epsilon<float>())
            _globalTimeCodes.push_back(*_skelEndTimeCode);
        }
      }
    }

    const auto& refAttribsKeyFrames() const noexcept { return _attribs; }
    const auto& refVisibilityKeyFrames() const noexcept { return _visibility; }
    const auto& refTransformKeyFrames() const noexcept { return _transform; }
    const auto& refGlobalTimeCodes() const noexcept { return _globalTimeCodes; }

    // protected:
    struct PlaceHolder {};

    /// @brief this overrides ths attrib in the corresponding attrib in prim sequence
    std::map<std::string, KeyFrames<AttrVector>> _attribs;  // "pos", "topo", "uv", "nrm", "clr"
    KeyFrames<bool> _visibility;
    KeyFrames<PlaceHolder> _transform;

    std::deque<TimeCode> _globalTimeCodes{};

    std::optional<TimeCode> _skelStartTimeCode, _skelEndTimeCode;
  };

#if ZS_ENABLE_SERIALIZATION
  template <typename S> void serialize(S& s, PrimKeyFrames::PlaceHolder& primKeyframes) {}
  template <typename S> void serialize(S& s, PrimKeyFrames& primKeyframes) {
    s.ext(primKeyframes._attribs, bitsery::ext::StdMap{g_max_serialization_size_limit},
          [](S& s, std::string& key, KeyFrames<AttrVector>& attrib) {
            s.text1b(key, g_max_serialization_size_limit);
            serialize(s, attrib);
          });
    serialize(s, primKeyframes._visibility);
    serialize(s, primKeyframes._transform);
  }
#endif

  ///
  /// Geometry::Primitive
  /// primitive container (mesh, sdf, analytic levelset, curve, etc.)
  /// @note actually inherent types are better rendered through less indirections

  /// visitees
  struct PolyPrimContainer;
  struct TriPrimContainer;
  struct LinePrimContainer;
  struct PointPrimContainer;
  struct SdfPrimContainer;
  struct PackPrimContainer;
  struct AnalyticPrimContainer;

  struct CameraPrimContainer;
  struct LightPrimContainer;

  /// visitors
  template <typename T> struct VisitorConceptBase {
    virtual void visit(T&) = 0;
  };
  template <typename... Ts> struct VisitorConcept : VisitorConceptBase<Ts>... {
    virtual ~VisitorConcept() = default;
  };
  using NativePrimType = type_seq<PolyPrimContainer, TriPrimContainer, LinePrimContainer,
                                  PointPrimContainer, SdfPrimContainer, PackPrimContainer,
                                  AnalyticPrimContainer, CameraPrimContainer, LightPrimContainer>;
  // PrimContainerVisitor
  using PrimContainerVisitor = assemble_t<VisitorConcept, NativePrimType>;

  enum prim_type_e : PrimTypeIndex {
    Poly_ = 0,
    Tri_,
    Line_,
    Point_,
    Sdf_,
    Pack_,
    Analytic_,
    Camera_,
    Light_
  };

  /// visitee concept
  struct ZS_WORLD_EXPORT PrimContainerConcept {
    virtual ~PrimContainerConcept() = default;
    virtual void accept(PrimContainerVisitor& visitor) = 0;

    /// additional interfaces
    virtual bool isPoly() const { return false; }
    virtual bool isTri() const { return false; }
    virtual bool isLine() const { return false; }
    virtual bool isPoint() const { return false; }
    virtual bool isMesh() const { return isPoint() || isLine() || isTri() || isPoly(); }
    virtual bool isAnalytic() const { return false; }
    virtual bool isSdf() const { return false; }
    virtual bool isPack() const { return false; }
    virtual bool isCamera() const { return false; }
    virtual bool isLight() const { return false; }
  };
  // @ref visitors/ObjectVisitor.hpp
  /// visitee crtp helper
  template <typename PrimContainerT> constexpr prim_type_e get_prim_container_type_index() noexcept;
  template <typename Derived, typename Parent = PrimContainerConcept> struct PrimContainerInterface
      : virtual Parent {
    void accept(PrimContainerVisitor& v) override {
      static_cast<VisitorConceptBase<Derived>&>(v).visit(static_cast<Derived&>(*this));
    }
  };
  template <typename Derived, typename... Parents>
  struct PrimContainerInterface<Derived, type_seq<Parents...>> : virtual Parents... {
    void accept(PrimContainerVisitor& v) override {
      static_cast<VisitorConceptBase<Derived>&>(v).visit(static_cast<Derived&>(*this));
    }
  };

  ///
  struct ZS_WORLD_EXPORT CameraPrimContainer : PrimContainerInterface<CameraPrimContainer> {
    bool isCamera() const override {
      return true;
    }

    ;
  };

  template <> constexpr prim_type_e get_prim_container_type_index<CameraPrimContainer>() noexcept {
    return prim_type_e::Camera_;
  }
  struct ZS_WORLD_EXPORT LightPrimContainer : PrimContainerInterface<LightPrimContainer> {
    bool isLight() const override {
      return true;
    }

    ;
  };

  template <> constexpr prim_type_e get_prim_container_type_index<LightPrimContainer>() noexcept {
    return prim_type_e::Light_;
  }
  struct ZS_WORLD_EXPORT PackPrimContainer : PrimContainerInterface<PackPrimContainer> {
    bool isPack() const override { return true; }

    void resize(size_t size) { _packedPrims.resize(size); }
    auto& packedPrims() noexcept { return _packedPrims; }
    const auto& packedPrims() const noexcept { return _packedPrims; }

    std::vector<PackGeometry> _packedPrims;
  };

  template <> constexpr prim_type_e get_prim_container_type_index<PackPrimContainer>() noexcept {
    return prim_type_e::Pack_;
  }
  // curve, analytic levelset
  struct ZS_WORLD_EXPORT AnalyticPrimContainer : PrimContainerInterface<AnalyticPrimContainer> {
    bool isAnalytic() const override { return true; }
  };

  template <>
  constexpr prim_type_e get_prim_container_type_index<AnalyticPrimContainer>() noexcept {
    return prim_type_e::Analytic_;
  }
  struct ZS_WORLD_EXPORT SdfPrimContainer : PrimContainerInterface<SdfPrimContainer> {
    bool isSdf() const override { return true; }
  };

  template <> constexpr prim_type_e get_prim_container_type_index<SdfPrimContainer>() noexcept {
    return prim_type_e::Sdf_;
  }
  // e.g. Cube/Sphere is both analytic and sdf
  struct ZS_WORLD_EXPORT MeshPrimContainer : PrimContainerConcept {
    bool isMesh() const override { return true; }

    void resize(size_t size) { _prims.resize(size); }
    auto& prims() noexcept { return _prims; }
    const auto& prims() const noexcept { return _prims; }

    AttrVector _prims;
  };
  struct ZS_WORLD_EXPORT PolyPrimContainer
      : PrimContainerInterface<PolyPrimContainer, MeshPrimContainer> {
    bool isPoly() const override { return true; }
  };

  template <> constexpr prim_type_e get_prim_container_type_index<PolyPrimContainer>() noexcept {
    return prim_type_e::Poly_;
  }
  struct ZS_WORLD_EXPORT TriPrimContainer
      : PrimContainerInterface<TriPrimContainer, MeshPrimContainer> {
    bool isTri() const override { return true; }
  };

  template <> constexpr prim_type_e get_prim_container_type_index<TriPrimContainer>() noexcept {
    return prim_type_e::Tri_;
  }
  struct ZS_WORLD_EXPORT LinePrimContainer
      : PrimContainerInterface<LinePrimContainer, MeshPrimContainer> {
    bool isLine() const override { return true; }
  };

  template <> constexpr prim_type_e get_prim_container_type_index<LinePrimContainer>() noexcept {
    return prim_type_e::Line_;
  }
  struct ZS_WORLD_EXPORT PointPrimContainer
      : PrimContainerInterface<PointPrimContainer, MeshPrimContainer> {
    bool isPoint() const override { return true; }
  };

  template <> constexpr prim_type_e get_prim_container_type_index<PointPrimContainer>() noexcept {
    return prim_type_e::Point_;
  }

  ///
  /// primitive detail
  ///
  template <typename T = PrimIndex> struct PrimIDGenerator {
    T nextId() noexcept {
      _mutex.lock();
      if (_recycled.size() > 0) {
        auto ret = _recycled.back();
        // _size.fetch_sub(1);
        _recycled.pop_back();
        _mutex.unlock();
        return ret;
      }
      _mutex.unlock();
      return _curId++;
    }
    /// @note user must ensure this is the distributed one
    void recycleId(T id) {
      _mutex.lock();
      _recycled.push_back(id);
      // _size.fetch_add(1);
      _mutex.unlock();
    }

    std::atomic<T> _curId{1};  //, _size{0};
    std::vector<T> _recycled{};
    zs::Mutex _mutex;
  };

  using ZsTriMesh = Mesh<float, 3, u32, 3>;
  using ZsLineMesh = Mesh<float, 3, u32, 2>;
  using ZsPointMesh = Mesh<float, 3, u32, 1>;

  struct ZS_WORLD_EXPORT PrimitiveId {
    PrimitiveId() noexcept;
    ~PrimitiveId() noexcept;
    PrimitiveId(PrimitiveId&& o) noexcept;
    PrimitiveId& operator=(PrimitiveId&& o) noexcept;
    PrimitiveId(const PrimitiveId& o) = delete;
    PrimitiveId& operator=(const PrimitiveId& o) = delete;

    operator PrimIndex&() { return _id; }
    operator const PrimIndex&() const { return _id; }

    PrimIndex _id;
  };

  struct ZS_WORLD_EXPORT PrimitiveMeta {
    PrimitiveMeta() noexcept = default;
    ~PrimitiveMeta();
    PrimitiveMeta(PrimitiveMeta&& o) noexcept = default;
    PrimitiveMeta& operator=(PrimitiveMeta&& o) noexcept = default;
    PrimitiveMeta(const PrimitiveMeta& o) = delete;
    PrimitiveMeta& operator=(const PrimitiveMeta& o) = delete;

    ZsVar& meta(const std::string& tag) noexcept { return _metas[tag]; }
    const ZsVar& meta(const std::string& tag) const noexcept { return _metas.at(tag); }

    std::map<std::string, ZsVar> _metas;  // mostly python-based data
  };

  struct ZS_WORLD_EXPORT PrimitiveBoundingSphere {
    glm::vec3 _center;  // world space center position of bounding sphere
    float _radius;      // world space radius of bounding sphere
    void init(const glm::vec3& a) noexcept;
    void init(const glm::vec3& a, const glm::vec3& b) noexcept;
    void merge(const glm::vec3& p) noexcept;
  };

  struct ZS_WORLD_EXPORT PrimitiveBoundingBox {
    glm::vec3 minPos;
    glm::vec3 maxPos;
    void init(const glm::vec3& pos) noexcept;
    void merge(const glm::vec3& pos) noexcept;
  };

  struct ZS_WORLD_EXPORT PrimitiveDetail {
    using DirtyFlag = u32;
    enum dirty_flag_e : DirtyFlag {
      dirty_Topo = 1 << 0,  // the above should cause a full rebuild
      mask_Topo = (dirty_Topo << 1) - 1,

      dirty_Pos = 1 << 1,  // the above should cause an update to GAS
      mask_Shape = (dirty_Topo << 1) - 1,

      dirty_UV = 1 << 2,
      dirty_Normal = 1 << 3,
      dirty_Tangent = 1 << 4,
      dirty_Color = 1 << 5,  // the above should cause an update to VkModel
      dirty_TextureId = 1 << 6,
      mask_Attrib = (dirty_TextureId << 1) - 1,

      dirty_Translation = 1 << 10,
      dirty_Rotation = 1 << 11,
      dirty_Scale = 1 << 12,
      dirty_Transform = 1 << 13,  // this affects transform cache
      mask_Transform = ((dirty_Transform << 1) - 1) ^ mask_Attrib,

      dirty_TimeCode = 1 << 16,

      dirty_All = ~(u32)0
    };

    bool isDirty(DirtyFlag f) const noexcept;
    void clearDirtyFlag() noexcept;
    void setDirty(DirtyFlag f) noexcept;
    void unsetDirty(DirtyFlag f) noexcept;

    bool isTopoDirty() const noexcept;
    bool isShapeDirty() const noexcept;
    bool isAttribDirty() const noexcept;
    bool isTransformDirty() const noexcept;
    bool isTimeCodeDirty() const noexcept;
    void clearTopoDirtyFlag() noexcept;
    void clearShapeDirtyFlag() noexcept;
    void clearAttribDirtyFlag() noexcept;
    void clearTransformDirtyFlag() noexcept;
    void clearTimeCodeDirtyFlag() noexcept;
#define ZS_DEF_DIRTY_FLAG(NAME)                                              \
  void set##NAME##Dirty() noexcept { setDirty(dirty_flag_e::dirty_##NAME); } \
  void unset##NAME##Dirty() noexcept { unsetDirty(dirty_flag_e::dirty_##NAME); }

    ZS_DEF_DIRTY_FLAG(Topo)
    ZS_DEF_DIRTY_FLAG(Pos)
    ZS_DEF_DIRTY_FLAG(UV)
    ZS_DEF_DIRTY_FLAG(Normal)
    ZS_DEF_DIRTY_FLAG(Tangent)
    ZS_DEF_DIRTY_FLAG(Color)
    ZS_DEF_DIRTY_FLAG(Transform)
    ZS_DEF_DIRTY_FLAG(TimeCode)

#undef ZS_DEF_DIRTY_FLAG

    PrimIndex id() const noexcept { return _id; }
    std::string& label() noexcept { return _label; }
    const std::string& label() const noexcept { return _label; }
    glm::mat4 toNativeCoordTransform() const;
    bool isYUpAxis() const noexcept { return _isYUpAxis; }
    bool& isYUpAxis() noexcept { return _isYUpAxis; }
    bool isRightHandedCoord() const noexcept { return _isRightHandedCoord; }
    bool& isRightHandedCoord() noexcept { return _isRightHandedCoord; }
    const PrimitiveBoundingBox& localBoundingBox() const noexcept { return _localBoundingBox; }
    PrimitiveBoundingBox& localBoundingBox() noexcept { return _localBoundingBox; }
    const PrimitiveBoundingBox& worldBoundingBox() const noexcept { return _worldBoundingBox; }
    PrimitiveBoundingBox& worldBoundingBox() noexcept { return _worldBoundingBox; }
    void initLocalBoundingBox() noexcept;
    void updateWorldBoundingBox(const glm::mat4& worldMat) noexcept;

    PrimKeyFrames& keyframes() noexcept { return _keyframes; }
    const PrimKeyFrames& keyframes() const noexcept { return _keyframes; }
    /// @note synchrounous resource
    Shared<ZsPrimitive>& visualMesh() noexcept { return _visualMesh; }
    const Shared<ZsPrimitive>& visualMesh() const noexcept { return _visualMesh; }

    auto& triMesh() noexcept { return _triMesh; }
    const auto& triMesh() const noexcept { return _triMesh; }
    auto& lineMesh() noexcept { return _lineMesh; }
    const auto& lineMesh() const noexcept { return _lineMesh; }
    auto& pointMesh() noexcept { return _pointMesh; }
    const auto& pointMesh() const noexcept { return _pointMesh; }

    auto& triBvh() noexcept { return _triBvh; }
    const auto& triBvh() const noexcept { return _triBvh; }
    auto& lineBvh() noexcept { return _lineBvh; }
    const auto& lineBvh() const noexcept { return _lineBvh; }
    auto& pointBvh() noexcept { return _pointBvh; }
    const auto& pointBvh() const noexcept { return _pointBvh; }

    auto& vkTriMesh() noexcept { return _vkTriMesh; }
    const auto& vkTriMesh() const noexcept { return _vkTriMesh; }
    auto& vkLineMesh() noexcept { return _vkLineMesh; }
    const auto& vkLineMesh() const noexcept { return _vkLineMesh; }
    auto& vkPointMesh() noexcept { return _vkPointMesh; }
    const auto& vkPointMesh() const noexcept { return _vkPointMesh; }

    void setAssetOrigin(asset_origin_e origin) noexcept { _assetOrigin = origin; }
    asset_origin_e getAssetOrigin() const noexcept { return _assetOrigin; }

    const std::string& texturePath() const noexcept { return _texturePath; }
    std::string& texturePath() noexcept { return _texturePath; }

    ZsVar& meta(const std::string& tag) noexcept { return _metas.meta(tag); }
    const ZsVar& meta(const std::string& tag) const noexcept { return _metas.meta(tag); }

    // usd
    void setUsdPrim(const ScenePrimConcept* prim);
    std::string_view getUsdSceneName() const noexcept;
    std::string_view getUsdPrimPath() const noexcept;

    //
    glm::mat4 getTransform(TimeCode tc) const;
    glm::mat4 getTransform() const { return getTransform(getCurrentTimeCode()); }
    void setTransform(TimeCode tc, const glm::mat4& m);
    void setTransform(const glm::mat4& m) { setTransform(getCurrentTimeCode(), m); }

    TimeCode getCurrentTimeCode() const noexcept { return _curTimeCode; }
    void setCurrentTimeCode(TimeCode tc) noexcept { _curTimeCode = tc; }
    TimeCode getCurrentTransformTimeCode() const noexcept { return _transformTimeCode; }
    void setCurrentTransformTimeCode(TimeCode tc) noexcept { _transformTimeCode = tc; }

    /// @brief check if mesh requires an update (pos, nrm, clr, skinning)
    bool meshRequireUpdate(TimeCode newTc) const;
    bool transformRequireUpdate(TimeCode newTc) const;

    auto& refProcessingFlag() noexcept { return _processingFlag; }
    const auto& refProcessingFlag() const noexcept { return _processingFlag; }

    auto& refTransformVarying() noexcept { return _transformVarying; }
    const auto& refTransformVarying() const noexcept { return _transformVarying; }

    auto& refIsOpaque() noexcept { return _isOpaque; }
    const auto& refIsOpaque() const noexcept { return _isOpaque; }

  protected:
    PrimitiveMeta _metas;
    std::string _label;
    glm::mat4 _transform{glm::mat4(1.f)};  // local transform
    bool _transformVarying{false};
    bool _isOpaque{true};

    volatile u32 _processingFlag{0};

    /// cached for faster visualization
    KeyFrames<ZsTriMesh> _cachedTriMesh;
    KeyFrames<ZsLineMesh> _cachedLineMesh;
    KeyFrames<ZsPointMesh> _cachedPointMesh;
    // VkModel should be made persistent

    PrimKeyFrames _keyframes;
    TimeCode _curTimeCode{g_default_timecode()}, _transformTimeCode{g_default_timecode()};
    PrimitiveBoundingBox _localBoundingBox;
    PrimitiveBoundingBox _worldBoundingBox;
    Shared<ZsPrimitive> _visualMesh;
    ZsTriMesh _triMesh;
    ZsLineMesh _lineMesh;
    ZsPointMesh _pointMesh;
    VkModel _vkTriMesh, _vkLineMesh, _vkPointMesh;
    LBvh<3> _triBvh, _lineBvh, _pointBvh;

    std::string _texturePath;

    ScenePrimHolder _usdPrim{};
    // std::string _zpcTransformSuffix;
    std::string _usdSceneName{""}, _usdPrimPath{""};

    DirtyFlag _dirtyFlag{dirty_All};

    PrimitiveId _id;
    asset_origin_e _assetOrigin{asset_origin_e::native};
    bool _isYUpAxis = true;  // Y or Z as up-axis
    bool _isRightHandedCoord
        = true;  // determine if the primitive is in the left-handed or right-handed coordinates
  };

  ///
  /// general primitive
  ///
  struct ZS_WORLD_EXPORT PrimitiveStorage {
    enum native_prim_e : PrimTypeIndex {
      Poly_ = 0,
      Tri_,
      Line_,
      Point_,
      Sdf_,
      Analytic_,
      Pack_,
      Camera_,
      Light_,
      // custom prim type id starts here
      Custom_ = 100
    };
    /// @note signals emitted by ZsPrimitive state machine, upon state transitioning triggered
    /// by events
    Signal<void(std::vector<native_prim_e>)> _attribChange, _topoChange;

    // TODO: for further state machine
    enum formulation_state_e {
      PolyMesh_ = 0,
      SimpleMesh_,
      VisualMesh_,
      ZsModel_,
    };
    // modifiers
    void reset() {
      _groups.clear();
      _points.clear();
      _verts.clear();
      _localPrims.clear();
      _primTagIndex.clear();
      _globalPrimMapping.clear();
    }

    bool isEmpty() const { return _globalPrimMapping.size() == 0; }
    // lookup
    auto& points() noexcept { return _points; }
    auto& verts() noexcept { return _verts; }
    const auto& points() const noexcept { return _points; }
    const auto& verts() const noexcept { return _verts; }

    bool empty() const noexcept { return _points.size() == 0; }

    Shared<PrimContainerConcept>& localPrims(PrimTypeIndex id) { return _localPrims.at(id); }
    Shared<PrimContainerConcept>& localPrims(std::string_view tag) {
      return localPrims(_primTagIndex.at(std::string(tag)));
    }
    template <typename PrimContainerT> Shared<PrimContainerT> localPrims(PrimTypeIndex id) {
      return std::dynamic_pointer_cast<PrimContainerT>(_localPrims.at(id));
    }
    template <typename PrimContainerT> Shared<PrimContainerT> localPrims(std::string_view tag) {
      return localPrims<PrimContainerT>(_primTagIndex.at(std::string(tag)));
    }
    auto& globalPrims() noexcept { return _globalPrimMapping; }
    const auto& globalPrims() const noexcept { return _globalPrimMapping; }
    /// details lookup
    auto& details() noexcept { return _details; }
    const auto& details() const noexcept { return _details; }

    PrimKeyFrames& keyframes() noexcept { return details().keyframes(); }
    const PrimKeyFrames& keyframes() const noexcept { return details().keyframes(); }

    auto& label() noexcept { return _details.label(); }
    const auto& label() const noexcept { return _details.label(); }
    auto id() const noexcept { return _details.id(); }

    inline bool isSimpleMeshEstablished() const;
    Shared<ZsPrimitive>& visualMesh();
    ZsTriMesh& triMesh();

    void updatePrimFromKeyFrames(TimeCode tc);

    /// @brief compute the transformed position based upon the original pos (points())
    bool applySkinning(TimeCode tc);

#define ZS_DECLARE_LOCAL_PRIM(TYPE)                        \
  inline Shared<TYPE##PrimContainer> local##TYPE##Prims(); \
  inline Shared<const TYPE##PrimContainer> local##TYPE##Prims() const;
    ZS_DECLARE_LOCAL_PRIM(Poly)
    ZS_DECLARE_LOCAL_PRIM(Tri)
    ZS_DECLARE_LOCAL_PRIM(Line)
    ZS_DECLARE_LOCAL_PRIM(Point)
    ZS_DECLARE_LOCAL_PRIM(Sdf)
    ZS_DECLARE_LOCAL_PRIM(Analytic)
    ZS_DECLARE_LOCAL_PRIM(Pack)
    ZS_DECLARE_LOCAL_PRIM(Camera)
    ZS_DECLARE_LOCAL_PRIM(Light)
#undef ZS_DECLARE_LOCAL_PRIM

    std::vector<GeoGroup<std::set<i32>>> _groups;
    AttrVector _points;
    AttrVector _verts;
    std::map<PrimTypeIndex, Shared<PrimContainerConcept>> _localPrims;
    std::map<std::string, PrimTypeIndex> _primTagIndex;
    std::vector<zs::tuple<PrimTypeIndex, PrimIndex>> _globalPrimMapping;
    PrimitiveDetail _details;
  };

  /// @note general mesh: poly mesh
  /// @note simple mesh: point/line/tri, allow [verts], easy for simulations
  /// @note simple and general mesh only differs in prim representation, [verts] are shared
  /// @note visual mesh: zs::Mesh-like, store attribs on points
  struct ZS_WORLD_EXPORT ZsPrimitive final : PrimitiveStorage {
    ZsPrimitive() noexcept = default;
    ZsPrimitive(ZsPrimitive&&) noexcept = default;
    ZsPrimitive& operator=(ZsPrimitive&&) noexcept = default;
    enum state_e : u32 {
      invalid_ = 0,
      in_ram_,
      ram_to_disk_,
      on_disk_,
      dist_to_ram_,
    };

    /// @note this STEALs the reference to childPrim
    void appendChildPrimitve(ZsPrimitive* childPrim);
    inline bool removeChild(ZsPrimitive* p);
    inline Weak<ZsPrimitive> getChild(int i);
    inline Weak<ZsPrimitive> getChild(std::string_view label);
    inline Weak<ZsPrimitive> getChildByIdRecurse(PrimIndex id_);
    size_t numChildren() const noexcept { return _childs.size(); }
    inline std::vector<std::string> getChildLabels() const;

    bool queryStartEndTimeCodes(TimeCode& start, TimeCode& end) const noexcept;

    ZsPrimitive* getParent() noexcept { return _parent; }
    const ZsPrimitive* getParent() const noexcept { return _parent; }

    /// @note this path usually refers to other resources (e.g. usd),
    /// @note not necessarily the one composed from label hierarchy
    std::string_view getPath() const noexcept { return _path; }

    VkModel& vkTriMesh(VulkanContext& ctx);

    /// @note asynchrounous resources
    Future<Shared<ZsPrimitive>> visualMeshAsync(TimeCode tc);
    ZsPrimitive* queryVisualMesh(TimeCode tc);

    // Future<ZsTriMesh> triMeshAsync(TimeCode tc);
    Future<void> zsMeshAsync(TimeCode tc);
    ZsTriMesh* queryTriMesh(TimeCode tc);
    // Future<ZsLineMesh> lineMeshAsync();
    // Future<ZsPointMesh> linePointAsync();

    Future<void> vkTriMeshAsync(VulkanContext& ctx, TimeCode tc);
    VkModel* queryVkTriMesh(VulkanContext& ctx, TimeCode tc = g_default_timecode());
    // Future<VkModel> vkLineMeshAsync(VulkanContext& ctx);
    // Future<VkModel> vkPointMeshAsync(VulkanContext& ctx);

    auto& path() noexcept { return _path; }
    const auto& path() const noexcept { return _path; }

    inline glm::mat4 parentTransform() const noexcept;
    inline glm::mat4 parentTransform(TimeCode tc) const noexcept;
    glm::mat4 worldTransform(TimeCode tc) const noexcept {
      return parentTransform(tc) * details().getTransform(tc);
    }
    glm::mat4 worldTransform() const noexcept {
      auto tc = details().getCurrentTimeCode();
      return parentTransform(tc) * details().getTransform(tc);
    }
    glm::mat4 visualTransform() const noexcept {
      return details().toNativeCoordTransform() * worldTransform(details().getCurrentTimeCode());
    }
    glm::mat4 visualTransform(TimeCode tc) const noexcept {
      return details().toNativeCoordTransform() * worldTransform(tc);
    }
    void updateTransform(TimeCode tc) noexcept {
      if (details().refProcessingFlag() == 0)
        _visualTransform = visualTransform(tc);
      else
        _visualTransform = visualTransform(details().getCurrentTimeCode());
    }
    void updateTransform() noexcept {
      // fmt::print("name: {}, path: {}\n", details().label(), path());
      _visualTransform = visualTransform(details().getCurrentTimeCode());
    }
    const glm::mat4& currentTimeVisualTransform() const noexcept { return _visualTransform; }
    auto glm_to_zs_transform(const glm::mat4& trans) const noexcept {
      vec<float, 4, 4> ret;
      for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) ret(i, j) = trans[i][j];
      return ret;
    }
    auto zs_transform() const noexcept { return glm_to_zs_transform(worldTransform()); }

  protected:
    std::string _path{""};  // accumulated path
    ZsPrimitive* _parent{nullptr};
    /// @note although shared being used here, but never share-owned, mostly for observers
    std::vector<Shared<ZsPrimitive>> _childs;  // usually built first then moved to parent

    /// @brief async resources
    Future<Shared<ZsPrimitive>> _visualMeshAsync;
    Future<void> _zsMeshAsync;
    Future<void> _vkTriMeshAsync, _vkLineMeshAsync, _vkPointMeshAsync;
    glm::mat4 _visualTransform{glm::mat4(1.f)};

    // std::atomic<state_e> _status{invalid_};  // interaction states
  };

  ///
  /// definitions
  ///

  PrimHandle::operator PrimIndex() const noexcept {
    if (auto p = _primPtr.lock(); p) return p->details().id();
    return -1;
  }

  template <typename Value> void KeyFrames<Value>::updateSequence() {
    _orderedKeys.clear();
    _orderedFrames.clear();
    _orderedKeys.reserve(_keyframes.size());
    _orderedFrames.reserve(_keyframes.size());
    for (auto&& [tc, val] : _keyframes) {
      _orderedKeys.push_back(tc);
      _orderedFrames.push_back(val);
    }
    _dirty = false;
  }

  template <typename Value> Weak<Value> KeyFrames<Value>::getSegmentFrame(int segmentNo) const {
    if (_keyframes.size() == 0) {
      if (hasDefaultValue())
        return _defaultValue;
      else
        return {};
    }
    if (_dirty) const_cast<KeyFrames*>(this)->updateSequence();
    if (segmentNo < 0)
      return _orderedFrames[0];
    else if (segmentNo >= _keyframes.size())
      return _orderedFrames[_keyframes.size() - 1];
    return _orderedFrames[segmentNo];
  }
  template <typename Value> const std::vector<TimeCode>& KeyFrames<Value>::getTimeCodes() const {
    if (_dirty) const_cast<KeyFrames*>(this)->updateSequence();
    return _orderedKeys;
  }
  template <typename Value> TimeCode KeyFrames<Value>::getSegmentTimeCode(int segmentNo) const {
    if (_keyframes.size() == 0) return g_default_timecode();
    if (_dirty) const_cast<KeyFrames*>(this)->updateSequence();
    if (segmentNo < 0)
      return _orderedKeys[0];
    else if (segmentNo >= _keyframes.size())
      return _orderedKeys[_keyframes.size() - 1];
    return _orderedKeys[segmentNo];
  }
  template <typename Value> int KeyFrames<Value>::getTimeCodeSegmentIndex(TimeCode tc) const {
    if (_dirty) const_cast<KeyFrames*>(this)->updateSequence();
    int left = 0, right = _keyframes.size();
    while (left < right) {
      auto mid = left + (right - left) / 2;
      if (_orderedKeys[mid] > tc)
        right = mid;
      else
        left = mid + 1;
    }
    if (left < _keyframes.size()) {
      if (_orderedKeys[left] > tc) left--;
    } else
      left = _keyframes.size() - 1;
    return left;
  }
  /// @note TBD
  template <typename Value> template <typename Float, typename T, typename Ret>
  Ret KeyFrames<Value>::getInterpolation(Float tc) {
    static_assert(is_same_v<Ret, Value>, "assume the return type is also the same frame type!");
    auto segmentNo = getTimeCodeSegmentIndex(tc);
    if (segmentNo < 0) {
      /// @note may indicate a time-invariant case
      if (_keyframes.size())
        return *getSegmentFrame(0).lock();
      else if (hasDefaultValue())
        return *_defaultValue;
      else
        return {};
    } else if (segmentNo + 1 == _keyframes.size())
      return *getSegmentFrame(segmentNo).lock();
    auto nextSegmentNo = segmentNo + 1;
    auto st = getSegmentTimeCode(segmentNo);
    auto ed = getSegmentTimeCode(nextSegmentNo);
    auto len = ed - st;
    return *getSegmentFrame(segmentNo).lock() * static_cast<Float>((ed - tc) / len)
           + *getSegmentFrame(nextSegmentNo).lock() * static_cast<Float>((tc - st) / len);
  }

  bool PrimKeyFrames::isTimeDependent() const noexcept {
    bool isTimeVarying = false;
    if (!isTimeVarying && hasSkelAnim()) isTimeVarying = true;
    for (const auto& [_, keyframes] : _attribs)
      if (keyframes.isTimeDependent()) {
        isTimeVarying = true;
        break;
      }
    if (!isTimeVarying && _visibility.isTimeDependent()) isTimeVarying = true;
    if (!isTimeVarying && _transform.isTimeDependent()) isTimeVarying = true;
    return isTimeVarying;
  }

  bool PrimKeyFrames::queryStartEndTimeCodes(TimeCode& start, TimeCode& end) const noexcept {
    if (_globalTimeCodes.size() == 0) {
      start = end = g_default_timecode();
      return false;
    } else {
      start = _globalTimeCodes[0];
      end = _globalTimeCodes.back();
      return true;
    }
  }

  void AttrVector::printDbg(std::string_view msg) const {
    fmt::print("{}\n", msg);
    auto props = getProperties();
    fmt::print("size: {}, num props: {}\n", size(), props.size());
    for (int i = 0; i < props.size(); ++i) {
      const auto& prop = props[i];
      fmt::print("{}-th prop ({}, {}, {})\n", i, prop.name, prop.numChannels,
                 getPropertyOffset(prop.name));
    }
  }
  template <typename T> void AttrVector::printAttrib(const SmallString& prop, wrapt<T>) {
    assert(attr32().memspace() == memsrc_e::host);

    if (hasProperty(prop)) {
      auto v = view<execspace_e::host>(attr32());
      auto propOffset = getPropertyOffset(prop);
      auto nChns = getPropertySize(prop);
      for (PrimIndex i = 0; i < size(); ++i) {
        fmt::print("[{}]: <", i);
        for (int d = 0; d < nChns; ++d)
          fmt::print("{} {}", d ? "," : "", v(propOffset + d, i, wrapt<T>{}));
        fmt::print(" >\t");
      }
      fmt::print("\n");
    } else {
      fmt::print("property [{}] does not exist.\n", prop);
    }
  }

#define ZS_DEFINE_LOCAL_PRIM(TYPE)                                                 \
  Shared<TYPE##PrimContainer> PrimitiveStorage::local##TYPE##Prims() {             \
    Shared<TYPE##PrimContainer> ret;                                               \
    if (auto it = _localPrims.find(TYPE##_); it == _localPrims.end()) {            \
      ret = std::make_shared<TYPE##PrimContainer>();                               \
      _localPrims.emplace(TYPE##_, ret);                                           \
    } else                                                                         \
      ret = std::dynamic_pointer_cast<TYPE##PrimContainer>((*it).second);          \
    return ret;                                                                    \
  }                                                                                \
  Shared<const TYPE##PrimContainer> PrimitiveStorage::local##TYPE##Prims() const { \
    Shared<const TYPE##PrimContainer> ret;                                         \
    if (auto it = _localPrims.find(TYPE##_); it == _localPrims.end()) {            \
      return const_cast<PrimitiveStorage*>(this)->local##TYPE##Prims();            \
    } else                                                                         \
      ret = std::dynamic_pointer_cast<TYPE##PrimContainer>((*it).second);          \
    return ret;                                                                    \
  }
  ZS_DEFINE_LOCAL_PRIM(Poly)
  ZS_DEFINE_LOCAL_PRIM(Tri)
  ZS_DEFINE_LOCAL_PRIM(Line)
  ZS_DEFINE_LOCAL_PRIM(Point)
  ZS_DEFINE_LOCAL_PRIM(Sdf)
  ZS_DEFINE_LOCAL_PRIM(Analytic)
  ZS_DEFINE_LOCAL_PRIM(Pack)
  ZS_DEFINE_LOCAL_PRIM(Camera)
  ZS_DEFINE_LOCAL_PRIM(Light)
#undef ZS_DEFINE_LOCAL_PRIM

  bool PrimitiveStorage::isSimpleMeshEstablished() const {
    return (localPointPrims()->prims().size() > 0
            && localPointPrims()->prims().hasProperty(TO_POLY_ID_TAG))
           || (localLinePrims()->prims().size() > 0
               && localLinePrims()->prims().hasProperty(TO_POLY_ID_TAG))
           || (localTriPrims()->prims().size() > 0
               && localTriPrims()->prims().hasProperty(TO_POLY_ID_TAG));
  }

  Weak<ZsPrimitive> ZsPrimitive::getChild(int i) {
    assert(i >= 0 && i < _childs.size());
    return _childs[i];
  }

  Weak<ZsPrimitive> ZsPrimitive::getChild(std::string_view label) {
    for (int i = 0; i < numChildren(); ++i)
      if (_childs[i]->label() == label) return _childs[i];
    return {};
  }
  Weak<ZsPrimitive> ZsPrimitive::getChildByIdRecurse(PrimIndex id_) {
    for (int i = 0; i < numChildren(); ++i)
      if (_childs[i]->id() == id_) return _childs[i];
    // recurse
    for (int i = 0; i < numChildren(); ++i)
      if (auto ret = _childs[i]->getChildByIdRecurse(id_); ret.lock()) return ret;
    return {};
  }
  std::vector<std::string> ZsPrimitive::getChildLabels() const {
    std::vector<std::string> ret;
    ret.reserve(numChildren());
    for (int i = 0; i < numChildren(); ++i) ret.push_back(_childs[i]->label());
    return ret;
  }
  bool ZsPrimitive::removeChild(ZsPrimitive* pr) {
    for (auto it = _childs.begin(); it != _childs.end(); ++it) {
      if ((*it).get() == pr) {
        _childs.erase(it);
        return true;
      }
    }
    return false;
  }

  glm::mat4 ZsPrimitive::parentTransform() const noexcept {
    if (_parent)
      return _parent->worldTransform(details().getCurrentTimeCode());
    else
      return glm::mat4(1.f);
  }
  glm::mat4 ZsPrimitive::parentTransform(TimeCode tc) const noexcept {
    if (_parent)
      return _parent->worldTransform(tc);
    else
      return glm::mat4(1.f);
  }

}  // namespace zs
