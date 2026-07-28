#pragma once

#include <array>
#include <cstddef>
#include <string_view>

namespace amphibia::ui {

struct Rect
{
  float left{};
  float top{};
  float right{};
  float bottom{};

  constexpr float Width() const noexcept { return right - left; }
  constexpr float Height() const noexcept { return bottom - top; }
  constexpr bool Positive() const noexcept { return Width() > 0.f && Height() > 0.f; }
  constexpr bool Contains(const Rect& other) const noexcept
  {
    return other.left >= left && other.top >= top && other.right <= right && other.bottom <= bottom;
  }
  constexpr bool Overlaps(const Rect& other) const noexcept
  {
    return left < other.right && right > other.left && top < other.bottom && bottom > other.top;
  }
};

enum class SettingsSection : std::size_t
{
  General,
  LocalFiles,
  ManagedLibrary,
  Tone3000,
  Calibration,
  Diagnostics,
  About,
  Count
};

inline constexpr std::array<std::string_view, static_cast<std::size_t>(SettingsSection::Count)>
  kSettingsSectionCaptions{{
    "General", "Local Files", "Managed Library", "TONE3000", "Calibration", "Diagnostics", "About"}};
inline constexpr std::string_view kSearchTone3000Caption{"Search TONE3000"};
inline constexpr std::string_view kFindTone3000IrCaption{"Find IR on TONE3000"};
inline constexpr std::array<std::string_view, 5> kTone3000SettingsActionCaptions{{
  "Connect", "Disconnect", "Test Connection", "Cancel Current Operation", "Open TONE3000"}};

struct LayoutMetrics
{
  float outerMargin{20.f};
  float innerMargin{10.f};
  float headerHeight{48.f};
  float footerHeight{34.f};
  float navigationWidth{124.f};
  float navigationGap{12.f};
  float rowHeight{30.f};
  float rowGap{7.f};
  float labelWidth{154.f};
  float buttonWidth{128.f};
  float smallButtonWidth{92.f};
  float textAreaHeight{44.f};
  float statusAreaHeight{72.f};
  float closeButtonSize{28.f};
  float scaleFactor{1.f};
};

struct MainEditorLayout
{
  Rect editor;
  Rect content;
  Rect title;
  std::array<Rect, 6> knobs;
  Rect model;
  Rect impulseResponse;
  Rect searchTone3000;
  Rect findTone3000Ir;
  Rect modelIcon;
  Rect impulseResponseSwitch;
  Rect slimIcon;
  Rect inputMeter;
  Rect outputMeter;
  Rect settings;
};

struct SettingsLayout
{
  Rect editor;
  Rect header;
  Rect navigation;
  Rect content;
  Rect footer;
  Rect close;
  std::array<Rect, static_cast<std::size_t>(SettingsSection::Count)> navigationButtons;
};

constexpr Rect Inset(Rect value, float amount) noexcept
{
  return {value.left + amount, value.top + amount, value.right - amount, value.bottom - amount};
}

constexpr MainEditorLayout CalculateMainEditorLayout(float width, float height,
                                                      float scaleFactor = 1.f) noexcept
{
  const LayoutMetrics metrics{.scaleFactor = scaleFactor};
  const Rect editor{0.f, 0.f, width, height};
  const Rect main = Inset(editor, metrics.outerMargin);
  const Rect content = Inset(main, metrics.innerMargin);
  const float knobLeft = content.left + 20.f;
  const float knobRight = content.right - 20.f;
  const float knobWidth = (knobRight - knobLeft) / 6.f;
  const float knobTop = content.top + metrics.headerHeight + 24.f;
  const float knobBottom = knobTop + 100.f;
  std::array<Rect, 6> knobs{};
  for (std::size_t index = 0; index < knobs.size(); ++index)
  {
    const float left = knobLeft + knobWidth * static_cast<float>(index);
    knobs[index] = {left, knobTop, left + knobWidth, knobBottom};
  }

  constexpr float fileWidth = 200.f;
  constexpr float fileHeight = 30.f;
  constexpr float providerGap = 7.f;
  const float fileLeft = content.left + (content.Width() - fileWidth) * .5f;
  const float modelTop = content.bottom - 67.f;
  const Rect model{fileLeft, modelTop, fileLeft + fileWidth, modelTop + fileHeight};
  const Rect impulseResponse{fileLeft, model.bottom + 7.f, fileLeft + fileWidth, model.bottom + 7.f + fileHeight};
  const Rect search{model.right + providerGap, model.top, content.right, model.bottom};
  const Rect findIr{impulseResponse.right + providerGap, impulseResponse.top, content.right, impulseResponse.bottom};

  return {
    editor,
    content,
    {content.left, content.top, content.right, content.top + metrics.headerHeight},
    knobs,
    model,
    impulseResponse,
    search,
    findIr,
    {model.left - 40.f, model.top + 7.f, model.left - 10.f, model.bottom + 7.f},
    {impulseResponse.left - 40.f, impulseResponse.top + 4.f, impulseResponse.left - 10.f, impulseResponse.bottom - 4.f},
    {model.right + 6.f, model.top - 29.f, model.right + 62.f, model.top - 1.f},
    {content.left - 20.f, content.top + 75.f, content.left + 10.f, content.bottom - 75.f},
    {content.right - 10.f, content.top + 75.f, content.right + 20.f, content.bottom - 75.f},
    {editor.right - 43.f, editor.top + 15.f, editor.right - 15.f, editor.top + 43.f}
  };
}

constexpr SettingsLayout CalculateSettingsLayout(float width, float height,
                                                  float scaleFactor = 1.f) noexcept
{
  const LayoutMetrics metrics{.scaleFactor = scaleFactor};
  const Rect editor{0.f, 0.f, width, height};
  const Rect shell = Inset(editor, metrics.outerMargin);
  const Rect header{shell.left + metrics.innerMargin, shell.top, shell.right - metrics.innerMargin,
                    shell.top + metrics.headerHeight};
  const Rect footer{shell.left + metrics.innerMargin, shell.bottom - metrics.footerHeight,
                    shell.right - metrics.innerMargin, shell.bottom};
  const float bodyTop = header.bottom + metrics.rowGap;
  const float bodyBottom = footer.top - metrics.rowGap;
  const Rect navigation{shell.left + metrics.innerMargin, bodyTop,
                        shell.left + metrics.innerMargin + metrics.navigationWidth, bodyBottom};
  const Rect content{navigation.right + metrics.navigationGap, bodyTop,
                     shell.right - metrics.innerMargin, bodyBottom};
  std::array<Rect, static_cast<std::size_t>(SettingsSection::Count)> buttons{};
  float top = navigation.top;
  for (auto& button : buttons)
  {
    button = {navigation.left, top, navigation.right, top + metrics.rowHeight};
    top += metrics.rowHeight + metrics.rowGap;
  }
  return {
    editor,
    header,
    navigation,
    content,
    footer,
    {editor.right - metrics.outerMargin - metrics.closeButtonSize, editor.top + metrics.outerMargin,
     editor.right - metrics.outerMargin, editor.top + metrics.outerMargin + metrics.closeButtonSize},
    buttons
  };
}

constexpr Rect Row(const Rect& content, std::size_t index, float height = 30.f, float gap = 7.f) noexcept
{
  const float top = content.top + static_cast<float>(index) * (height + gap);
  return {content.left, top, content.right, top + height};
}

constexpr Rect RowButton(const Rect& row, float width = 128.f) noexcept
{
  return {row.right - width, row.top, row.right, row.bottom};
}

constexpr Rect RowLabel(const Rect& row, float buttonWidth = 128.f, float gap = 8.f) noexcept
{
  return {row.left, row.top, row.right - buttonWidth - gap, row.bottom};
}

constexpr Rect GridCell(const Rect& area, std::size_t rows, std::size_t columns,
                        std::size_t index, float gap = 2.f) noexcept
{
  const std::size_t row = index / columns;
  const std::size_t column = index % columns;
  const float width = area.Width() / static_cast<float>(columns);
  const float height = area.Height() / static_cast<float>(rows);
  return {
    area.left + static_cast<float>(column) * width + gap,
    area.top + static_cast<float>(row) * height + gap,
    area.left + static_cast<float>(column + 1) * width - gap,
    area.top + static_cast<float>(row + 1) * height - gap
  };
}

constexpr std::array<bool, static_cast<std::size_t>(SettingsSection::Count)>
CalculateSettingsPageVisibility(SettingsSection active) noexcept
{
  std::array<bool, static_cast<std::size_t>(SettingsSection::Count)> result{};
  result[static_cast<std::size_t>(active)] = true;
  return result;
}

} // namespace amphibia::ui
