// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Bridge Contributors

#pragma once

#include "profile.hpp"

#include <QWidget>

#include <functional>

class QLabel;
class QPushButton;

// A compact, reusable profile selector row for the dock's account overview.
class ProfileRow final : public QWidget {
public:
	// Keep selector rows predictable so the list can show five full profiles
	// before scrolling is needed.
	static constexpr int kHeight = 44;

	explicit ProfileRow(QWidget *parent = nullptr);

	void set_profile(const Profile &profile, bool selected);
	std::function<void()> on_clicked;

private:
	QPushButton *button_ = nullptr;
	QLabel *dot_ = nullptr;
};
