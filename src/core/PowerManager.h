#pragma once

// Prevents the machine from sleeping while idling (mirrors the original
// SetThreadExecutionState usage) and exposes explicit sleep/hibernate helpers
// used by the "when idling completes" action.
namespace PowerManager {

void preventSleep();
void allowSleep();

// Suspends the machine to RAM. Returns false when the request failed.
bool sleep();

// Hibernates the machine (falls back to sleep when hibernation is disabled).
bool hibernate();

} // namespace PowerManager
