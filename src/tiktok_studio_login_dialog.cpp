// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#include "tiktok_studio_login_dialog.hpp"

#include "localization.hpp"

#include <QCloseEvent>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPixmap>
#include <QPointer>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

TikTokStudioLoginDialog::TikTokStudioLoginDialog(TikTokStudioClient *client,
	TikTokStudioAccountCredentials account, QWidget *parent)
	: QDialog(parent), client_(client), account_(std::move(account))
{
	setWindowTitle(translated_or("Studio.Login.Title", QStringLiteral("Sign in to TikTok LIVE Studio")));
	setModal(true);
	setMinimumWidth(400);

	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(20, 18, 20, 16);
	layout->setSpacing(12);

	instructions_ = new QLabel(translated_or("Studio.Login.QrGuide", QStringLiteral(
		"<b>Scan the code to sign in</b><br/><br/>"
		"Use your phone camera to scan the QR code. Or open TikTok and go to:<br/><br/>"
		"<b>\"TikTok app\" → \"Profile\" → ☰ → \"My QR code\" → ⌗</b>")), this);
	instructions_->setWordWrap(true);
	instructions_->setTextFormat(Qt::RichText);
	instructions_->setStyleSheet(QStringLiteral("QLabel { color: palette(text); font-size: 14px; }"));

	qr_code_ = new QLabel(this);
	qr_code_->setAlignment(Qt::AlignCenter);
	qr_code_->setFixedSize(220, 220);
	qr_code_->setStyleSheet(QStringLiteral(
		"QLabel { background: white; border: 1px solid palette(mid); border-radius: 12px; padding: 12px; }"));
	layout->addWidget(qr_code_, 0, Qt::AlignHCenter);
	layout->addWidget(instructions_);

	status_ = new QLabel(this);
	status_->setAlignment(Qt::AlignCenter);
	status_->setWordWrap(true);
	status_->setOpenExternalLinks(true);
	status_->setVisible(false);
	layout->addWidget(status_);

	retry_ = new QPushButton(translated_or("Studio.Login.NewCode", QStringLiteral("Generate a new QR code")), this);
	retry_->setVisible(false);
	connect(retry_, &QPushButton::clicked, this, [this] { start_login(); });
	layout->addWidget(retry_);

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
	close_button_ = buttons->button(QDialogButtonBox::Cancel);
	close_button_->setText(translated_or("Studio.Login.Cancel", QStringLiteral("Cancel")));
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	layout->addWidget(buttons);

	poll_timer_ = new QTimer(this);
	poll_timer_->setInterval(2500);
	connect(poll_timer_, &QTimer::timeout, this, [this] { poll_login(); });
	QTimer::singleShot(0, this, [this] { start_login(); });
}

TikTokStudioLoginDialog::~TikTokStudioLoginDialog()
{
	cancel_login();
}

std::optional<TikTokStudioQrPoll> TikTokStudioLoginDialog::result() const
{
	return result_;
}

TikTokStudioAccountCredentials TikTokStudioLoginDialog::account() const
{
	return account_;
}

void TikTokStudioLoginDialog::closeEvent(QCloseEvent *event)
{
	cancel_login();
	QDialog::closeEvent(event);
}

void TikTokStudioLoginDialog::start_login()
{
	if (!client_ || account_.rapidapi_key.trimmed().isEmpty()) {
		show_retry(translated_or("Studio.Login.MissingKey",
			QStringLiteral("Enter a RapidAPI key before signing in.")));
		return;
	}

	cancel_login();
	closing_ = false;
	result_.reset();
	poll_in_flight_ = false;
	retry_->setVisible(false);
	close_button_->setText(translated_or("Studio.Login.Cancel", QStringLiteral("Cancel")));
	instructions_->setVisible(true);
	instructions_->setText(translated_or("Studio.Login.QrGuide", QStringLiteral(
		"<b>Scan the code to sign in</b><br/><br/>"
		"Use your phone camera to scan the QR code. Or open TikTok and go to:<br/><br/>"
		"<b>\"TikTok app\" → \"Profile\" → ☰ → \"My QR code\" → ⌗</b>")));
	qr_code_->setVisible(true);
	qr_code_->clear();
	qr_code_->setText(translated_or("Studio.Login.PreparingDevice",
		QStringLiteral("Preparing this device…")));
	status_->clear();
	status_->setVisible(false);

	QPointer<TikTokStudioLoginDialog> guard(this);
	client_->begin_qr_login(account_, [guard](TikTokStudioQrCode code, QString error) {
		if (!guard || guard->closing_)
			return;
		// Device registration and response cookies are useful even when the QR
		// request itself fails; retain them for the next attempt.
		if (code.account.has_device())
			guard->account_ = code.account;
		if (!error.isEmpty()) {
			guard->show_retry(error);
			return;
		}
		QPixmap image;
		if (!image.loadFromData(code.png, "PNG")) {
			guard->show_retry(translated_or("Studio.Login.InvalidCode",
				QStringLiteral("TikTok returned an unreadable QR code.")), true);
			return;
		}
		guard->account_ = std::move(code.account);
		guard->qr_code_->setPixmap(image.scaled(guard->qr_code_->contentsRect().size(),
			Qt::KeepAspectRatio, Qt::SmoothTransformation));
		guard->status_->clear();
		guard->status_->setVisible(false);
		guard->poll_timer_->start();
	});
}

void TikTokStudioLoginDialog::poll_login()
{
	if (!client_ || poll_in_flight_ || closing_)
		return;
	poll_in_flight_ = true;
	QPointer<TikTokStudioLoginDialog> guard(this);
	client_->poll_qr_login([guard](TikTokStudioQrPoll poll, QString error) {
		if (!guard || guard->closing_)
			return;
		guard->poll_in_flight_ = false;
		if (poll.account.has_device())
			guard->account_ = poll.account;
		if (!error.isEmpty()) {
			guard->show_retry(error);
			return;
		}
		switch (poll.state) {
		case TikTokStudioQrState::Waiting:
			guard->status_->clear();
			guard->status_->setVisible(false);
			break;
		case TikTokStudioQrState::Scanned:
			guard->status_->setText(translated_or("Studio.Login.ConfirmPhone",
				QStringLiteral("Code scanned. Confirm the login on your phone.")));
			guard->status_->setVisible(true);
			break;
		case TikTokStudioQrState::Expired:
			guard->show_retry(translated_or("Studio.Login.Expired",
				QStringLiteral("This QR code expired. Generate a new one to continue.")), true);
			break;
		case TikTokStudioQrState::Confirmed:
			guard->poll_timer_->stop();
			guard->result_ = std::move(poll);
			guard->accept();
			break;
		}
	});
}

void TikTokStudioLoginDialog::cancel_login()
{
	closing_ = true;
	if (poll_timer_)
		poll_timer_->stop();
	poll_in_flight_ = false;
	if (client_)
		client_->cancel_qr_login();
}

void TikTokStudioLoginDialog::show_retry(const QString &message, bool can_generate_new_code)
{
	if (poll_timer_)
		poll_timer_->stop();
	qr_code_->clear();
	// A failed request has no QR code to scan. Hiding this fixed-size widget
	// keeps the retry state compact instead of showing an empty white square.
	qr_code_->setVisible(false);
	instructions_->setVisible(false);
	if (can_generate_new_code) {
		status_->setText(message);
	} else {
		const QString help = translated_or("Studio.Login.RapidApiErrorHelp", QStringLiteral(
			"<a href=\"https://rapidapi.com/Loukious/api/tiktok-live-studio-api-signer1\">Open the RapidAPI signer page</a>"));
		status_->setText(QStringLiteral("<p>%1</p><p>%2</p>").arg(message.toHtmlEscaped(), help));
	}
	status_->setVisible(true);
	// Only a QR code that was created but then expired or could not be decoded
	// can be generated again within this dialog. Request/API errors must first
	// be resolved outside the dialog, so do not present a misleading retry.
	retry_->setVisible(can_generate_new_code);
	close_button_->setText(translated_or("Studio.Login.Close", QStringLiteral("Close")));
}
