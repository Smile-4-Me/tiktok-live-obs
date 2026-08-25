// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#pragma once

#include <QAction>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QPalette>
#include <QPaintEvent>
#include <QPainter>
#include <QPushButton>
#include <QSettings>
#include <QSizePolicy>
#include <QSize>
#include <QString>
#include <QToolBar>
#include <QToolButton>
#include <QStyle>
#include <QStyleOptionButton>
#include <QStylePainter>

#include "plugin_paths.hpp"

namespace obs_button_style {

// OBS docks use a 32 px toolbar with 16 px icons. The generated action button
// remains a compact square inside that toolbar, as in the Scenes dock.
inline constexpr int toolbar_height = 32;
// A compact OBS toolbar action occupies the complete 32 px cell. The native
// theme paints its one-pixel frame inside that area, matching the Hotkeys and
// Scenes tool buttons.
inline constexpr int compact_button_size = toolbar_height;
inline constexpr int text_button_height = 28;
inline constexpr int compact_icon_size = 16;

inline QString obs_root_directory()
{
	QDir directory(QCoreApplication::applicationDirPath());
	if (!directory.cdUp() || !directory.cdUp())
		return {};
	return directory.absolutePath();
}

inline QString configured_theme_directory()
{
	const QString obs_root = obs_root_directory();
	const QString user_ini = QDir(obs_root).filePath(QStringLiteral("config/obs-studio/user.ini"));
	QSettings settings(user_ini, QSettings::IniFormat);
	const QString theme = settings.value(QStringLiteral("General/Theme")).toString();
	if (theme.contains(QStringLiteral("aitum"), Qt::CaseInsensitive))
		return QStringLiteral("Aitum");
	if (theme.contains(QStringLiteral("light"), Qt::CaseInsensitive))
		return QStringLiteral("Light");
	return QStringLiteral("Dark");
}

// Reuse OBS' shipped SVG files instead of drawing approximations in this plugin.
// A custom or unavailable theme falls back to the regular OBS dark/light assets.
inline QIcon obs_theme_icon(const QString &file_name, const QPalette &palette)
{
	const QDir themes(QDir(obs_root_directory()).filePath(QStringLiteral("data/obs-studio/themes")));
	QStringList theme_directories{configured_theme_directory()};
	const bool light_palette = palette.color(QPalette::Window).lightness() > 128;
	theme_directories.append(light_palette ? QStringLiteral("Light") : QStringLiteral("Dark"));
	theme_directories.append(QStringLiteral("Dark"));

	for (const QString &theme : theme_directories) {
		const QString path = themes.filePath(theme + QLatin1Char('/') + file_name);
		if (QFileInfo::exists(path))
			return QIcon(path);
	}
	return {};
}

// Some actions intentionally use a project-owned asset. It is installed under
// the module data directory together with all locales, so portable OBS copies
// retain the exact same asset without depending on a user-specific path.
inline QIcon plugin_asset_icon(const QString &file_name)
{
	const QString path = QDir(module_data_directory()).filePath(
		QStringLiteral("assets/") + file_name);
	return QFileInfo::exists(path) ? QIcon(path) : QIcon{};
}

// QPushButton centres ordinary icons vertically. For the destructive action we
// instead align the custom trash icon to the text baseline: its bottom meets
// the baseline and its top reaches the ascender height of a lowercase "l".
// The button background, hover state and focus frame still come from OBS'
// active Qt theme.
class BaselineIconButton final : public QPushButton {
public:
	BaselineIconButton(const QIcon &icon, const QString &label, QWidget *parent)
		: QPushButton(label, parent), icon_(icon)
	{
	}

	QSize sizeHint() const override
	{
		QSize hint = QPushButton::sizeHint();
		const QFontMetrics metrics(font());
		const int icon_width = baseline_icon_width(metrics);
		// QPushButton's base hint knows the text and theme padding. Add only
		// the custom baseline icon and its small text gap.
		hint.rwidth() += icon_width + icon_text_gap;
		return hint;
	}

protected:
	void paintEvent(QPaintEvent *event) override
	{
		Q_UNUSED(event);
		QStyleOptionButton option;
		initStyleOption(&option);
		option.text.clear();
		option.icon = QIcon{};
		option.iconSize = QSize{};

		QStylePainter painter(this);
		painter.drawControl(QStyle::CE_PushButton, option);

		const QRect contents = style()->subElementRect(
			QStyle::SE_PushButtonContents, &option, this);
		const QFontMetrics metrics(font());
		const int icon_height = qMax(1, metrics.ascent());
		const int icon_width = baseline_icon_width(metrics);
		const int text_width = metrics.horizontalAdvance(text());
		const int total_width = icon_width + icon_text_gap + text_width;
		const int start_x = contents.x() + qMax(0, (contents.width() - total_width) / 2);
		const int baseline = contents.y() + (contents.height() - metrics.height()) / 2 +
			metrics.ascent();

		icon_.paint(&painter, QRect(start_x, baseline - icon_height, icon_width, icon_height),
			Qt::AlignCenter, isEnabled() ? QIcon::Normal : QIcon::Disabled);
		painter.setPen(option.palette.color(QPalette::ButtonText));
		painter.setFont(font());
		painter.drawText(start_x + icon_width + icon_text_gap, baseline, text());
	}

private:
	// The visual glyph is intentionally 20% wider than the previous 110%-wide
	// treatment. Its height remains locked to the font ascender, so the icon
	// starts at the text baseline and reaches the lowercase ascender height.
	static int baseline_icon_width(const QFontMetrics &metrics)
	{
		return qRound(qMax(1, metrics.ascent()) * 1.32);
	}

	// Native OBS buttons use a very tight icon/text relationship. One pixel
	// less than the regular four-pixel Qt spacing keeps the custom asset optically
	// aligned with the profile-edit button without making the label collide.
	inline static constexpr int icon_text_gap = 3;

	QIcon icon_;
};

// This is intentionally a QPushButton, not a nested QToolBar. OBS themes
// style QPushButton[toolButton="true"] as the native compact control used by
// the Hotkeys dialog. Nested toolbars inherit footer padding from themes such
// as Aitum and can therefore distort a one-action button.
inline QPushButton *create_native_button(QWidget *parent, const QIcon &icon,
	const QString &label, bool compact)
{
	auto *button = new QPushButton(icon, compact ? QString{} : label, parent);
	button->setProperty("toolButton", true);
	button->setToolTip(label);
	button->setAccessibleName(label);
	button->setIconSize(QSize(compact_icon_size, compact_icon_size));
	button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	if (compact) {
		button->setFixedSize(compact_button_size, compact_button_size);
	} else {
		button->setFixedHeight(text_button_height);
		button->setMinimumWidth(0);
	}
	return button;
}

inline BaselineIconButton *create_baseline_icon_button(QWidget *parent,
	const QIcon &icon, const QString &label)
{
	auto *button = new BaselineIconButton(icon, label, parent);
	button->setProperty("toolButton", true);
	button->setToolTip(label);
	button->setAccessibleName(label);
	button->setFixedHeight(text_button_height);
	button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	return button;
}

inline QToolBar *create_toolbar(QWidget *parent, Qt::ToolButtonStyle style)
{
	auto *toolbar = new QToolBar(parent);
	toolbar->setFloatable(false);
	toolbar->setMovable(false);
	toolbar->setIconSize(QSize(compact_icon_size, compact_icon_size));
	toolbar->setToolButtonStyle(style);
	toolbar->setContentsMargins(0, 0, 0, 0);
	// Aitum's global QToolBar rule adds horizontal and bottom padding for dock
	// footers. That is appropriate for the Scenes toolbar, but it shrinks an
	// embedded one-action toolbar down to the glyph. Override only the wrapper
	// geometry so the underlying native QToolButton style remains intact.
	toolbar->setStyleSheet(QStringLiteral(
		"QToolBar { background: transparent; border: none; margin: 0px; padding: 0px; spacing: 0px; }"
		"QToolBar QToolButton { margin: 0px; }"));
	toolbar->setFixedHeight(style == Qt::ToolButtonIconOnly ? toolbar_height : text_button_height);
	toolbar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	return toolbar;
}

inline QAction *add_toolbar_action(QToolBar *toolbar, const QIcon &icon,
	const QString &label, bool compact)
{
	auto *action = toolbar->addAction(icon, label);
	action->setToolTip(label);
	action->setIconText(label);

	if (auto *button = qobject_cast<QToolButton *>(toolbar->widgetForAction(action))) {
		button->setToolTip(label);
		button->setAccessibleName(label);
		button->setAutoRaise(false);
		if (compact) {
			// The Scenes dock reserves a full 32 px toolbar cell for each
			// compact action. Without this width, a plugin toolbar collapses to
			// the 16 px glyph instead of presenting the native square target.
			toolbar->setFixedSize(toolbar_height, toolbar_height);
			button->setFixedSize(compact_button_size, compact_button_size);
		} else {
			button->setFixedHeight(text_button_height);
		}
	}
	return action;
}

} // namespace obs_button_style
