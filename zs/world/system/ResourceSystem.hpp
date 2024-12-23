#pragma once

#include <deque>
#include <map>
#include <set>
#include <string_view>

#include "world/WorldExport.hpp"
#include "world/core/Archive.hpp"
#include "world/core/ConsoleHelper.hpp"
//
#include "world/core/Signal.hpp"
//
#include "zensim/ui/Widget.hpp"
//
#include "world/scene/SceneContext.hpp"
//
#include "zensim/ZpcImplPattern.hpp"
#include "zensim/io/Filesystem.hpp"
#include "zensim/types/ImplPattern.hpp"
#include "zensim/types/SourceLocation.hpp"
#include "zensim/vulkan/VkShader.hpp"
#include "zensim/vulkan/VkTexture.hpp"
#if ZS_ENABLE_USD
#  include "world/usd/USDScenePrim.hpp"
#endif

namespace zs {

  enum struct icon_e { play = 0, pause, stop, simulate, speed_up, file, folder, num_icons };

  constexpr const char *g_defaultWidgetLabelAssetBrowser = "asset_manager";
  constexpr const char *g_defaultWidgetLabelSequencer = "sequencer";
  constexpr const char *g_defaultWidgetLabelGraphEditor = "node_graph";
  constexpr const char *g_defaultWidgetLabelScene = "scene";
  constexpr const char *g_defaultWidgetLabelSceneHierarchy = "scene_hierarchy";
  constexpr const char *g_defaultWidgetLabelDetail = "details";

  constexpr const char *g_textEditorFile = "___editor_content.py";
  constexpr const char *g_textEditorLabel = "___text_editor";

  constexpr const char *g_defaultUsdFile = "___default_usd_scene.usda";
  constexpr const char *g_defaultUsdLabel = "___usd_default";

  constexpr const char *g_logFile = "___editor_log.txt";
  constexpr const char *g_logLabel = "___log_editor";

  constexpr const char *g_defaultSceneFile = "___default_scene.bin";
  constexpr const char *g_defaultSceneLabel = "___default_scene";

  inline std::string get_scene_prim_widget_label(std::string_view sceneLabel,
                                                 std::string_view primLabel) {
    return fmt::format("scene_ctx_{}:prim_{}", sceneLabel, primLabel);
  }

  struct ResourceSystem {
  private:
    ResourceSystem();
    ~ResourceSystem();

    /// image
    std::array<vk::DescriptorSet, static_cast<size_t>(icon_e::num_icons)> _iconDescriptorSets;
    std::array<VkTexture, static_cast<size_t>(icon_e::num_icons)> _iconTextures;
    mutable std::set<void *> _registeredSets;  // for imgui image display

    /// texture
    static_assert(is_same_v<image_handle_t, int>, "...");
    struct TextureEntry {
      void reset() {
        bindlessId = -1;
        tex.reset();
      }
      image_handle_t bindlessId;
      VkTexture tex;
    };
    std::map<std::string, vk::DescriptorSet> _textureDescriptorSets;
    std::map<std::string, TextureEntry> _textures;
    TextureEntry _missingTexture;
    vk::DescriptorSet _missingTextureDescriptorSet;
    static const std::string _missingTextureRelPath;

    Signal<void(const std::vector<std::string> &)> _textureModified;
    Signal<void(const std::vector<std::string> &)> _textureAdded;
    Signal<void(const std::vector<std::string> &)> _textureRemoved;

    /// shader
    std::map<std::string, Owner<ShaderModule>> _shaders;

    /// scripts
    std::map<std::string, Archive, std::less<void>> _scripts{};
    Signal<void(const std::vector<std::string> &)> _scriptFilesChanged;
    Signal<void(const std::vector<std::string> &)> _scriptFilesOpened;
    Signal<void(const std::vector<std::string> &)> _scriptFilesClosed;

    /// usd
    struct UsdMeta {
      UsdMeta &operator=(std::string_view label) {
        _filename = label;
        return *this;
      }
      std::string_view getFileName() const noexcept { return _filename; }

      void markDirty() noexcept { _dirty = true; }
      void markClean() noexcept { _dirty = false; }
      bool isDirty() const noexcept { return _dirty; }

      std::string _filename;
      bool _dirty{false};
    };
    std::map<std::string, UsdMeta, std::less<void>> _usdEntries;  // keys to access usd scene
#if ZS_ENABLE_USD
    SceneObserver _usdObserver;
#endif
    Signal<void(const std::vector<std::string> &)> _usdFilesChanged;
    Signal<void(const std::vector<std::string> &)> _usdFilesOpened;
    Signal<void(const std::vector<std::string> &)> _usdFilesClosed;

    /// widgets
    EmptyWidget _emptyWidget;
    std::map<std::string, Shared<WidgetComponentConcept>, std::less<>> _widgets{};

    /// contexts
    PrimIDGenerator<> _primIdGenerator;
    PrimIDGenerator<u32> _widgetIdGenerator;

    // scene context
    std::map<std::string, SceneContext> _sceneContexts;
    Signal<void(const std::vector<std::string> &)> _sceneContextPrimitiveChanged;
    Signal<void(const std::vector<std::string> &)> _sceneContextPrimitiveCreation;
    Signal<void(const std::vector<std::string> &)> _sceneContextPrimitiveRemoval;

    enum collection_entity_type_e { collection_prim = 0 };
    struct PrimCollectionEntry {
      ZsPrimitive *prim{nullptr};
    };
    std::vector<PrimCollectionEntry> _primCollection;

    enum collection_entity_op_e { collection_add = 0, collection_remove };
    struct PrimCollectionOp {
      ZsPrimitive *prim{nullptr};
      collection_entity_op_e op{collection_add};
    };
    Signal<void(const std::vector<PrimCollectionOp> &)> _primCollectionChanged;

    /// log
    std::string _capturedInfoStr, _capturedErrStr;
    u32 _logEntryLimit{1024};
    std::deque<ConsoleRecord> _logEntries;

    /// cache
    std::string _rootCachePath;

    /// status
    std::atomic<u32> _primInFlight{0};
    Signal<void(i32)> _numPrimInFlightChanged;

  public:
    /// cache
    std::string_view get_cache_path() noexcept { return instance()._rootCachePath; }

    /// signals
    static auto &onTexturesChanged() { return instance()._textureModified; }
    static auto &onTexturesAdded() { return instance()._textureAdded; }
    static auto &onTexturesRemoved() { return instance()._textureRemoved; }

    static auto &onScriptFilesChanged() { return instance()._scriptFilesChanged; }
    static auto &onScriptFilesOpened() { return instance()._scriptFilesOpened; }
    static auto &onScriptFilesClosed() { return instance()._scriptFilesClosed; }

    static auto &onUsdFilesChanged() { return instance()._usdFilesChanged; }
    static auto &onUsdFilesOpened() { return instance()._usdFilesOpened; }
    static auto &onUsdFilesClosed() { return instance()._usdFilesClosed; }

    static auto &onSceneContextPrimitiveChanged() {
      return instance()._sceneContextPrimitiveChanged;
    }
    static auto &onSceneContextPrimitiveCreation() {
      return instance()._sceneContextPrimitiveCreation;
    }
    static auto &onSceneContextPrimitiveRemoval() {
      return instance()._sceneContextPrimitiveRemoval;
    }

    static auto &onPrimInFlightChanged() { return instance()._numPrimInFlightChanged; }

    static auto &onPrimCollectionChanged() { return instance()._primCollectionChanged; }

    /// state
    // for scene visualization
    void set_inflight_prim_cnt(i32 cnt) { _primInFlight.store(cnt); }
    void inc_inflight_prim_cnt() {
      /// @note avoid emitting unuseful signals
      if (_primInFlight.fetch_add(1) == 0) onPrimInFlightChanged().emit(1);
    }
    void dec_inflight_prim_cnt() {
      /// @note avoid emitting unuseful signals
      if (_primInFlight.fetch_sub(1) == 1) onPrimInFlightChanged().emit(-1);
    }
    auto get_inflight_prim_cnt() const { return _primInFlight.load(); }
    bool vis_prims_ready() const {
      auto v = _primInFlight.load();
      return v == 0;
    }

    /// id
    static u32 next_widget_id() { return instance()._widgetIdGenerator.nextId(); }
    static void recycle_widget_id(u32 id) { return instance()._widgetIdGenerator.recycleId(id); }
    static PrimIndex next_prim_id() { return instance()._primIdGenerator.nextId(); }
    static void recycle_prim_id(PrimIndex id) { return instance()._primIdGenerator.recycleId(id); }

    /// scene context
    static SceneContext &get_scene_context(std::string_view label = g_defaultSceneLabel) {
      return instance()._sceneContexts.at(std::string(label));
    }
    static SceneContext *get_scene_context_ptr(std::string_view label = g_defaultSceneLabel) {
      if (auto it = instance()._sceneContexts.find(std::string(label));
          it != instance()._sceneContexts.end())
        return &(*it).second;
      return nullptr;
    }
    static std::vector<std::string> get_scene_context_labels() noexcept {
      const auto &scenes = instance()._sceneContexts;
      std::vector<std::string> ret;
      ret.reserve(scenes.size());
      for (const auto &[label, sceneCtx] : scenes) ret.emplace_back(label);
      return ret;
    }
    static Weak<ZsPrimitive> get_scene_context_primitive(std::string_view sceneLabel,
                                                         std::string_view primLabel) {
      return get_scene_context(sceneLabel).getPrimitive(primLabel);
    }
    static std::vector<std::string> get_scene_context_primitive_labels(
        std::string_view sceneLabel) {
      return get_scene_context(sceneLabel).getPrimitiveLabels();
    }
    static bool register_scene_primitive(std::string_view sceneLabel, std::string_view primLabel,
                                         Shared<ZsPrimitive> prim) {
      auto ret = get_scene_context(sceneLabel).registerPrimitive(primLabel, prim);
      onSceneContextPrimitiveCreation().emit({std::string(primLabel)});
      return ret;
    }

    ///
    static ZS_WORLD_EXPORT ResourceSystem &instance();
    static ZS_WORLD_EXPORT void initialize();

    /// path query
    static ZS_WORLD_EXPORT std::string root_directory();
    static ZS_WORLD_EXPORT std::string cache_directory();

    /// image
    static ZS_WORLD_EXPORT void register_image_for_gui(void *s);
    static ZS_WORLD_EXPORT bool image_avail_for_gui(void *s);
    static ZS_WORLD_EXPORT vk::DescriptorSet &get_icon_descr_set(icon_e id);
    static ZS_WORLD_EXPORT VkTexture &get_icon_texture(icon_e id);
    static ZS_WORLD_EXPORT bool update_icon_texture(VulkanContext &ctx, icon_e id,
                                                    std::string file);

    /// texture
    static ZS_WORLD_EXPORT const vk::DescriptorSet &get_texture_descriptor_set(const std::string &);
    static ZS_WORLD_EXPORT const vk::DescriptorSet &get_missing_texture_descriptor_set();
    static ZS_WORLD_EXPORT VkTexture &get_texture(const std::string &label);
    static ZS_WORLD_EXPORT bool has_texture_loaded(const std::string &label);
    static ZS_WORLD_EXPORT bool load_texture(VulkanContext &ctx, const std::string &label,
                                             const ShaderModule &bind_shader, int layout_set = 0,
                                             int binding = 0);
    static ZS_WORLD_EXPORT bool load_missing_texture(VulkanContext &ctx,
                                                     const ShaderModule &bind_shader,
                                                     int layout = 0, int binding = 0);

    /// shader
    static ZS_WORLD_EXPORT bool load_shader(VulkanContext &ctx, const std::string &label,
                                            vk::ShaderStageFlagBits stage, std::string_view code,
                                            const source_location &loc
                                            = source_location::current());
    static ZS_WORLD_EXPORT ShaderModule &get_shader(const std::string &label);

    /// script
    static ZS_WORLD_EXPORT bool load_script(std::string_view filename = g_textEditorFile,
                                            std::string_view label = g_textEditorLabel);
    static void update_script(std::string_view label, std::string_view text) {
      auto &scripts = instance()._scripts;
      auto it = scripts.find(label);
      if (it != scripts.end()) {
        (*it).second.setString(text);
      }
    }
    static bool has_script(std::string_view label = g_textEditorLabel) {
      return instance()._scripts.find(label) != instance()._scripts.end();
    }
    static bool save_script(std::string_view label = g_textEditorLabel) {
      auto &scripts = instance()._scripts;
      auto it = scripts.find(label);
      if (it != scripts.end()) {
        auto &archive = (*it).second;
        if (archive.modified()) {
          archive.saveToFileAscii();
          archive.setUnchanged();
          return true;
        }
      }
      return false;
    }
    static bool script_modified(std::string_view label = g_textEditorLabel) {
      auto &scripts = instance()._scripts;
      auto it = scripts.find(label);
      if (it != scripts.end()) {
        return (*it).second.modified();
      }
      return false;
    }
    static bool close_script(std::string_view label = g_textEditorLabel) {
      auto &scripts = instance()._scripts;
      auto it = scripts.find(label);
      if (it != scripts.end()) {
        scripts.erase(it);
        onScriptFilesClosed().emit({std::string{label}});
        return true;
      }
      return false;
    }
    static void mark_script_clean(std::string_view label = g_textEditorLabel) {
      auto &scripts = instance()._scripts;
      auto it = scripts.find(label);
      if (it != scripts.end()) {
        (*it).second.setUnchanged();
      }
    }
    static void mark_script_dirty(std::string_view label = g_textEditorLabel) {
      auto &scripts = instance()._scripts;
      auto it = scripts.find(label);
      if (it != scripts.end()) {
        (*it).second.setChanged();
      }
    }
    static std::string get_script(const std::string &label = g_textEditorLabel) {
      return instance()._scripts.at(label).getBuffer();
    }
    static std::string_view get_script_filename(const std::string &label = g_textEditorLabel) {
      return instance()._scripts.at(label).fileName();
    }
    // static auto &ref_scripts() { return instance()._scripts; }

    static std::vector<std::string> get_script_labels() noexcept {
      const auto &scripts = instance()._scripts;
      std::vector<std::string> ret;
      ret.reserve(scripts.size());
      for (const auto &[label, archive] : scripts) ret.emplace_back(label);
      return ret;
    }

    /// usd assets
#if ZS_ENABLE_USD
    static ZS_WORLD_EXPORT SceneDescConcept *load_usd(std::string_view filename = g_defaultUsdFile,
                                                      std::string_view label = g_defaultUsdLabel);
    static ZS_WORLD_EXPORT SceneDescConcept *get_usd(std::string_view label = g_defaultUsdLabel);
    static ZS_WORLD_EXPORT bool save_usd_as(std::string_view filename = g_defaultUsdFile,
                                            std::string_view label = g_defaultUsdLabel);
    static ZS_WORLD_EXPORT bool save_usd(std::string_view label = g_defaultUsdLabel);
    static bool usd_modified(std::string_view label = g_defaultUsdLabel) {
      auto &entries = instance()._usdEntries;
      auto it = entries.find(label);
      if (it != entries.end()) {
        return (*it).second.isDirty();
      }
      return false;
    }
    static void mark_usd_clean(std::string_view label = g_defaultUsdLabel) {
      auto &entries = instance()._usdEntries;
      auto it = entries.find(label);
      if (it != entries.end()) {
        (*it).second.markClean();
      }
    }
    static void mark_usd_dirty(std::string_view label = g_defaultUsdLabel) {
      auto &entries = instance()._usdEntries;
      auto it = entries.find(label);
      if (it != entries.end()) {
        (*it).second.markDirty();
      }
    }
    static ZS_WORLD_EXPORT bool close_usd(const std::string &label = g_defaultUsdLabel);
    static bool has_usd(std::string_view label = g_defaultUsdLabel) {
      return instance()._usdEntries.contains(label);
    }
#endif
    static std::vector<std::string> get_usd_labels() noexcept {
      const auto &usdEntries = instance()._usdEntries;
      std::vector<std::string> usdSceneNames;
      usdSceneNames.reserve(usdEntries.size());
      for (auto &[label, entry] : usdEntries) usdSceneNames.emplace_back(label);
      return usdSceneNames;
    }

    /// ui widgets
    template <typename WidgetT, enable_if_t<is_base_of_v<WidgetComponentConcept, WidgetT>> = 0>
    static bool register_widget(std::string_view label, WidgetT &&w) {
      return instance()
          ._widgets.emplace(std::string(label), zs::make_shared<remove_cvref_t<WidgetT>>(FWD(w)))
          .second;
    }
    template <typename WidgetT, enable_if_t<is_base_of_v<WidgetComponentConcept, WidgetT>> = 0>
    static bool register_widget(std::string_view label, WidgetT *w) {
      return instance()._widgets.emplace(std::string(label), Shared<WidgetT>(w)).second;
    }
    static bool register_widget(std::string_view label, Shared<WidgetComponentConcept> w) {
      return instance()._widgets.emplace(std::string(label), w).second;
    }
    template <typename WidgetT, enable_if_t<is_base_of_v<WidgetComponentConcept, WidgetT>> = 0>
    static void set_widget(std::string_view label, WidgetT &&w) {
      instance()._widgets[std::string(label)] = zs::make_shared<remove_cvref_t<WidgetT>>(FWD(w));
    }
    template <typename WidgetT, enable_if_t<is_base_of_v<WidgetComponentConcept, WidgetT>> = 0>
    static void set_widget(std::string_view label, WidgetT *w) {
      instance()._widgets[std::string(label)] = Shared<WidgetT>(w);
    }
    static void set_widget(std::string_view label, Shared<WidgetComponentConcept> w) {
      instance()._widgets[std::string(label)] = w;
    }

    template <typename WidgetT = WidgetComponentConcept>
    static WidgetT *get_widget_ptr(std::string_view label) {
      if (auto it = instance()._widgets.find(label); it == instance()._widgets.end())
        return dynamic_cast<WidgetT *>(&instance()._emptyWidget);
      else
        // return std::dynamic_pointer_cast<WidgetT>(instance()._widgets.at(label));
        return dynamic_cast<WidgetT *>((*it).second.get());
    }
    template <typename WidgetT = WidgetComponentConcept>
    static Shared<WidgetT> ref_widget(const std::string &label) {
      return std::dynamic_pointer_cast<WidgetT>(instance()._widgets.at(label));
    }
    static bool has_widget(const std::string &label) {
      return instance()._widgets.find(label) != instance()._widgets.end();
    }

    /// log
    static ZS_WORLD_EXPORT void start_cstream_capture();
    static ZS_WORLD_EXPORT void dump_cstream_capture();
    /// @return if the info stream is NOT empty
    static ZS_WORLD_EXPORT bool retrieve_info_cstream(std::string &);
    /// @return if the error stream is NOT empty
    static ZS_WORLD_EXPORT bool retrieve_error_cstream(std::string &);

    static u32 get_log_entry_limit() noexcept { return instance()._logEntryLimit; }
    static void set_log_entry_limit(u32 n) noexcept { instance()._logEntryLimit = n; }
    static const auto &ref_logs() noexcept { return instance()._logEntries; }
    static void clear_logs() { instance()._logEntries.clear(); }
    static ZS_WORLD_EXPORT bool push_log(std::string_view msg,
                                         ConsoleRecord::type_e type = ConsoleRecord::info) noexcept;
    static ZS_WORLD_EXPORT void push_assembled_log(std::string_view msg, ConsoleRecord::type_e type
                                                                         = ConsoleRecord::info);

    ///
    static ZS_WORLD_EXPORT void reset();
  };

  inline auto &zs_resources() { return ResourceSystem::instance(); }

}  // namespace zs