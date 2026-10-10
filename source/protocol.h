#pragma once
#include <windows.h>
#define TBS_MAGIC 0x32534254u
enum { TBS_APPLY=1, TBS_DISABLE=2, TBS_STATUS=3 };
typedef struct {
    DWORD magic, action, showPercent, hidePercent;
    DWORD result, enabled, initialized, legacyCalls, expandCalls, collapseCalls;
    LONGLONG originalShow, currentShow, originalHide, currentHide;
    DWORD legacyShowOriginalMs, legacyShowTargetMs, legacyHideOriginalMs, legacyHideTargetMs;
    LONGLONG legacyShowElapsedUs, legacyHideElapsedUs;
    DWORD hideDelayPercent, hideTimerCalls, originalHideDelayMs, currentHideDelayMs;
} Control;
