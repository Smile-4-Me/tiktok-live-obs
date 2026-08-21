// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Bridge Contributors

#include "profile_row.hpp"
#include "localization.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

namespace {

QString state_color(ProfileState state)
{
	switch (state) {
	case ProfileState::NeedsLogin: return QStringLiteral("#e05d5d");
	case ProfileState::AwaitingLiveAccess: return QStringLiteral("#e3a341");
	case ProfileState::Ready: return QStringLiteral("#8c939d");
	case ProfileState::SessionUncertain: return QStringLiteral("#e3a341");
	case ProfileState::Live: return QStringLiteral("#62c370");
	}
	return QStringLiteral("#8c939d");
}

QString state_tooltip(ProfileState state)
{
	switch (state) {
	case ProfileState::NeedsLogin: return text("Status.NeedsLogin");
	case ProfileState::AwaitingLiveAccess: return text("Status.AwaitingLiveAccess");
	case ProfileState::Ready: return text("Status.Ready");
	case ProfileState::SessionUncertain: return text("Status.SessionUncertain");
	case ProfileState::Live: return text("Status.Live");
	}
	return {};
}

} // namespace

ProfileRow::ProfileRow(QWidget *parent) : QWidget(parent)
{
	setFixedHeight(kHeight);
	auto *layout = new QHBoxLayout(this);
	layout->setContentsMargins(8, 5, 8, 5);
	layout->setSpacing(7);
	button_ = new QPushButton(this);
	button_->setFlat(true);
	button_->setMinimumHeight(32);
	layout->addWidget(button_, 1);
	dot_ = new QLabel(this);
	dot_->setFixedSize(14, 14);
	dot_->setToolTipDuration(10000);
	layout->addWidget(dot_);
	connect(button_, &QPushButton::clicked, this, [this] {
		if (on_clicked)
			on_clicked();
	});
}

void ProfileRow::set_profile(const Profile &profile, bool selected)
{
	QString line = profile.display_name;
	if (!profile.tiktok_username.isEmpty())
		line += QStringLiteral(" (@%1)").arg(profile.tiktok_username);
	if (!profile.output_name.isEmpty())
		line += QStringLiteral("  |  %1").arg(profile.output_name);
	button_->setText(line);
	button_->setStyleSheet(QStringLiteral(
		"QPushButton { text-align: left; border: 0; padding: 4px; border-radius: 3px; %1 }"
	).arg(selected ? QStringLiteral("background: #38465f;") : QStringLiteral("")));
	const ProfileState state = profile.state();
	dot_->setStyleSheet(QStringLiteral(
		"QLabel { background-color: %1; border: 1px solid #d8dee9; border-radius: 7px; "
		"min-width: 14px; min-height: 14px; max-width: 14px; max-height: 14px; }")
		.arg(state_color(state)));
	dot_->setToolTip(state_tooltip(state));
}
