// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#include "output_signing_manager.hpp"
#include "native_platform.hpp"

#include <QDateTime>
#include <QMetaObject>
#include <QPointer>
#include <QTimer>

#include <obs.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <future>
#include <limits>
#include <optional>
#include <utility>

namespace {

using get_output_by_name_fn = obs_output_t *(*)(const char *);
using enum_outputs_fn = void (*)(bool (*)(void *, obs_output_t *), void *);
using output_get_ref_fn = obs_output_t *(*)(obs_output_t *);
using output_release_fn = void (*)(obs_output_t *);
using output_get_name_fn = const char *(*)(const obs_output_t *);
using add_packet_callback_fn = void (*)(obs_output_t *,
	void (*)(obs_output_t *, encoder_packet *, encoder_packet_time *, void *), void *);
using remove_packet_callback_fn = void (*)(obs_output_t *,
	void (*)(obs_output_t *, encoder_packet *, encoder_packet_time *, void *), void *);
using packet_release_fn = void (*)(encoder_packet *);
using obs_allocate_fn = void *(*)(size_t);
using encoder_get_codec_fn = const char *(*)(const obs_encoder_t *);
using encoder_get_width_fn = uint32_t (*)(const obs_encoder_t *);
using encoder_get_height_fn = uint32_t (*)(const obs_encoder_t *);
using output_set_last_error_fn = void (*)(obs_output_t *, const char *);
using output_signal_stop_fn = void (*)(obs_output_t *, int);
using frontend_get_streaming_output_fn = obs_output_t *(*)();
using frontend_add_event_callback_fn = void (*)(obs_frontend_event_cb, void *);
using frontend_remove_event_callback_fn = void (*)(obs_frontend_event_cb, void *);

const QString &main_stream_key()
{
	static const QString key = QStringLiteral("@obs-main-stream");
	return key;
}

QString normalized_output_key(const QString &name)
{
	return name.trimmed().isEmpty() ? main_stream_key() : name;
}

struct ObsPacketApi {
	get_output_by_name_fn get_output_by_name = nullptr;
	enum_outputs_fn enum_outputs = nullptr;
	output_get_ref_fn output_get_ref = nullptr;
	output_release_fn output_release = nullptr;
	output_get_name_fn output_get_name = nullptr;
	add_packet_callback_fn add_packet_callback = nullptr;
	remove_packet_callback_fn remove_packet_callback = nullptr;
	packet_release_fn packet_release = nullptr;
	obs_allocate_fn allocate = nullptr;
	encoder_get_codec_fn encoder_get_codec = nullptr;
	encoder_get_width_fn encoder_get_width = nullptr;
	encoder_get_height_fn encoder_get_height = nullptr;
	output_set_last_error_fn output_set_last_error = nullptr;
	output_signal_stop_fn output_signal_stop = nullptr;
	frontend_get_streaming_output_fn frontend_get_streaming_output = nullptr;
	frontend_add_event_callback_fn frontend_add_event_callback = nullptr;
	frontend_remove_event_callback_fn frontend_remove_event_callback = nullptr;

	ObsPacketApi()
	{
#define LOAD_OBS(member, name) member = reinterpret_cast<decltype(member)>(resolve_native_symbol(NativeLibrary::Obs, name))
		LOAD_OBS(get_output_by_name, "obs_get_output_by_name");
		LOAD_OBS(enum_outputs, "obs_enum_outputs");
		LOAD_OBS(output_get_ref, "obs_output_get_ref");
		LOAD_OBS(output_release, "obs_output_release");
		LOAD_OBS(output_get_name, "obs_output_get_name");
		LOAD_OBS(add_packet_callback, "obs_output_add_packet_callback");
		LOAD_OBS(remove_packet_callback, "obs_output_remove_packet_callback");
		LOAD_OBS(packet_release, "obs_encoder_packet_release");
		LOAD_OBS(allocate, "bmalloc");
		LOAD_OBS(encoder_get_codec, "obs_encoder_get_codec");
		LOAD_OBS(encoder_get_width, "obs_encoder_get_width");
		LOAD_OBS(encoder_get_height, "obs_encoder_get_height");
		LOAD_OBS(output_set_last_error, "obs_output_set_last_error");
		LOAD_OBS(output_signal_stop, "obs_output_signal_stop");
#undef LOAD_OBS
		frontend_get_streaming_output = reinterpret_cast<frontend_get_streaming_output_fn>(
			resolve_native_symbol(NativeLibrary::Frontend, "obs_frontend_get_streaming_output"));
		frontend_add_event_callback = reinterpret_cast<frontend_add_event_callback_fn>(
			resolve_native_symbol(NativeLibrary::Frontend, "obs_frontend_add_event_callback"));
		frontend_remove_event_callback = reinterpret_cast<frontend_remove_event_callback_fn>(
			resolve_native_symbol(NativeLibrary::Frontend, "obs_frontend_remove_event_callback"));
	}

	[[nodiscard]] bool valid() const
	{
		return get_output_by_name && enum_outputs && output_get_ref && output_release && output_get_name &&
			add_packet_callback && remove_packet_callback && packet_release && allocate &&
			encoder_get_codec && encoder_get_width && encoder_get_height && output_set_last_error && output_signal_stop;
	}
};

ObsPacketApi &obs_api()
{
	static ObsPacketApi api;
	return api;
}

struct OutputLookup {
	QString requested_name;
	obs_output_t *output = nullptr;
};

bool find_output_case_insensitive(void *param, obs_output_t *candidate)
{
	auto *lookup = static_cast<OutputLookup *>(param);
	ObsPacketApi &api = obs_api();
	const char *name = api.output_get_name(candidate);
	if (name && QString::fromUtf8(name).compare(lookup->requested_name, Qt::CaseInsensitive) == 0) {
		lookup->output = api.output_get_ref(candidate);
		return false;
	}
	return true;
}

obs_output_t *find_output(const QString &name)
{
	ObsPacketApi &api = obs_api();
	if (name == main_stream_key())
		return api.frontend_get_streaming_output ? api.frontend_get_streaming_output() : nullptr;
	const QByteArray utf8_name = name.toUtf8();
	if (obs_output_t *output = api.get_output_by_name(utf8_name.constData()))
		return output;
	OutputLookup lookup{name};
	api.enum_outputs(find_output_case_insensitive, &lookup);
	return lookup.output;
}

std::optional<SignedVideoCodec> video_codec(const char *name)
{
	const QByteArray codec = QByteArray(name ? name : "").toLower();
	if (codec == "h264" || codec == "avc")
		return SignedVideoCodec::H264;
	if (codec == "hevc" || codec == "h265")
		return SignedVideoCodec::Hevc;
	return std::nullopt;
}

} // namespace

struct OutputSigningManager::Session : std::enable_shared_from_this<OutputSigningManager::Session> {
	Session(OutputSigningManager *session_owner, QString output_name, FrameSignApiConfig api_config,
		SignedSeiConfig signing_config)
		: owner(session_owner), name(std::move(output_name)), api(std::move(api_config)),
		  signing(std::move(signing_config)), metadata(signing)
	{
		input.aid = signing.aid;
		input.uid = signing.uid;
		input.device_id = signing.device_id;
		input.room_id = signing.room_id;
		input.frame_type = QStringLiteral("2");
	}

	OutputSigningManager *owner = nullptr;
	QString name;
	FrameSignApiConfig api;
	SignedSeiConfig signing;
	FrameSignInput input;
	SignedSeiSession metadata;
	obs_output_t *output = nullptr;
	std::atomic_bool refreshing = false;
	std::atomic_bool fatal = false;
	std::atomic_bool cancelled = false;
};

OutputSigningManager::OutputSigningManager(QObject *parent) : QObject(parent)
{
	refresh_timer_ = new QTimer(this);
	refresh_timer_->setInterval(30000);
	connect(refresh_timer_, &QTimer::timeout, this, [this] { refresh_due_sessions(); });
	if (obs_api().frontend_add_event_callback)
		obs_api().frontend_add_event_callback(frontend_event_callback, this);
}

OutputSigningManager::~OutputSigningManager()
{
	shutting_down_ = true;
	if (obs_api().frontend_remove_event_callback)
		obs_api().frontend_remove_event_callback(frontend_event_callback, this);
	refresh_timer_->stop();
	detach_all();
	for (std::future<void> &worker : workers_) {
		if (!worker.valid())
			continue;
		try {
			worker.get();
		} catch (...) {
		}
	}
}

void OutputSigningManager::frontend_event_callback(obs_frontend_event event, void *param)
{
	if (event == OBS_FRONTEND_EVENT_STREAMING_STARTING)
		static_cast<OutputSigningManager *>(param)->rebind_main_output();
}

void OutputSigningManager::rebind_main_output()
{
	const std::shared_ptr<Session> session = sessions_.value(main_stream_key());
	if (!session || !session->output)
		return;
	session->metadata.reset_stream();
	obs_output_t *current = find_output(main_stream_key());
	if (!current)
		return;
	if (current == session->output) {
		obs_api().output_release(current);
		return;
	}
	obs_api().remove_packet_callback(session->output, packet_callback, session.get());
	obs_api().output_release(session->output);
	session->output = current;
	obs_api().add_packet_callback(session->output, packet_callback, session.get());
}

void OutputSigningManager::prepare_and_attach(const QString &output_name, FrameSignApiConfig api,
	SignedSeiConfig signing, Completion completion)
{
	if (!api.valid()) {
		completion(false, QStringLiteral("The RapidAPI frame-signing URL or key is missing or invalid."));
		return;
	}
	if (!signing.complete()) {
		QStringList missing;
		if (signing.uid.trimmed().isEmpty())
			missing.push_back(QStringLiteral("TikTok UID"));
		if (signing.device_id.trimmed().isEmpty())
			missing.push_back(QStringLiteral("device ID"));
		if (signing.room_id.trimmed().isEmpty())
			missing.push_back(QStringLiteral("room ID"));
		completion(false, QStringLiteral("Frame signing is missing: %1.").arg(missing.join(QStringLiteral(", "))));
		return;
	}
	if (!obs_api().valid()) {
		completion(false, QStringLiteral("This OBS version does not provide the encoded-packet callback API required for in-process signing."));
		return;
	}
	const QString output_key = normalized_output_key(output_name);
	if (output_key == main_stream_key() && (!obs_api().frontend_get_streaming_output ||
		!obs_api().frontend_add_event_callback || !obs_api().frontend_remove_event_callback)) {
		completion(false, QStringLiteral("This OBS frontend does not expose the main streaming output required for in-process signing."));
		return;
	}
	obs_output_t *probe = find_output(output_key);
	if (!probe) {
		completion(false, output_key == main_stream_key()
			? QStringLiteral("OBS could not obtain its main streaming output before signature prefetch.")
			: QStringLiteral("OBS could not find the selected output before signature prefetch."));
		return;
	}
	obs_api().output_release(probe);
	detach(output_key);
	auto session = std::make_shared<Session>(this, output_key, std::move(api), std::move(signing));
	pending_.insert(output_key, session);
	QPointer<OutputSigningManager> guard(this);
	auto completion_holder = std::make_shared<Completion>(std::move(completion));
	if (!launch_worker([guard, session, completion_holder]() mutable {
		const qint64 start = QDateTime::currentSecsSinceEpoch();
		FrameSignInput input = session->input;
		input.timestamp_seconds = start;
		FrameSignBatch batch = FrameSignClient::fetch_batch(session->api, input, start, 300, 1,
			&session->cancelled);
		if (!guard)
			return;
		QMetaObject::invokeMethod(guard, [guard, session, batch = std::move(batch), completion_holder]() mutable {
			if (guard)
				guard->finish_initial_prepare(session, std::move(batch), std::move(*completion_holder));
		}, Qt::QueuedConnection);
	})) {
		pending_.remove(output_key);
		session->cancelled = true;
		(*completion_holder)(false, QStringLiteral("The frame-signing background worker could not be started."));
	}
}

void OutputSigningManager::finish_initial_prepare(const std::shared_ptr<Session> &session, FrameSignBatch batch,
	Completion completion)
{
	if (pending_.value(session->name) != session)
		return;
	pending_.remove(session->name);
	if (!batch.valid()) {
		completion(false, batch.error);
		return;
	}
	obs_output_t *output = find_output(session->name);
	if (!output) {
		completion(false, session->name == main_stream_key()
			? QStringLiteral("OBS could not obtain its main streaming output before it started.")
			: QStringLiteral("OBS could not find the selected output before it started."));
		return;
	}
	session->metadata.merge_signatures(batch.signatures);
	session->output = output;
	obs_api().add_packet_callback(output, packet_callback, session.get());
	sessions_.insert(session->name, session);
	if (!refresh_timer_->isActive())
		refresh_timer_->start();
	completion(true, {});
}

void OutputSigningManager::detach(const QString &output_name)
{
	const QString output_key = normalized_output_key(output_name);
	const auto pending_it = pending_.find(output_key);
	if (pending_it != pending_.end()) {
		pending_it.value()->cancelled = true;
		pending_.erase(pending_it);
	}
	const auto it = sessions_.find(output_key);
	if (it == sessions_.end())
		return;
	const std::shared_ptr<Session> session = it.value();
	sessions_.erase(it);
	session->cancelled = true;
	if (session->output) {
		obs_api().remove_packet_callback(session->output, packet_callback, session.get());
		obs_api().output_release(session->output);
		session->output = nullptr;
	}
	if (sessions_.isEmpty())
		refresh_timer_->stop();
}

void OutputSigningManager::detach_all()
{
	for (const std::shared_ptr<Session> &session : std::as_const(pending_))
		session->cancelled = true;
	pending_.clear();
	const QStringList names = sessions_.keys();
	for (const QString &name : names)
		detach(name);
}

bool OutputSigningManager::attached(const QString &output_name) const
{
	return sessions_.contains(normalized_output_key(output_name));
}

void OutputSigningManager::packet_callback(obs_output *, encoder_packet *packet,
	encoder_packet_time *, void *param)
{
	auto *session = static_cast<Session *>(param);
	if (!session || !packet || !packet->data || packet->type != OBS_ENCODER_VIDEO || packet->track_idx != 0 ||
		session->fatal.load(std::memory_order_relaxed))
		return;
	ObsPacketApi &api = obs_api();
	QString error;
	const auto codec = packet->encoder ? video_codec(api.encoder_get_codec(packet->encoder)) : std::nullopt;
	if (!packet->encoder) {
		error = QStringLiteral("OBS returned an encoded video packet without its encoder.");
	} else if (!codec) {
		error = QStringLiteral("In-process TikTok signing supports only H.264 and HEVC outputs.");
	} else {
		const uint32_t width = api.encoder_get_width(packet->encoder);
		const uint32_t height = api.encoder_get_height(packet->encoder);
		const qint64 stream_ms = packet->dts_usec / 1000;
		const qint64 wall_ms = QDateTime::currentMSecsSinceEpoch();
		const QByteArray original = QByteArray::fromRawData(reinterpret_cast<const char *>(packet->data),
			static_cast<qsizetype>(packet->size));
		QByteArray replacement_data;
		if (session->metadata.transform_packet(*codec, original, stream_ms, static_cast<int>(width),
			static_cast<int>(height), wall_ms, &replacement_data, &error)) {
			const qsizetype byte_count = replacement_data.size();
			if (byte_count <= 0 || static_cast<quint64>(byte_count) >
				static_cast<quint64>(std::numeric_limits<size_t>::max() - sizeof(long))) {
				error = QStringLiteral("The signed video packet is too large for OBS.");
			} else {
				const size_t packet_size = static_cast<size_t>(byte_count);
				// OBS packet buffers have a long reference count immediately before
				// data. This is the ownership layout used by OBS's own BPM packet
				// callback; obs_encoder_packet_release then owns the replacement.
				auto *references = static_cast<long *>(api.allocate(sizeof(long) + packet_size));
				if (!references) {
					error = QStringLiteral("OBS could not allocate the signed video packet.");
				} else {
					*references = 1;
					std::memcpy(references + 1, replacement_data.constData(), packet_size);
					encoder_packet replacement = *packet;
					replacement.data = reinterpret_cast<uint8_t *>(references + 1);
					replacement.size = packet_size;
					api.packet_release(packet);
					*packet = replacement;
				}
			}
		}
	}
	if (!error.isEmpty() && !session->fatal.exchange(true)) {
		const std::shared_ptr<Session> keep_alive = session->shared_from_this();
		QPointer<OutputSigningManager> guard(session->owner);
		if (guard) {
			QMetaObject::invokeMethod(guard, [guard, keep_alive, error = std::move(error)] {
				if (guard)
					guard->finish_fatal(keep_alive, error);
			}, Qt::QueuedConnection);
		}
	}
}

void OutputSigningManager::finish_fatal(const std::shared_ptr<Session> &session, const QString &error)
{
	if (sessions_.value(session->name) != session || !session->output)
		return;
	const QByteArray message = error.toUtf8();
	obs_api().output_set_last_error(session->output, message.constData());
	// Packet callbacks run while OBS holds its callback-list mutex. Stopping
	// from the callback itself can synchronously notify listeners that detach
	// callbacks, so this is deliberately queued until that mutex is released.
	obs_api().output_signal_stop(session->output, OBS_OUTPUT_ERROR);
}

void OutputSigningManager::refresh_due_sessions()
{
	const qint64 now = QDateTime::currentSecsSinceEpoch();
	for (const std::shared_ptr<Session> &session : std::as_const(sessions_)) {
		const qint64 cache_until = session->metadata.cache_last_timestamp();
		if (cache_until - now > 60 || session->refreshing.exchange(true))
			continue;
		const qint64 start = now > cache_until + 1 ? now : cache_until + 1;
		QPointer<OutputSigningManager> guard(this);
		if (!launch_worker([guard, session, start] {
			FrameSignInput input = session->input;
			input.timestamp_seconds = start;
			FrameSignBatch batch = FrameSignClient::fetch_batch(session->api, input, start, 300, 1,
				&session->cancelled);
			if (!guard) {
				session->refreshing = false;
				return;
			}
			QMetaObject::invokeMethod(guard, [guard, session, batch = std::move(batch)]() mutable {
				session->refreshing = false;
				if (guard && batch.valid() && guard->sessions_.value(session->name) == session)
					session->metadata.merge_signatures(batch.signatures);
			}, Qt::QueuedConnection);
		}))
			session->refreshing = false;
	}
}

bool OutputSigningManager::launch_worker(std::function<void()> work)
{
	if (shutting_down_)
		return false;
	reap_workers();
	try {
		workers_.push_back(std::async(std::launch::async, [work = std::move(work)]() mutable { work(); }));
		return true;
	} catch (...) {
		return false;
	}
}

void OutputSigningManager::reap_workers()
{
	for (auto it = workers_.begin(); it != workers_.end();) {
		if (!it->valid() || it->wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
			if (it->valid()) {
				try {
					it->get();
				} catch (...) {
				}
			}
			it = workers_.erase(it);
		} else {
			++it;
		}
	}
}
