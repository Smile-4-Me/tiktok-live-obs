// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Bridge Contributors

#pragma once

#include <QHash>
#include <QStringList>

// Returns true only when the Aitum Stream Suite module is loaded in OBS.
bool aitum_stream_suite_available();

// Reads Aitum's published obs-websocket vendor request `get_outputs`.
// It never opens Aitum UI and never changes its configuration.
// When types is provided, it receives Aitum's output type for each name
// (for example stream, record, or ffmpeg).
QStringList aitum_output_names(QString *diagnostic = nullptr, QHash<QString, QString> *types = nullptr,
	QHash<QString, bool> *active_states = nullptr);

// Returns the actual OBS output state published by Aitum. This is deliberately
// separate from `aitum_start_output()`: a successful start request only means
// Aitum accepted the action, while this confirms whether the encoder/output
// genuinely became active.
bool aitum_output_is_active(const QString &output_name, bool *active, QString *diagnostic = nullptr);

// Starts one Aitum output through its published obs-websocket vendor request.
// This follows the same output-specific action that Aitum exposes to external
// integrations and does not depend on screen coordinates or mouse injection.
bool aitum_start_output(const QString &output_name, QString *diagnostic = nullptr);
