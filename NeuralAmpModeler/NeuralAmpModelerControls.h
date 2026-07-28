#pragma once

#include <cmath> // std::round
#include <cstdio> // FILE, fclose
#include <sstream> // std::stringstream
#include <tuple>
#include <unordered_map> // std::unordered_map
#include "IControls.h"
#include "IPlugPaths.h"
#include "Providers/ProviderTypes.h"
#include "Providers/Tone3000/Tone3000HostedSelectionCoordinator.h"
#include "UI/EditorLayout.h"

#ifdef OS_WIN
  #include <Windows.h>
  #include <Shellapi.h>
#endif

#define PLUG() static_cast<PLUG_CLASS_NAME*>(GetDelegate())
#define NAM_KNOB_HEIGHT 120.0f
#define NAM_SWTICH_HEIGHT 50.0f

using namespace iplug;
using namespace igraphics;
using NAMDropHandler = std::function<void(const std::vector<std::string>&)>;

struct Tone3000PanelActions
{
  std::function<void(amphibia::providers::HostedSelectionKind)> begin;
  std::function<void()> reopenBrowser;
  std::function<void()> cancel;
  std::function<void(std::size_t)> selectVariant;
  std::function<void(bool)> download;
  std::function<void(int)> adjacentDownloaded;
  std::function<void()> removeLocalCopy;
  std::function<void(const std::string&)> openSource;
};

class Tone3000SelectionPanelControl final : public IControl
{
public:
  Tone3000SelectionPanelControl(const IRECT& bounds, Tone3000PanelActions actions)
  : IControl(bounds), mActions(std::move(actions))
  {
    mIgnoreMouse = false;
    SetTooltip("Hosted TONE3000 selection and exact variants");
  }

  void ShowArchitectureChoice()
  {
    mArchitectureChoice = true;
    mSnapshot = {};
    Hide(false);
    SetDirty(false);
  }

  void Update(amphibia::providers::HostedSelectionSnapshot snapshot)
  {
    mArchitectureChoice = false;
    mSnapshot = std::move(snapshot);
    Hide(false);
    SetDirty(false);
  }

  bool OnKeyDown(float, float, const IKeyPress& key) override
  {
    if (key.VK != kVK_ESCAPE) return false;
    if (mActions.cancel) mActions.cancel();
    Hide(true);
    return true;
  }

  void OnTextEntryCompletion(const char* str, int) override
  {
    mFilter = str ? str : "";
    if (mFilter.size() > 256) mFilter.resize(256);
    mFirstVisible = 0;
    SetDirty(false);
  }

  void Draw(IGraphics& g) override
  {
    g.FillRect(IColor(205, 0, 0, 0), mRECT);
    const auto panel = mRECT.GetPadded(-24.f);
    g.FillRect(IColor(255, 23, 25, 29), panel);
    g.DrawRect(PluginColors::NAM_THEMECOLOR, panel, nullptr, 1.5f);
    const IText titleText(17.f, COLOR_WHITE, "Michroma-Regular", EAlign::Near);
    const IText text(11.f, PluginColors::HELP_TEXT, "Roboto-Regular", EAlign::Near);
    const IText smallText(9.f, PluginColors::HELP_TEXT, "Roboto-Regular", EAlign::Near);
    auto content = panel.GetPadded(-14.f);
    g.DrawText(titleText, "TONE3000", content.GetFromTop(23.f));
    if (mArchitectureChoice)
    {
      g.DrawText(text, "Choose the hosted NAM architecture filter. TONE3000 cannot combine A2 with the default A1/custom view.",
                 content.GetFromTop(42.f).GetVShifted(30.f));
      DrawButton(g, ChoiceA1(content), "A1 / Custom");
      DrawButton(g, ChoiceA2(content), "A2");
      DrawButton(g, ChoiceCancel(content), "Cancel");
      return;
    }

    const auto toneTitle = mSnapshot.tone ? Bounded(mSnapshot.tone->title, 70) : std::string("Hosted selection");
    g.DrawText(titleText, toneTitle.c_str(), content.GetFromTop(23.f).GetVShifted(25.f));
    std::string attribution;
    if (mSnapshot.tone)
    {
      attribution = "by " + Bounded(mSnapshot.tone->creator.username, 32) + "  |  " +
        FormatName(mSnapshot.tone->format) + "  |  " + GearName(mSnapshot.tone->gearRole);
      if (mSnapshot.tone->licenseName) attribution += "  |  " + Bounded(*mSnapshot.tone->licenseName, 36);
    }
    g.DrawText(text, attribution.c_str(), content.GetFromTop(18.f).GetVShifted(51.f));
    if (mSnapshot.tone && mSnapshot.tone->description)
      g.DrawText(smallText, Bounded(*mSnapshot.tone->description, 150).c_str(), content.GetFromTop(25.f).GetVShifted(70.f));
    g.DrawText(text, Bounded(mSnapshot.statusText, 120).c_str(), content.GetFromTop(20.f).GetVShifted(96.f));

    if (mSnapshot.stage == amphibia::providers::HostedSelectionStage::WaitingForBrowser)
    {
      g.DrawText(text, "Complete the selection in your browser. This panel does not block audio.",
                 content.GetFromTop(24.f).GetVShifted(123.f));
      DrawButton(g, WaitingReopen(content), "Open Browser Again");
      DrawButton(g, WaitingCancel(content), "Cancel");
      return;
    }

    if (!mSnapshot.tone || mSnapshot.stage == amphibia::providers::HostedSelectionStage::Failed ||
        mSnapshot.stage == amphibia::providers::HostedSelectionStage::Cancelled)
    {
      DrawButton(g, WaitingCancel(content), "Close");
      return;
    }

    const auto filter = FilterRect(content);
    g.DrawRect(PluginColors::NAM_THEMECOLOR.WithOpacity(0.55f), filter);
    const auto filterLabel = mFilter.empty() ? "Filter variants in this pack" : mFilter;
    g.DrawText(text, filterLabel.c_str(), filter.GetPadded(-5.f));
    const auto visible = amphibia::providers::tone3000::Tone3000HostedSelectionCoordinator::FilteredVariantIndices(
      mSnapshot, mFilter, mFirstVisible, kVisibleRows);
    mVisible = visible;
    for (std::size_t row = 0; row < visible.size(); ++row)
    {
      const auto index = visible[row];
      const auto& variant = mSnapshot.variants[index];
      const auto bounds = RowRect(content, row);
      if (mSnapshot.selectedVariant && *mSnapshot.selectedVariant == index)
        g.FillRect(PluginColors::NAM_THEMECOLOR.WithOpacity(0.18f), bounds);
      g.DrawRect(PluginColors::NAM_THEMECOLOR.WithOpacity(0.25f), bounds);
      std::string label = Bounded(variant.model.name, 58) + "  |  " + ArchitectureName(variant.model.architecture) +
        "  |  " + FormatName(variant.model.format);
      if (variant.model.declaredSize) label += "  |  " + std::to_string(*variant.model.declaredSize) + " bytes";
      if (variant.downloaded) label += variant.active ? "  |  Active" : "  |  Downloaded";
      if (variant.compatibility != amphibia::providers::VariantCompatibility::Compatible)
        label += "  |  " + Bounded(variant.compatibilityMessage, 42);
      g.DrawText(smallText, label.c_str(), bounds.GetPadded(-4.f));
    }
    DrawButton(g, ActionRect(content, 0), "Download and Audition");
    DrawButton(g, ActionRect(content, 1), "Download Only");
    DrawButton(g, ActionRect(content, 2), "Open on TONE3000");
    DrawButton(g, ActionRect(content, 3), "Cancel");
    DrawButton(g, SecondaryRect(content, 0), "Previous downloaded variant");
    DrawButton(g, SecondaryRect(content, 1), "Next downloaded variant");
    DrawButton(g, SecondaryRect(content, 2), "Remove Local Copy");
  }

  void OnMouseDown(float x, float y, const IMouseMod&) override
  {
    const auto content = mRECT.GetPadded(-38.f);
    if (mArchitectureChoice)
    {
      if (ChoiceA1(content).Contains(x, y) && mActions.begin)
        mActions.begin(amphibia::providers::HostedSelectionKind::NamA1AndCustom);
      else if (ChoiceA2(content).Contains(x, y) && mActions.begin)
        mActions.begin(amphibia::providers::HostedSelectionKind::NamA2);
      else if (ChoiceCancel(content).Contains(x, y)) Hide(true);
      return;
    }
    if (mSnapshot.stage == amphibia::providers::HostedSelectionStage::WaitingForBrowser)
    {
      if (WaitingReopen(content).Contains(x, y) && mActions.reopenBrowser) mActions.reopenBrowser();
      else if (WaitingCancel(content).Contains(x, y) && mActions.cancel) mActions.cancel();
      return;
    }
    if (FilterRect(content).Contains(x, y))
    {
      const IText edit(11.f, COLOR_BLACK, "Roboto-Regular", EAlign::Near);
      GetUI()->CreateTextEntry(*this, edit, FilterRect(content), mFilter.c_str());
      return;
    }
    for (std::size_t row = 0; row < mVisible.size(); ++row)
      if (RowRect(content, row).Contains(x, y) && mActions.selectVariant)
      { mActions.selectVariant(mVisible[row]); return; }
    if (ActionRect(content, 0).Contains(x, y) && mActions.download) mActions.download(true);
    else if (ActionRect(content, 1).Contains(x, y) && mActions.download) mActions.download(false);
    else if (ActionRect(content, 2).Contains(x, y) && mActions.openSource && mSnapshot.tone && mSnapshot.tone->sourcePageUrl)
      mActions.openSource(*mSnapshot.tone->sourcePageUrl);
    else if (ActionRect(content, 3).Contains(x, y) && mActions.cancel) mActions.cancel();
    else if (SecondaryRect(content, 0).Contains(x, y) && mActions.adjacentDownloaded) mActions.adjacentDownloaded(-1);
    else if (SecondaryRect(content, 1).Contains(x, y) && mActions.adjacentDownloaded) mActions.adjacentDownloaded(1);
    else if (SecondaryRect(content, 2).Contains(x, y) && mActions.removeLocalCopy) mActions.removeLocalCopy();
  }

private:
  static constexpr std::size_t kVisibleRows = 6;
  static std::string Bounded(const std::string& value, std::size_t maximum)
  { return value.size() <= maximum ? value : value.substr(0, maximum - 3) + "..."; }
  static const char* FormatName(amphibia::providers::RemoteContentFormat format)
  { return format == amphibia::providers::RemoteContentFormat::Nam ? "NAM" :
           format == amphibia::providers::RemoteContentFormat::ImpulseResponse ? "IR" : "Unknown format"; }
  static const char* ArchitectureName(amphibia::providers::RemoteNamArchitecture architecture)
  { return architecture == amphibia::providers::RemoteNamArchitecture::A1 ? "A1" :
           architecture == amphibia::providers::RemoteNamArchitecture::A2 ? "A2" : "Unknown architecture"; }
  static const char* GearName(amphibia::providers::RemoteGearRole gear)
  { return gear == amphibia::providers::RemoteGearRole::AmpHead ? "Amp" :
           gear == amphibia::providers::RemoteGearRole::AmpAndCab ? "Amp + cab" :
           gear == amphibia::providers::RemoteGearRole::Cabinet ? "Cabinet" : "Other"; }
  static void DrawButton(IGraphics& g, const IRECT& bounds, const char* label)
  {
    g.FillRect(PluginColors::NAM_THEMECOLOR.WithOpacity(0.12f), bounds);
    g.DrawRect(PluginColors::NAM_THEMECOLOR.WithOpacity(0.65f), bounds);
    g.DrawText(IText(9.f, PluginColors::HELP_TEXT, "Roboto-Regular"), label, bounds.GetPadded(-2.f));
  }
  static IRECT ChoiceA1(const IRECT& r) { return r.GetCentredInside(145.f, 30.f).GetHShifted(-82.f); }
  static IRECT ChoiceA2(const IRECT& r) { return r.GetCentredInside(145.f, 30.f).GetHShifted(82.f); }
  static IRECT ChoiceCancel(const IRECT& r) { return r.GetFromBottom(26.f).GetMidHPadded(70.f); }
  static IRECT WaitingReopen(const IRECT& r) { return r.GetFromBottom(32.f).GetFromLeft(190.f); }
  static IRECT WaitingCancel(const IRECT& r) { return r.GetFromBottom(32.f).GetFromRight(90.f); }
  static IRECT FilterRect(const IRECT& r) { return IRECT(r.L, r.T + 121.f, r.R, r.T + 143.f); }
  static IRECT RowRect(const IRECT& r, std::size_t row)
  { return IRECT(r.L, r.T + 147.f + 21.f * row, r.R, r.T + 167.f + 21.f * row); }
  static IRECT ActionRect(const IRECT& r, int index)
  { return r.GetFromBottom(47.f).GetFromTop(21.f).GetGridCell(0, index, 1, 4).GetPadded(-2.f); }
  static IRECT SecondaryRect(const IRECT& r, int index)
  { return r.GetFromBottom(22.f).GetGridCell(0, index, 1, 3).GetPadded(-2.f); }

  Tone3000PanelActions mActions;
  amphibia::providers::HostedSelectionSnapshot mSnapshot;
  std::vector<std::size_t> mVisible;
  std::string mFilter;
  std::size_t mFirstVisible{};
  bool mArchitectureChoice{};
};

enum class NAMBrowserState
{
  Empty, // when no file loaded, show "Get" button
  Loaded // when file loaded, show "Clear" button
};

// Where the corner button on the plugin (settings, close settings) goes
// :param rect: Rect for the whole plugin's UI
IRECT CornerButtonArea(const IRECT& rect)
{
  const auto mainArea = rect.GetPadded(-20);
  return mainArea.GetFromTRHC(50, 50).GetCentredInside(20, 20);
};

class NAMSquareButtonControl : public ISVGButtonControl
{
public:
  NAMSquareButtonControl(const IRECT& bounds, IActionFunction af, const ISVG& svg)
  : ISVGButtonControl(bounds, af, svg, svg)
  {
  }

  void Draw(IGraphics& g) override
  {
    if (mMouseIsOver)
      g.FillRoundRect(PluginColors::MOUSEOVER, mRECT, 2.f);

    ISVGButtonControl::Draw(g);
  }

  void SetDropHandler(NAMDropHandler handler) { mDropHandler = std::move(handler); }
  void OnDrop(const char* path) override { if (mDropHandler && path) mDropHandler({path}); }
  void OnDropMultiple(const std::vector<const char*>& paths) override
  {
    if (!mDropHandler) return;
    std::vector<std::string> owned;
    for (const auto* path : paths) if (path) owned.emplace_back(path);
    mDropHandler(owned);
  }

private:
  NAMDropHandler mDropHandler;
};

class NAMCircleButtonControl : public ISVGButtonControl
{
public:
  NAMCircleButtonControl(const IRECT& bounds, IActionFunction af, const ISVG& svg)
  : ISVGButtonControl(bounds, af, svg, svg)
  {
  }

  void Draw(IGraphics& g) override
  {
    if (mMouseIsOver)
      g.FillEllipse(PluginColors::MOUSEOVER, mRECT);

    ISVGButtonControl::Draw(g);
  }
};

/// Full-window dim layer; click dismisses (used for Slim overlay).
class NAMSlimOverlayBackdropControl : public IControl
{
public:
  NAMSlimOverlayBackdropControl(const IRECT& bounds, IActionFunction dismiss)
  : IControl(bounds, dismiss)
  , mDismiss(dismiss)
  {
  }

  void Draw(IGraphics& g) override { g.FillRect(COLOR_BLACK.WithOpacity(0.45f), mRECT); }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    if (mDismiss)
      mDismiss(this);
  }

private:
  IActionFunction mDismiss;
};

class NAMKnobControl : public IVKnobControl, public IBitmapBase
{
public:
  NAMKnobControl(const IRECT& bounds, int paramIdx, const char* label, const IVStyle& style, IBitmap bitmap)
  : IVKnobControl(bounds, paramIdx, label, style, true)
  , IBitmapBase(bitmap)
  {
    mInnerPointerFrac = 0.55;
  }

  void OnRescale() override { mBitmap = GetUI()->GetScaledBitmap(mBitmap); }

  void DrawWidget(IGraphics& g) override
  {
    float widgetRadius = GetRadius() * 0.73;
    auto knobRect = mWidgetBounds.GetCentredInside(mWidgetBounds.W(), mWidgetBounds.W());
    const float cx = knobRect.MW(), cy = knobRect.MH();
    const float angle = mAngle1 + (static_cast<float>(GetValue()) * (mAngle2 - mAngle1));
    DrawIndicatorTrack(g, angle, cx + 0.5, cy, widgetRadius);
    g.DrawFittedBitmap(mBitmap, knobRect);
    float data[2][2];
    RadialPoints(angle, cx, cy, mInnerPointerFrac * widgetRadius, mInnerPointerFrac * widgetRadius, 2, data);
    g.PathCircle(data[1][0], data[1][1], 3);
    g.PathFill(IPattern::CreateRadialGradient(data[1][0], data[1][1], 4.0f,
                                              {{GetColor(mMouseIsOver ? kX3 : kX1), 0.f},
                                               {GetColor(mMouseIsOver ? kX3 : kX1), 0.8f},
                                               {COLOR_TRANSPARENT, 1.0f}}),
               {}, &mBlend);
    g.DrawCircle(COLOR_BLACK.WithOpacity(0.5f), data[1][0], data[1][1], 3, &mBlend);
  }
};

class NAMSwitchControl : public IVSlideSwitchControl, public IBitmapBase
{
public:
  NAMSwitchControl(const IRECT& bounds, int paramIdx, const char* label, const IVStyle& style, IBitmap bitmap)
  : IVSlideSwitchControl(bounds, paramIdx, label,
                         style.WithRoundness(0.666f)
                           .WithShowValue(false)
                           .WithEmboss(true)
                           .WithShadowOffset(1.5f)
                           .WithDrawShadows(false)
                           .WithColor(kFR, COLOR_BLACK)
                           .WithFrameThickness(0.5f)
                           .WithWidgetFrac(0.5f)
                           .WithLabelOrientation(EOrientation::South))
  , IBitmapBase(bitmap)
  {
  }

  void DrawWidget(IGraphics& g) override
  {
    DrawTrack(g, mWidgetBounds);
    DrawHandle(g, mHandleBounds);
  }

  void DrawTrack(IGraphics& g, const IRECT& bounds) override
  {
    IRECT handleBounds = GetAdjustedHandleBounds(bounds);
    handleBounds = IRECT(handleBounds.L, handleBounds.T, handleBounds.R, handleBounds.T + mBitmap.H());
    IRECT centreBounds = handleBounds.GetPadded(-mStyle.shadowOffset);
    IRECT shadowBounds = handleBounds.GetTranslated(mStyle.shadowOffset, mStyle.shadowOffset);
    //    const float contrast = mDisabled ? -GRAYED_ALPHA : 0.f;
    float cR = 7.f;
    const float tlr = cR;
    const float trr = cR;
    const float blr = cR;
    const float brr = cR;

    // outer shadow
    if (mStyle.drawShadows)
      g.FillRoundRect(GetColor(kSH), shadowBounds, tlr, trr, blr, brr, &mBlend);

    // Embossed style unpressed
    if (mStyle.emboss)
    {
      // Positive light
      g.FillRoundRect(GetColor(kPR), handleBounds, tlr, trr, blr, brr /*, &blend*/);

      // Negative light
      g.FillRoundRect(GetColor(kSH), shadowBounds, tlr, trr, blr, brr /*, &blend*/);

      // Fill in foreground
      g.FillRoundRect(GetValue() > 0.5 ? GetColor(kX1) : COLOR_BLACK, centreBounds, tlr, trr, blr, brr, &mBlend);

      // Shade when hovered
      if (mMouseIsOver)
        g.FillRoundRect(GetColor(kHL), centreBounds, tlr, trr, blr, brr, &mBlend);
    }
    else
    {
      g.FillRoundRect(GetValue() > 0.5 ? GetColor(kX1) : COLOR_BLACK, handleBounds, tlr, trr, blr, brr /*, &blend*/);

      // Shade when hovered
      if (mMouseIsOver)
        g.FillRoundRect(GetColor(kHL), handleBounds, tlr, trr, blr, brr, &mBlend);
    }

    if (mStyle.drawFrame)
      g.DrawRoundRect(GetColor(kFR), handleBounds, tlr, trr, blr, brr, &mBlend, mStyle.frameThickness);
  }

  void DrawHandle(IGraphics& g, const IRECT& filledArea) override
  {
    IRECT r;
    if (GetSelectedIdx() == 0)
    {
      r = filledArea.GetFromLeft(mBitmap.W());
    }
    else
    {
      r = filledArea.GetFromRight(mBitmap.W());
    }

    g.DrawBitmap(mBitmap, r, 0, 0, nullptr);
  }
};

class NAMFileNameControl : public IVButtonControl
{
public:
  NAMFileNameControl(const IRECT& bounds, const char* label, const IVStyle& style)
  : IVButtonControl(bounds, DefaultClickActionFunc, label, style)
  {
  }

  void SetLabelAndTooltip(const char* str)
  {
    SetLabelStr(str);
    SetTooltip(str);
  }

  void SetLabelAndTooltipEllipsizing(const WDL_String& fileName)
  {
    auto EllipsizeFilePath = [](const char* filePath, size_t prefixLength, size_t suffixLength, size_t maxLength) {
      const std::string ellipses = "...";
      assert(maxLength <= (prefixLength + suffixLength + ellipses.size()));
      std::string str{filePath};

      if (str.length() <= maxLength)
      {
        return str;
      }
      else
      {
        return str.substr(0, prefixLength) + ellipses + str.substr(str.length() - suffixLength);
      }
    };

    auto ellipsizedFileName = EllipsizeFilePath(fileName.get_filepart(), 22, 22, 45);
    SetLabelStr(ellipsizedFileName.c_str());
    SetTooltip(fileName.get_filepart());
  }

  void SetDropHandler(NAMDropHandler handler) { mDropHandler = std::move(handler); }
  void OnDrop(const char* path) override { if (mDropHandler && path) mDropHandler({path}); }
  void OnDropMultiple(const std::vector<const char*>& paths) override
  {
    if (!mDropHandler) return;
    std::vector<std::string> owned;
    owned.reserve(paths.size());
    for (const auto* path : paths) if (path) owned.emplace_back(path);
    mDropHandler(owned);
  }

private:
  NAMDropHandler mDropHandler;
};

// URL control for the "Get" models/irs links
class NAMGetButtonControl : public NAMSquareButtonControl
{
public:
  NAMGetButtonControl(const IRECT& bounds, const char* label, const char* url, const ISVG& globeSVG)
  : NAMSquareButtonControl(
      bounds,
      [url](IControl* pCaller) {
        WDL_String fullURL(url);
        pCaller->GetUI()->OpenURL(fullURL.Get());
      },
      globeSVG)
  {
    SetTooltip(label);
  }
};

class NAMFileBrowserControl : public IDirBrowseControlBase
{
public:
  using DropHandler = NAMDropHandler;

  NAMFileBrowserControl(const IRECT& bounds, int clearMsgTag, const char* labelStr, const char* fileExtension,
                        IFileDialogCompletionHandlerFunc ch, const IVStyle& style, const ISVG& loadSVG,
                        const ISVG& clearSVG, const ISVG& leftSVG, const ISVG& rightSVG, const IBitmap& bitmap,
                        const ISVG& globeSVG, const char* getButtonLabel, const char* getButtonURL,
                        DropHandler dropHandler = {})
  : IDirBrowseControlBase(bounds, fileExtension, false, false)
  , mClearMsgTag(clearMsgTag)
  , mDefaultLabelStr(labelStr)
  , mCompletionHandlerFunc(ch)
  , mStyle(style.WithColor(kFG, COLOR_TRANSPARENT).WithDrawFrame(false))
  , mBitmap(bitmap)
  , mLoadSVG(loadSVG)
  , mClearSVG(clearSVG)
  , mLeftSVG(leftSVG)
  , mRightSVG(rightSVG)
  , mGlobeSVG(globeSVG)
  , mGetButtonLabel(getButtonLabel)
  , mGetButtonURL(getButtonURL)
  , mDropHandler(std::move(dropHandler))
  , mBrowserState(NAMBrowserState::Empty)
  {
    mIgnoreMouse = true;
  }

  void Draw(IGraphics& g) override { g.DrawFittedBitmap(mBitmap, mRECT); }

  // Seeds same-directory Previous/Next navigation without loading a file.
  void SetPreferredDirectory(const char* directory)
  {
    if (!directory || !*directory) return;
    ClearPathList();
    AddPath(directory, "");
    SetupMenu();
    SelectFirstFile();
  }

  void OnDrop(const char* path) override
  {
    if (mDropHandler && path) mDropHandler({path});
  }

  void OnDropMultiple(const std::vector<const char*>& paths) override
  {
    if (!mDropHandler) return;
    std::vector<std::string> owned;
    owned.reserve(paths.size());
    for (const auto* path : paths) if (path) owned.emplace_back(path);
    mDropHandler(owned);
  }

  void OnPopupMenuSelection(IPopupMenu* pSelectedMenu, int valIdx) override
  {
    if (pSelectedMenu)
    {
      IPopupMenu::Item* pItem = pSelectedMenu->GetChosenItem();

      if (pItem)
      {
        mSelectedItemIndex = mItems.Find(pItem);
        LoadFileAtCurrentIndex();
      }
    }
  }

  void OnAttached() override
  {
    auto prevFileFunc = [&](IControl* pCaller) {
      const auto nItems = NItems();
      if (nItems == 0)
        return;
      mSelectedItemIndex--;

      if (mSelectedItemIndex < 0)
        mSelectedItemIndex = nItems - 1;

      LoadFileAtCurrentIndex();
    };

    auto nextFileFunc = [&](IControl* pCaller) {
      const auto nItems = NItems();
      if (nItems == 0)
        return;
      mSelectedItemIndex++;

      if (mSelectedItemIndex >= nItems)
        mSelectedItemIndex = 0;

      LoadFileAtCurrentIndex();
    };

    auto loadFileFunc = [&](IControl* pCaller) {
      WDL_String fileName;
      WDL_String path;
      GetSelectedFileDirectory(path);
#ifdef NAM_PICK_DIRECTORY
      pCaller->GetUI()->PromptForDirectory(path, [&](const WDL_String& fileName, const WDL_String& path) {
        if (path.GetLength())
        {
          ClearPathList();
          AddPath(path.Get(), "");
          SetupMenu();
          SelectFirstFile();
          LoadFileAtCurrentIndex();
        }
      });
#else
      pCaller->GetUI()->PromptForFile(
        fileName, path, EFileAction::Open, mExtension.Get(), [&](const WDL_String& fileName, const WDL_String& path) {
          if (fileName.GetLength())
          {
            ClearPathList();
            AddPath(path.Get(), "");
            SetupMenu();
            SetSelectedFile(fileName.Get());
            LoadFileAtCurrentIndex();
          }
        });
#endif
    };

    auto clearFileFunc = [&](IControl* pCaller) {
      pCaller->GetDelegate()->SendArbitraryMsgFromUI(mClearMsgTag);
      mFileNameControl->SetLabelAndTooltip(mDefaultLabelStr.Get());
      SetBrowserState(NAMBrowserState::Empty);
      // FIXME disabling output mode...
      //      pCaller->GetUI()->GetControlWithTag(kCtrlTagOutputMode)->SetDisabled(false);
    };

    auto chooseFileFunc = [&, loadFileFunc](IControl* pCaller) {
      if (std::string_view(pCaller->As<IVButtonControl>()->GetLabelStr()) == mDefaultLabelStr.Get())
      {
        loadFileFunc(pCaller);
      }
      else
      {
        CheckSelectedItem();

        if (!mMainMenu.HasSubMenus())
        {
          mMainMenu.SetChosenItemIdx(mSelectedItemIndex);
        }
        pCaller->GetUI()->CreatePopupMenu(*this, mMainMenu, pCaller->GetRECT());
      }
    };

    IRECT padded = mRECT.GetPadded(-6.f).GetHPadded(-2.f);
    const auto buttonWidth = padded.H();
    const auto loadFileButtonBounds = padded.ReduceFromLeft(buttonWidth);
    const auto clearAndGetButtonBounds = padded.ReduceFromRight(buttonWidth);
    const auto leftButtonBounds = padded.ReduceFromLeft(buttonWidth);
    const auto rightButtonBounds = padded.ReduceFromLeft(buttonWidth);
    const auto fileNameButtonBounds = padded;

    auto* loadButton = new NAMSquareButtonControl(loadFileButtonBounds, DefaultClickActionFunc, mLoadSVG);
    loadButton->SetDropHandler(mDropHandler);
    AddChildControl(loadButton)->SetAnimationEndActionFunction(loadFileFunc);
    auto* leftButton = new NAMSquareButtonControl(leftButtonBounds, DefaultClickActionFunc, mLeftSVG);
    leftButton->SetDropHandler(mDropHandler);
    AddChildControl(leftButton)->SetAnimationEndActionFunction(prevFileFunc);
    auto* rightButton = new NAMSquareButtonControl(rightButtonBounds, DefaultClickActionFunc, mRightSVG);
    rightButton->SetDropHandler(mDropHandler);
    AddChildControl(rightButton)->SetAnimationEndActionFunction(nextFileFunc);
    AddChildControl(mFileNameControl = new NAMFileNameControl(fileNameButtonBounds, mDefaultLabelStr.Get(), mStyle))
      ->SetAnimationEndActionFunction(chooseFileFunc);
    mFileNameControl->SetDropHandler(mDropHandler);

    // creates both right-side controls but only show one based on state
    mClearButton = new NAMSquareButtonControl(clearAndGetButtonBounds, DefaultClickActionFunc, mClearSVG);
    mClearButton->SetDropHandler(mDropHandler);
    mClearButton->SetAnimationEndActionFunction(clearFileFunc);
    AddChildControl(mClearButton);

    mGetButton = new NAMGetButtonControl(clearAndGetButtonBounds, mGetButtonLabel, mGetButtonURL, mGlobeSVG);
    mGetButton->SetDropHandler(mDropHandler);
    AddChildControl(mGetButton);

    // initialize control visibility
    SetBrowserState(NAMBrowserState::Empty);
  }

  void LoadFileAtCurrentIndex()
  {
    if (mSelectedItemIndex > -1 && mSelectedItemIndex < NItems())
    {
      WDL_String fileName, path;
      GetSelectedFile(fileName);
      mFileNameControl->SetLabelAndTooltipEllipsizing(fileName);
      mCompletionHandlerFunc(fileName, path);
    }
  }

  void OnMsgFromDelegate(int msgTag, int dataSize, const void* pData) override
  {
    switch (msgTag)
    {
      case kMsgTagLoadFailed:
        {
          std::string label;
          if (pData != nullptr && dataSize > 0)
            label.assign(reinterpret_cast<const char*>(pData), static_cast<size_t>(dataSize));
          else
            label = std::string("(FAILED) ") + std::string(mFileNameControl->GetLabelStr());
          mFileNameControl->SetLabelAndTooltip(label.c_str());
        }
        break;
      case kMsgTagLoadStatus:
        if (pData != nullptr && dataSize > 0)
        {
          const std::string label(reinterpret_cast<const char*>(pData), static_cast<size_t>(dataSize));
          const WDL_String labelString(label.c_str());
          mFileNameControl->SetLabelAndTooltipEllipsizing(labelString);
        }
        break;
      case kMsgTagLoadedModel:
      case kMsgTagLoadedIR:
      {
        WDL_String fileName, directory;
        fileName.Set(reinterpret_cast<const char*>(pData));
        directory.Set(reinterpret_cast<const char*>(pData));
        directory.remove_filepart(true);

        ClearPathList();
        AddPath(directory.Get(), "");
        SetupMenu();
        SetSelectedFile(fileName.Get());
        mFileNameControl->SetLabelAndTooltipEllipsizing(fileName);
        SetBrowserState(NAMBrowserState::Loaded);
      }
      break;
      default: break;
    }
  }

private:
  void SelectFirstFile() { mSelectedItemIndex = mFiles.GetSize() ? 0 : -1; }

  void GetSelectedFileDirectory(WDL_String& path)
  {
    GetSelectedFile(path);
    path.remove_filepart();
    return;
  }

  // set the state of the browser and the visibility of the "Get" vs. "Clear" buttons
  void SetBrowserState(NAMBrowserState newState)
  {
    mBrowserState = newState;

    switch (mBrowserState)
    {
      case NAMBrowserState::Empty:
        mClearButton->Hide(true);
        mGetButton->Hide(false);
        break;
      case NAMBrowserState::Loaded:
        mClearButton->Hide(false);
        mGetButton->Hide(true);
        break;
    }
  }

  WDL_String mDefaultLabelStr;
  IFileDialogCompletionHandlerFunc mCompletionHandlerFunc;
  NAMFileNameControl* mFileNameControl = nullptr;
  IVStyle mStyle;
  IBitmap mBitmap;
  ISVG mLoadSVG, mClearSVG, mLeftSVG, mRightSVG, mGlobeSVG;
  int mClearMsgTag;

  // new members for the "Get" button
  const char* mGetButtonLabel;
  const char* mGetButtonURL;
  DropHandler mDropHandler;
  NAMBrowserState mBrowserState;
  NAMSquareButtonControl* mClearButton = nullptr;
  NAMGetButtonControl* mGetButton = nullptr;
};

class NAMMeterControl : public IVPeakAvgMeterControl<>, public IBitmapBase
{
  static constexpr float KMeterMin = -70.0f;
  static constexpr float KMeterMax = -0.01f;

public:
  NAMMeterControl(const IRECT& bounds, const IBitmap& bitmap, const IVStyle& style)
  : IVPeakAvgMeterControl<>(bounds, "", style.WithShowValue(false).WithDrawFrame(false).WithWidgetFrac(0.8),
                            EDirection::Vertical, {}, 0, KMeterMin, KMeterMax, {})
  , IBitmapBase(bitmap)
  {
    SetPeakSize(1.0f);
  }

  void OnRescale() override { mBitmap = GetUI()->GetScaledBitmap(mBitmap); }

  virtual void OnResize() override
  {
    SetTargetRECT(MakeRects(mRECT));
    mWidgetBounds = mWidgetBounds.GetMidHPadded(5).GetVPadded(10);
    MakeTrackRects(mWidgetBounds);
    MakeStepRects(mWidgetBounds, mNSteps);
    SetDirty(false);
  }

  void DrawBackground(IGraphics& g, const IRECT& r) override { g.DrawFittedBitmap(mBitmap, r); }

  void DrawTrackHandle(IGraphics& g, const IRECT& r, int chIdx, bool aboveBaseValue) override
  {
    if (r.H() > 2)
      g.FillRect(GetColor(kX1), r, &mBlend);
  }

  void DrawPeak(IGraphics& g, const IRECT& r, int chIdx, bool aboveBaseValue) override
  {
    g.DrawGrid(COLOR_BLACK, mTrackBounds.Get()[chIdx], 10, 2);
    g.FillRect(GetColor(kX3), r, &mBlend);
  }
};

// Container where we can refer to children by names instead of indices
class IContainerBaseWithNamedChildren : public IContainerBase
{
public:
  IContainerBaseWithNamedChildren(const IRECT& bounds)
  : IContainerBase(bounds) {};
  ~IContainerBaseWithNamedChildren() = default;

protected:
  IControl* AddNamedChildControl(IControl* control, std::string name, int ctrlTag = kNoTag, const char* group = "")
  {
    // Make sure we haven't already used this name
    assert(mChildNameIndexMap.find(name) == mChildNameIndexMap.end());
    mChildNameIndexMap[name] = NChildren();
    return AddChildControl(control, ctrlTag, group);
  };

  IControl* GetNamedChild(std::string name)
  {
    const int index = mChildNameIndexMap[name];
    return GetChild(index);
  };


private:
  std::unordered_map<std::string, int> mChildNameIndexMap;
}; // class IContainerBaseWithNamedChildren


struct PossiblyKnownParameter
{
  bool known = false;
  double value = 0.0;
};

struct ModelInfo
{
  PossiblyKnownParameter sampleRate;
  PossiblyKnownParameter inputCalibrationLevel;
  PossiblyKnownParameter outputCalibrationLevel;
};

class ModelInfoControl : public IContainerBaseWithNamedChildren
{
public:
  ModelInfoControl(const IRECT& bounds, const IVStyle& style)
  : IContainerBaseWithNamedChildren(bounds)
  , mStyle(style) {};

  void ClearModelInfo()
  {
    static_cast<IVLabelControl*>(GetNamedChild(mControlNames.sampleRate))->SetStr("");
    mHasInfo = false;
  };

  void Hide(bool hide) override
  {
    // Don't show me unless I have info to show!
    IContainerBase::Hide(hide || (!mHasInfo));
  };

  void OnAttached() override
  {
    AddChildControl(new IVLabelControl(GetRECT().SubRectVertical(4, 0), "Model information:", mStyle));
    AddNamedChildControl(new IVLabelControl(GetRECT().SubRectVertical(4, 1), "", mStyle), mControlNames.sampleRate);
    // AddNamedChildControl(
    //   new IVLabelControl(GetRECT().SubRectVertical(4, 2), "", mStyle), mControlNames.inputCalibrationLevel);
    // AddNamedChildControl(
    //   new IVLabelControl(GetRECT().SubRectVertical(4, 3), "", mStyle), mControlNames.outputCalibrationLevel);
  };

  void SetModelInfo(const ModelInfo& modelInfo)
  {
    auto SetControlStr = [&](const std::string& name, const PossiblyKnownParameter& p, const std::string& units,
                             const std::string& childName) {
      std::stringstream ss;
      ss << name << ": ";
      if (p.known)
      {
        ss << p.value << " " << units;
      }
      else
      {
        ss << "(Unknown)";
      }
      static_cast<IVLabelControl*>(GetNamedChild(childName))->SetStr(ss.str().c_str());
    };

    SetControlStr("Sample rate", modelInfo.sampleRate, "Hz", mControlNames.sampleRate);
    // SetControlStr(
    //   "Input calibration level", modelInfo.inputCalibrationLevel, "dBu", mControlNames.inputCalibrationLevel);
    // SetControlStr(
    //   "Output calibration level", modelInfo.outputCalibrationLevel, "dBu", mControlNames.outputCalibrationLevel);

    mHasInfo = true;
  };

private:
  const IVStyle mStyle;
  struct
  {
    const std::string sampleRate = "sampleRate";
    // const std::string inputCalibrationLevel = "inputCalibrationLevel";
    // const std::string outputCalibrationLevel = "outputCalibrationLevel";
  } mControlNames;
  // Do I have info?
  bool mHasInfo = false;
};

class OutputModeControl : public IVRadioButtonControl
{
public:
  OutputModeControl(const IRECT& bounds, int paramIdx, const IVStyle& style, float buttonSize)
  : IVRadioButtonControl(
      bounds, paramIdx, {}, "Output Mode", style, EVShape::Ellipse, EDirection::Vertical, buttonSize) {};

  void SetNormalizedDisable(const bool disable)
  {
    // HACK non-DRY string and hard-coded indices
    std::stringstream ss;
    ss << "Normalized";
    if (disable)
    {
      ss << " [Not supported by model]";
    }
    mTabLabels.Get(1)->Set(ss.str().c_str());
  };
  void SetCalibratedDisable(const bool disable)
  {
    // HACK non-DRY string and hard-coded indices
    std::stringstream ss;
    ss << "Calibrated";
    if (disable)
    {
      ss << " [Not supported by model]";
    }
    mTabLabels.Get(2)->Set(ss.str().c_str());
  };
};

struct LibraryUIActions
{
  std::function<std::string()> information;
  IActionFunction openFolder;
  IActionFunction verify;
  IActionFunction removeUnused;
  IActionFunction clearStaging;
  IActionFunction importFiles;
  IActionFunction importFolder;
  IActionFunction importZip;
};

struct RemoteProviderUIActions
{
  std::function<std::string()> information;
  IActionFunction connect;
  IActionFunction disconnect;
  IActionFunction testConnection;
  IActionFunction cancel;
  IActionFunction open;
  bool connectEnabled{};
  bool disconnectEnabled{};
  bool testEnabled{};
  bool cancelEnabled{};
  bool openEnabled{};
};

struct LocalFolderUIActions
{
  std::function<std::string()> information;
  IActionFunction chooseNamFolder;
  IActionFunction chooseIrFolder;
};

class NAMConfigurationPanelControl : public IContainerBase
{
public:
  NAMConfigurationPanelControl(const IRECT& bounds, const IBitmap& bitmap, ISVG closeSVG, const IVStyle& style,
                               LocalFolderUIActions folderActions, LibraryUIActions libraryActions,
                               RemoteProviderUIActions providerActions)
  : IContainerBase(bounds), mBitmap(bitmap), mCloseSVG(closeSVG), mStyle(style),
    mFolderActions(std::move(folderActions)), mLibraryActions(std::move(libraryActions)),
    mProviderActions(std::move(providerActions)) { mIgnoreMouse = false; }

  void ShowPanel()
  {
    Refresh();
    Hide(false);
    GetUI()->SetAllControlsDirty();
  }

  bool OnKeyDown(float, float, const IKeyPress& key) override
  {
    if (key.VK != kVK_ESCAPE) return false;
    Hide(true);
    return true;
  }

  void OnAttached() override
  {
    const auto bounds = GetRECT();
    const auto padded = bounds.GetPadded(-24.f);
    const auto text = IText(DEFAULT_TEXT_SIZE, EAlign::Near, PluginColors::HELP_TEXT);
    const auto labelStyle = mStyle.WithDrawFrame(false).WithValueText(text);
    const auto buttonStyle = mStyle.WithShowLabel(true).WithDrawFrame(true)
      .WithLabelText(IText(11.f, EAlign::Center, PluginColors::NAM_THEMEFONTCOLOR));
    AddChildControl(new IBitmapControl(bounds, mBitmap))->SetIgnoreMouse(true);
    AddChildControl(new IVLabelControl(padded.GetFromTop(42.f), "CONFIGURATION",
      DEFAULT_STYLE.WithValueText(IText(26.f, COLOR_WHITE, "Michroma-Regular")).WithDrawFrame(false)));
#ifdef APP_API
    constexpr const char* audioOwnership = "Standalone audio devices, channels, sample rate, and buffer are configured in Audio Settings.";
#else
    constexpr const char* audioOwnership = "Audio devices, channel routing, sample rate, and buffer size are controlled by your DAW.";
#endif
    AddChildControl(new IVLabelControl(IRECT(28.f, 68.f, PLUG_WIDTH - 28.f, 91.f), audioOwnership, labelStyle));
    mFolderInfo = new IVLabelControl(IRECT(28.f, 100.f, PLUG_WIDTH - 28.f, 140.f), "Local folders unavailable", labelStyle);
    AddChildControl(mFolderInfo);
    AddChildControl(new IVButtonControl(IRECT(28.f, 143.f, 275.f, 172.f), mFolderActions.chooseNamFolder,
                                        "Choose NAM folder", buttonStyle));
    AddChildControl(new IVButtonControl(IRECT(325.f, 143.f, 572.f, 172.f), mFolderActions.chooseIrFolder,
                                        "Choose IR folder", buttonStyle));
    mLibraryInfo = new IVLabelControl(IRECT(28.f, 184.f, PLUG_WIDTH - 28.f, 226.f), "Managed library unavailable", labelStyle);
    AddChildControl(mLibraryInfo);
    const std::array<std::pair<const char*, IActionFunction>, 4> libraryButtons{{
      {"Open library", mLibraryActions.openFolder}, {"Verify", mLibraryActions.verify},
      {"Import folder", mLibraryActions.importFolder}, {"Import ZIP", mLibraryActions.importZip}}};
    for (std::size_t index = 0; index < libraryButtons.size(); ++index)
      AddChildControl(new IVButtonControl(IRECT(28.f + index * 137.f, 230.f, 159.f + index * 137.f, 259.f),
                                          libraryButtons[index].second, libraryButtons[index].first, buttonStyle));
    mProviderInfo = new IVLabelControl(IRECT(28.f, 274.f, PLUG_WIDTH - 28.f, 318.f),
                                       "TONE3000 is not configured", labelStyle);
    AddChildControl(mProviderInfo);
    const std::array<std::tuple<const char*, IActionFunction, bool>, 3> providerButtons{{
      {"Connect", mProviderActions.connect, mProviderActions.connectEnabled},
      {"Disconnect", mProviderActions.disconnect, mProviderActions.disconnectEnabled},
      {"Test connection", mProviderActions.testConnection, mProviderActions.testEnabled}}};
    for (std::size_t index = 0; index < providerButtons.size(); ++index)
    {
      auto action = std::get<1>(providerButtons[index]);
      if (!action) action = [](IControl*) {};
      auto* button = AddChildControl(new IVButtonControl(IRECT(28.f + index * 183.f, 322.f, 205.f + index * 183.f, 352.f),
                                                        std::move(action), std::get<0>(providerButtons[index]), buttonStyle));
      button->SetDisabled(!std::get<2>(providerButtons[index]));
    }
    AddChildControl(new NAMSquareButtonControl(CornerButtonArea(bounds), [](IControl* caller) {
      caller->GetParent()->Hide(true);
    }, mCloseSVG));
    Refresh();
  }

private:
  void Refresh()
  {
    if (mFolderInfo && mFolderActions.information) mFolderInfo->SetStr(mFolderActions.information().c_str());
    if (mLibraryInfo && mLibraryActions.information) mLibraryInfo->SetStr(mLibraryActions.information().c_str());
    if (mProviderInfo && mProviderActions.information) mProviderInfo->SetStr(mProviderActions.information().c_str());
  }
  IBitmap mBitmap;
  ISVG mCloseSVG;
  IVStyle mStyle;
  LocalFolderUIActions mFolderActions;
  LibraryUIActions mLibraryActions;
  RemoteProviderUIActions mProviderActions;
  IVLabelControl* mFolderInfo{};
  IVLabelControl* mLibraryInfo{};
  IVLabelControl* mProviderInfo{};
};

class NAMSettingsPageControl : public IContainerBaseWithNamedChildren
{
public:
  NAMSettingsPageControl(const IRECT& bounds, const IBitmap& bitmap, const IBitmap& inputLevelBackgroundBitmap,
                         const IBitmap& switchBitmap, ISVG closeSVG, const IVStyle& style,
                         const IVStyle& radioButtonStyle, LocalFolderUIActions folderActions = {},
                         LibraryUIActions libraryActions = {},
                         RemoteProviderUIActions providerActions = {})
  : IContainerBaseWithNamedChildren(bounds)
  , mAnimationTime(0)
  , mBitmap(bitmap)
  , mInputLevelBackgroundBitmap(inputLevelBackgroundBitmap)
  , mSwitchBitmap(switchBitmap)
  , mStyle(style)
  , mRadioButtonStyle(radioButtonStyle)
  , mCloseSVG(closeSVG)
  , mFolderActions(std::move(folderActions))
  , mLibraryActions(std::move(libraryActions))
  , mProviderActions(std::move(providerActions))
  {
    mIgnoreMouse = false;
  }

  void ClearModelInfo()
  {
    if (mModelInfo) mModelInfo->ClearModelInfo();
  }

  void RefreshLibraryInfo()
  {
    if (mLibraryInfo && mLibraryActions.information)
      mLibraryInfo->SetStr(mLibraryActions.information().c_str());
  }

  void RefreshProviderInfo()
  {
    if (mProviderInfo && mProviderActions.information)
      mProviderInfo->SetStr(mProviderActions.information().c_str());
  }

  void RefreshFolderInfo()
  {
    if (mFolderInfo && mFolderActions.information)
      mFolderInfo->SetStr(mFolderActions.information().c_str());
  }

  void ShowSection(amphibia::ui::SettingsSection section)
  {
    mActiveSection = section;
    ApplySectionVisibility();
    RefreshFolderInfo();
    RefreshLibraryInfo();
    RefreshProviderInfo();
    HideAnimated(false);
  }

  bool OnKeyDown(float x, float y, const IKeyPress& key) override
  {
    if (key.VK == kVK_ESCAPE)
    {
      HideAnimated(true);
      return true;
    }

    return false;
  }

  void HideAnimated(bool hide)
  {
    mWillHide = hide;

    if (hide == false)
    {
      mHide = false;
      ForAllChildrenFunc([](int, IControl* child) { child->Hide(false); });
      ApplySectionVisibility();
    }
    else // hide subcontrols immediately
    {
      ForAllChildrenFunc([hide](int childIdx, IControl* pChild) { pChild->Hide(hide); });
    }

    SetAnimation(
      [&](IControl* pCaller) {
        auto progress = static_cast<float>(pCaller->GetAnimationProgress());

        if (mWillHide)
          SetBlend(IBlend(EBlend::Default, 1.0f - progress));
        else
          SetBlend(IBlend(EBlend::Default, progress));

        if (progress > 1.0f)
        {
          pCaller->OnEndAnimation();
          IContainerBase::Hide(mWillHide);
          GetUI()->SetAllControlsDirty();
          return;
        }
      },
      mAnimationTime);

    SetDirty(true);
  }

  void OnAttached() override
  {
    const auto calculated = amphibia::ui::CalculateSettingsLayout(GetRECT().W(), GetRECT().H());
    const auto rect = [](const amphibia::ui::Rect& value) {
      return IRECT(value.left, value.top, value.right, value.bottom);
    };
    const auto text = IText(DEFAULT_TEXT_SIZE, EAlign::Center, PluginColors::HELP_TEXT);
    const auto leftText = text.WithAlign(EAlign::Near);
    const auto labelStyle = mStyle.WithDrawFrame(false).WithValueText(leftText);
    const auto headingStyle = DEFAULT_STYLE
      .WithValueText(IText(18.f, COLOR_WHITE, "Michroma-Regular"))
      .WithDrawFrame(false);
    const auto buttonStyle = mStyle.WithShowLabel(true).WithDrawFrame(true)
      .WithLabelText(IText(11.f, EAlign::Center, PluginColors::NAM_THEMEFONTCOLOR));

    AddNamedChildControl(new IBitmapControl(GetRECT(), mBitmap), mControlNames.bitmap)->SetIgnoreMouse(true);
    AddNamedChildControl(new IVLabelControl(rect(calculated.header), "SETTINGS",
      DEFAULT_STYLE.WithValueText(IText(26.f, COLOR_WHITE, "Michroma-Regular")).WithDrawFrame(false)),
      mControlNames.title);

    for (std::size_t index = 0; index < amphibia::ui::kSettingsSectionCaptions.size(); ++index)
    {
      AddChildControl(new IVButtonControl(rect(calculated.navigationButtons[index]),
        [this, index](IControl*) { ShowSection(static_cast<amphibia::ui::SettingsSection>(index)); },
        amphibia::ui::kSettingsSectionCaptions[index].data(), buttonStyle));
    }

    const IRECT content = rect(calculated.content);
    const auto addPage = [&](amphibia::ui::SettingsSection section) {
      auto* page = new IContainerBase(content);
      AddChildControl(page);
      page->SetIgnoreMouse(false);
      mPages[static_cast<std::size_t>(section)] = page;
      return page;
    };
    const auto row = [&](std::size_t index, float height = 30.f) {
      const auto value = amphibia::ui::Row(
        {content.L, content.T, content.R, content.B}, index, height, 7.f);
      return rect(value);
    };

    auto* general = addPage(amphibia::ui::SettingsSection::General);
    general->AddChildControl(new IVLabelControl(row(0), "GENERAL", headingStyle));
#ifdef APP_API
    constexpr const char* audioOwnership =
      "Standalone audio devices, channels, sample rate, and buffer size are configured in Audio Settings.";
#else
    constexpr const char* audioOwnership =
      "Audio devices, channel routing, sample rate, and buffer size are controlled by your DAW.";
#endif
    general->AddChildControl(new IVLabelControl(IRECT(content.L, row(1).T, content.R, row(1).B + 18.f),
                                                audioOwnership, labelStyle));
    general->AddChildControl(new IVLabelControl(IRECT(content.L, row(3).T, content.R, row(3).B + 18.f),
      "Local files remain primary. Hosted actions start only after an explicit click.", labelStyle));

    auto* local = addPage(amphibia::ui::SettingsSection::LocalFiles);
    local->AddChildControl(new IVLabelControl(row(0), "LOCAL FILES", headingStyle));
    mFolderInfo = new IVLabelControl(IRECT(content.L, row(1).T, content.R, row(2).B),
                                    "Local folders unavailable", labelStyle);
    local->AddChildControl(mFolderInfo);
    local->AddChildControl(new IVButtonControl(row(3), mFolderActions.chooseNamFolder,
                                               "Choose NAM folder", buttonStyle));
    local->AddChildControl(new IVButtonControl(row(4), mFolderActions.chooseIrFolder,
                                               "Choose IR folder", buttonStyle));

    auto* library = addPage(amphibia::ui::SettingsSection::ManagedLibrary);
    library->AddChildControl(new IVLabelControl(row(0), "MANAGED LIBRARY", headingStyle));
    mLibraryInfo = new IVLabelControl(IRECT(content.L, row(1).T, content.R, row(2).B),
                                     "Managed library unavailable", labelStyle);
    library->AddChildControl(mLibraryInfo);
    const std::array<std::pair<const char*, IActionFunction>, 7> libraryActions{{
      {"Open folder", mLibraryActions.openFolder}, {"Verify", mLibraryActions.verify},
      {"Remove unused", mLibraryActions.removeUnused}, {"Clear staging", mLibraryActions.clearStaging},
      {"Import files", mLibraryActions.importFiles}, {"Import folder", mLibraryActions.importFolder},
      {"Import ZIP", mLibraryActions.importZip}}};
    const amphibia::ui::Rect libraryButtons{content.L, row(3).T, content.R, row(5).B};
    for (std::size_t index = 0; index < libraryActions.size(); ++index)
      library->AddChildControl(new IVButtonControl(
        rect(amphibia::ui::GridCell(libraryButtons, 3, 3, index)),
        libraryActions[index].second, libraryActions[index].first, buttonStyle));

    auto* provider = addPage(amphibia::ui::SettingsSection::Tone3000);
    provider->AddChildControl(new IVLabelControl(row(0), "TONE3000", headingStyle));
    mProviderInfo = new IVLabelControl(IRECT(content.L, row(1).T, content.R, row(2).B + 18.f),
      "Application registration: Development\nCallback: Fixed local callback\nStatus: Disconnected", labelStyle);
    provider->AddChildControl(mProviderInfo);
    const std::array<std::tuple<std::string_view, IActionFunction, bool>, 5> providerActions{{
      {amphibia::ui::kTone3000SettingsActionCaptions[0], mProviderActions.connect, mProviderActions.connectEnabled},
      {amphibia::ui::kTone3000SettingsActionCaptions[1], mProviderActions.disconnect, mProviderActions.disconnectEnabled},
      {amphibia::ui::kTone3000SettingsActionCaptions[2], mProviderActions.testConnection, mProviderActions.testEnabled},
      {amphibia::ui::kTone3000SettingsActionCaptions[3], mProviderActions.cancel, mProviderActions.cancelEnabled},
      {amphibia::ui::kTone3000SettingsActionCaptions[4], mProviderActions.open, mProviderActions.openEnabled}}};
    const amphibia::ui::Rect providerButtons{content.L, row(3).T, content.R, row(5).B};
    for (std::size_t index = 0; index < providerActions.size(); ++index)
    {
      auto action = std::get<1>(providerActions[index]);
      if (!action) action = [](IControl*) {};
      auto* button = new IVButtonControl(
        rect(amphibia::ui::GridCell(providerButtons, 3, 2, index)),
        std::move(action), std::get<0>(providerActions[index]).data(), buttonStyle);
      provider->AddChildControl(button);
      button->SetDisabled(!std::get<2>(providerActions[index]));
    }

    auto* calibration = addPage(amphibia::ui::SettingsSection::Calibration);
    calibration->AddChildControl(new IVLabelControl(row(0), "CALIBRATION", headingStyle));
    const auto inputArea = IRECT(content.L, row(1).T, content.L + content.W() * .48f, content.B);
    const auto outputArea = IRECT(content.L + content.W() * .52f, row(1).T, content.R, content.B);
    auto* inputLevelControl = new InputLevelControl(inputArea.GetFromTop(42.f), kInputCalibrationLevel,
                                                    mInputLevelBackgroundBitmap, text);
    calibration->AddChildControl(inputLevelControl, kCtrlTagInputCalibrationLevel);
    inputLevelControl->SetTooltip(
      "The analog level in dBu RMS corresponding to 0 dBFS peak at the plug-in input.");
    calibration->AddChildControl(new NAMSwitchControl(
      IRECT(inputArea.L, inputArea.T + 54.f, inputArea.R, inputArea.T + 104.f),
      kCalibrateInput, "Calibrate Input", mStyle, mSwitchBitmap), kCtrlTagCalibrateInput);
    auto* outputModeControl =
      new OutputModeControl(outputArea.GetFromTop(126.f), kOutputMode, mRadioButtonStyle, 10.f);
    calibration->AddChildControl(outputModeControl, kCtrlTagOutputMode);
    outputModeControl->SetTooltip(
      "Raw: no adjustment. Normalized: consistent loudness. Calibrated: match input calibration.");

    auto* diagnostics = addPage(amphibia::ui::SettingsSection::Diagnostics);
    diagnostics->AddChildControl(new IVLabelControl(row(0), "DIAGNOSTICS", headingStyle));
    mModelInfo = new ModelInfoControl(IRECT(content.L, row(1).T, content.R, row(5).B), labelStyle);
    diagnostics->AddChildControl(mModelInfo);

    auto* about = addPage(amphibia::ui::SettingsSection::About);
    about->AddChildControl(new IVLabelControl(row(0), "ABOUT", headingStyle));
    about->AddChildControl(new AboutControl(IRECT(content.L, row(1).T, content.R, content.B),
                                            labelStyle, leftText));

    AddChildControl(new IVLabelControl(rect(calculated.footer),
      "Choose one section. Inactive sections are hidden and cannot receive input.",
      mStyle.WithDrawFrame(false).WithValueText(IText(10.f, EAlign::Near, PluginColors::HELP_TEXT))));
    AddNamedChildControl(new NAMSquareButtonControl(rect(calculated.close), [this](IControl*) {
      HideAnimated(true);
    }, mCloseSVG), mControlNames.close);

    RefreshFolderInfo();
    RefreshLibraryInfo();
    RefreshProviderInfo();
    ApplySectionVisibility();
    OnResize();
  }

  void SetModelInfo(const ModelInfo& modelInfo)
  {
    if (mModelInfo) mModelInfo->SetModelInfo(modelInfo);
  };

private:
  void ApplySectionVisibility()
  {
    const auto visible = amphibia::ui::CalculateSettingsPageVisibility(mActiveSection);
    for (std::size_t index = 0; index < mPages.size(); ++index)
    {
      if (!mPages[index]) continue;
      const bool inactive = !visible[index];
      mPages[index]->SetDisabled(inactive);
      mPages[index]->Hide(inactive);
    }
    if (GetUI()) GetUI()->SetAllControlsDirty();
  }

  IBitmap mBitmap;
  IBitmap mInputLevelBackgroundBitmap;
  IBitmap mSwitchBitmap;
  IVStyle mStyle;
  IVStyle mRadioButtonStyle;
  ISVG mCloseSVG;
  LocalFolderUIActions mFolderActions;
  LibraryUIActions mLibraryActions;
  RemoteProviderUIActions mProviderActions;
  std::array<IContainerBase*, static_cast<std::size_t>(amphibia::ui::SettingsSection::Count)> mPages{};
  amphibia::ui::SettingsSection mActiveSection{amphibia::ui::SettingsSection::General};
  ModelInfoControl* mModelInfo{};
  IVLabelControl* mFolderInfo{};
  IVLabelControl* mLibraryInfo{};
  IVLabelControl* mProviderInfo{};
  int mAnimationTime = 200;
  bool mWillHide = false;

  // Names for controls
  // Make sure that these are all unique and that you use them with AddNamedChildControl
  struct ControlNames
  {
    const std::string about = "About";
    const std::string bitmap = "Bitmap";
    const std::string calibrateInput = "CalibrateInput";
    const std::string close = "Close";
    const std::string inputCalibrationLevel = "InputCalibrationLevel";
    const std::string modelInfo = "ModelInfo";
    const std::string libraryInfo = "LibraryInfo";
    const std::string outputMode = "OutputMode";
    const std::string providerInfo = "ProviderInfo";
    const std::string title = "Title";
  } mControlNames;

  class InputLevelControl : public IEditableTextControl
  {
  public:
    InputLevelControl(const IRECT& bounds, int paramIdx, const IBitmap& bitmap, const IText& text = DEFAULT_TEXT,
                      const IColor& BGColor = DEFAULT_BGCOLOR)
    : IEditableTextControl(bounds, "", text, BGColor)
    , mBitmap(bitmap)
    {
      SetParamIdx(paramIdx);
    };

    void Draw(IGraphics& g) override
    {
      g.DrawFittedBitmap(mBitmap, mRECT);
      ITextControl::Draw(g);
    };

    void SetValueFromUserInput(double normalizedValue, int valIdx) override
    {
      IControl::SetValueFromUserInput(normalizedValue, valIdx);
      const std::string s = ConvertToString(normalizedValue);
      OnTextEntryCompletion(s.c_str(), valIdx);
    };

    void SetValueFromDelegate(double normalizedValue, int valIdx) override
    {
      IControl::SetValueFromDelegate(normalizedValue, valIdx);
      const std::string s = ConvertToString(normalizedValue);
      SetStr(s.c_str());
      SetDirty(false);
    };

  private:
    std::string ConvertToString(const double normalizedValue)
    {
      const double naturalValue = GetParam()->FromNormalized(normalizedValue);
      // And make the value to display
      std::stringstream ss;
      ss << naturalValue << " dBu";
      std::string s = ss.str();
      return s;
    };

    IBitmap mBitmap;
  };

  class AboutControl : public IContainerBase
  {
  public:
    AboutControl(const IRECT& bounds, const IVStyle& style, const IText& text)
    : IContainerBase(bounds)
    , mStyle(style)
    , mText(text) {};

    void OnAttached() override
    {
      WDL_String verStr, buildInfoStr;
      PLUG()->GetPluginVersionStr(verStr);

      buildInfoStr.SetFormatted(160, "%s-%s+%s %s %s", verStr.Get(), AMPHIBIA_BUILD_TYPE, AMPHIBIA_BUILD_COMMIT,
                               PLUG()->GetArchStr(), PLUG()->GetAPIStr());

      AddChildControl(new IVLabelControl(GetRECT().SubRectVertical(7, 0), "AMPHIBIA", mStyle));
      AddChildControl(
        new IVLabelControl(GetRECT().SubRectVertical(7, 1), "Independent, free, open-source NAM player", mStyle));
      AddChildControl(new IVLabelControl(GetRECT().SubRectVertical(7, 2),
                                         "Not the official Neural Amp Modeler application", mStyle));
      AddChildControl(new IVLabelControl(GetRECT().SubRectVertical(7, 3), buildInfoStr.Get(), mStyle));
      AddChildControl(new IURLControl(GetRECT().SubRectVertical(7, 4),
                                      "Derived from NeuralAmpModelerPlugin 0.7.15",
                                      "https://github.com/sdatkinson/NeuralAmpModelerPlugin", mText,
                                      COLOR_TRANSPARENT, PluginColors::HELP_TEXT_MO, PluginColors::HELP_TEXT_CLICKED));
      AddChildControl(new IVLabelControl(GetRECT().SubRectVertical(7, 5),
                                         "NAM Core 0.5.3 | iPlug2 66f9060", mStyle));
      AddChildControl(new ThirdPartyNoticesControl(GetRECT().SubRectVertical(7, 6), mText));
    };

  private:
    class ThirdPartyNoticesControl : public IURLControl
    {
    public:
      ThirdPartyNoticesControl(const IRECT& bounds, const IText& text)
      : IURLControl(bounds, "License and third-party notices", "", text, COLOR_TRANSPARENT,
                    PluginColors::HELP_TEXT_MO,
                    PluginColors::HELP_TEXT_CLICKED)
      {
      }

      void OnMouseDown(float x, float y, const IMouseMod& mod) override
      {
        WDL_String path;
        bool opened = false;

        if (ResolveNoticesPath(GetUI(), path))
          opened = OpenNoticesPath(GetUI(), path);

        if (!opened)
          ShowOpenError(GetUI());

        GetUI()->ReleaseMouseCapture();
        mClicked = true;
        SetDirty(false);
      }

    private:
      static bool FileExists(const WDL_String& path)
      {
        if (!CStringHasContents(path.Get()))
          return false;

        FILE* file = WDL_fopenA(path.Get(), "rb");
        if (file == nullptr)
          return false;

        fclose(file);
        return true;
      }

      static bool TryNoticePathInDirectory(WDL_String& result, const WDL_String& directory)
      {
        if (!CStringHasContents(directory.Get()))
          return false;

        WDL_String candidate(directory);
        const char lastChar = candidate.Get()[candidate.GetLength() - 1];

        if (!WDL_IS_DIRCHAR(lastChar))
          candidate.Append(WDL_DIRCHAR_STR);

        candidate.Append(kNoticesFileName);

        if (!FileExists(candidate))
          return false;

        result.Set(candidate.Get());
        return true;
      }

      // AAX (and similar) load the binary from Contents\x64 or Contents\Win32 while notices live in
      // Contents\Resources. Same layout as a VST3 bundle; this path is not covered by BundleResourcePath
      // when the plug-in is built as AAX_API only (no VST3_API).
      static bool TryNoticePathSiblingResources(WDL_String& result, const WDL_String& moduleDirectory)
      {
        if (!CStringHasContents(moduleDirectory.Get()))
          return false;

        WDL_String candidate(moduleDirectory);
        const char lastChar = candidate.Get()[candidate.GetLength() - 1];
        if (!WDL_IS_DIRCHAR(lastChar))
          candidate.Append(WDL_DIRCHAR_STR);

        candidate.Append("..");
        candidate.Append(WDL_DIRCHAR_STR);
        candidate.Append("Resources");
        candidate.Append(WDL_DIRCHAR_STR);
        candidate.Append(kNoticesFileName);

        if (!FileExists(candidate))
          return false;

        result.Set(candidate.Get());
        return true;
      }

      static bool ResolveNoticesPath(IGraphics* pGraphics, WDL_String& path)
      {
        path.Set("");

        if (pGraphics == nullptr)
          return false;

#ifdef OS_WIN
        WDL_String directory;
        const auto moduleHandle = static_cast<PluginIDType>(pGraphics->GetWinModuleHandle());

        BundleResourcePath(directory, moduleHandle);
        if (TryNoticePathInDirectory(path, directory))
          return true;

        directory.Set("");
        PluginPath(directory, moduleHandle);
        if (TryNoticePathInDirectory(path, directory))
          return true;

        if (TryNoticePathSiblingResources(path, directory))
          return true;
#endif

        const auto resourceLocation =
          LocateResource(kNoticesFileName, "txt", path, pGraphics->GetBundleID(), pGraphics->GetWinModuleHandle(),
                         pGraphics->GetSharedResourcesSubPath());

        return resourceLocation == EResourceLocation::kAbsolutePath && FileExists(path);
      }

      static bool OpenNoticesPath(IGraphics* pGraphics, const WDL_String& path)
      {
        if (pGraphics == nullptr || !CStringHasContents(path.Get()))
          return false;

#ifdef OS_WIN
        WCHAR pathWide[IPLUG_WIN_MAX_WIDE_PATH];
        UTF8ToUTF16(pathWide, path.Get(), IPLUG_WIN_MAX_WIDE_PATH);

        if (pathWide[0] == 0)
          return false;

        WCHAR canon[IPLUG_WIN_MAX_WIDE_PATH];
        const DWORD nCanon = GetFullPathNameW(pathWide, IPLUG_WIN_MAX_WIDE_PATH, canon, nullptr);
        const WCHAR* const launchPath = (nCanon > 0 && nCanon < IPLUG_WIN_MAX_WIDE_PATH) ? canon : pathWide;

        return ShellExecuteW(nullptr, L"open", launchPath, nullptr, nullptr, SW_SHOWNORMAL) > HINSTANCE(32);
#else
        return pGraphics->OpenURL(path.Get());
#endif
      }

      static void ShowOpenError(IGraphics* pGraphics)
      {
        if (pGraphics == nullptr)
          return;

        const char* const title = "Third party notices";
        const char* const message = "Could not open ThirdPartyNotices.txt.";

#ifdef OS_MAC
        pGraphics->ShowMessageBox(title, message, kMB_OK);
#else
        pGraphics->ShowMessageBox(message, title, kMB_OK);
#endif
      }

      static constexpr const char* kNoticesFileName = "ThirdPartyNotices.txt";
    };

    IVStyle mStyle;
    IText mText;
  };
};
