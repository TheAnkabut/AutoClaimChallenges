#pragma once

namespace PopupDismiss
{
    void Initialize(std::shared_ptr<GameWrapper> wrapper);
    void BlockPopups();    // Disable popup creation by zeroing function flags
    void RestorePopups();  // Restore flags
}
