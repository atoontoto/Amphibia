#pragma once

#include "../Providers/ProviderTypes.h"

namespace amphibia::ui {

// Terminal Cancelled/Failed states draw Close instead of selection controls.
// Keep this predicate shared by drawing and input dispatch so that a rendered
// Close button is always actionable. Active loading states may not yet have a
// tone, but retain Cancel rather than being treated as terminal.
inline bool IsTone3000SelectionPanelCloseState(
  const providers::HostedSelectionSnapshot& snapshot) noexcept
{
  return snapshot.stage == providers::HostedSelectionStage::Failed ||
    snapshot.stage == providers::HostedSelectionStage::Cancelled;
}

} // namespace amphibia::ui
