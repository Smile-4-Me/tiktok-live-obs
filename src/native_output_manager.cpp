// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#include "native_output_manager.hpp"
#include "native_platform.hpp"

#include <obs.h>

#include <QStringList>

namespace {

using data_create_fn = obs_data_t *(*)();
using data_release_fn = void (*)(obs_data_t *);
using data_set_string_fn = void (*)(obs_data_t *, const char *, const char *);
using output_create_fn = obs_output_t *(*)(const char *, const char *, obs_data_t *, obs_data_t *);
using output_release_fn = void (*)(obs_output_t *);
using output_start_fn = bool (*)(obs_output_t *);
using output_force_stop_fn = void (*)(obs_output_t *);
using output_active_fn = bool (*)(const obs_output_t *);
using output_set_video_encoder_fn = void (*)(obs_output_t *, obs_encoder_t *);
using output_set_audio_encoder_fn = void (*)(obs_output_t *, obs_encoder_t *, size_t);
using output_get_video_encoder_fn = obs_encoder_t *(*)(const obs_output_t *);
using output_get_audio_encoder_fn = obs_encoder_t *(*)(const obs_output_t *, size_t);
using output_set_service_fn = void (*)(obs_output_t *, obs_service_t *);
using output_get_last_error_fn = const char *(*)(obs_output_t *);
using get_encoder_by_name_fn = obs_encoder_t *(*)(const char *);
using encoder_release_fn = void (*)(obs_encoder_t *);
using encoder_set_video_fn = void (*)(obs_encoder_t *, video_t *);
using encoder_set_audio_fn = void (*)(obs_encoder_t *, audio_t *);
using encoder_video_fn = video_t *(*)(const obs_encoder_t *);
using encoder_audio_fn = audio_t *(*)(const obs_encoder_t *);
using encoder_get_codec_fn = const char *(*)(const obs_encoder_t *);
using get_video_fn = video_t *(*)();
using get_audio_fn = audio_t *(*)();
using service_create_fn = obs_service_t *(*)(const char *, const char *, obs_data_t *, obs_data_t *);
using service_release_fn = void (*)(obs_service_t *);
using frontend_get_streaming_output_fn = obs_output_t *(*)();

struct ObsNativeOutputApi {
	data_create_fn data_create = nullptr;
	data_release_fn data_release = nullptr;
	data_set_string_fn data_set_string = nullptr;
	output_create_fn output_create = nullptr;
	output_release_fn output_release = nullptr;
	output_start_fn output_start = nullptr;
	output_force_stop_fn output_force_stop = nullptr;
	output_active_fn output_active = nullptr;
	output_set_video_encoder_fn output_set_video_encoder = nullptr;
	output_set_audio_encoder_fn output_set_audio_encoder = nullptr;
	output_get_video_encoder_fn output_get_video_encoder = nullptr;
	output_get_audio_encoder_fn output_get_audio_encoder = nullptr;
	output_set_service_fn output_set_service = nullptr;
	output_get_last_error_fn output_get_last_error = nullptr;
	get_encoder_by_name_fn get_encoder_by_name = nullptr;
	encoder_release_fn encoder_release = nullptr;
	encoder_set_video_fn encoder_set_video = nullptr;
	encoder_set_audio_fn encoder_set_audio = nullptr;
	encoder_video_fn encoder_video = nullptr;
	encoder_audio_fn encoder_audio = nullptr;
	encoder_get_codec_fn encoder_get_codec = nullptr;
	get_video_fn get_video = nullptr;
	get_audio_fn get_audio = nullptr;
	service_create_fn service_create = nullptr;
	service_release_fn service_release = nullptr;
	frontend_get_streaming_output_fn frontend_get_streaming_output = nullptr;

	ObsNativeOutputApi()
	{
#define LOAD_OBS(member, name) member = reinterpret_cast<decltype(member)>(resolve_native_symbol(NativeLibrary::Obs, name))
		LOAD_OBS(data_create, "obs_data_create");
		LOAD_OBS(data_release, "obs_data_release");
		LOAD_OBS(data_set_string, "obs_data_set_string");
		LOAD_OBS(output_create, "obs_output_create");
		LOAD_OBS(output_release, "obs_output_release");
		LOAD_OBS(output_start, "obs_output_start");
		LOAD_OBS(output_force_stop, "obs_output_force_stop");
		LOAD_OBS(output_active, "obs_output_active");
		LOAD_OBS(output_set_video_encoder, "obs_output_set_video_encoder");
		LOAD_OBS(output_set_audio_encoder, "obs_output_set_audio_encoder");
		LOAD_OBS(output_get_video_encoder, "obs_output_get_video_encoder");
		LOAD_OBS(output_get_audio_encoder, "obs_output_get_audio_encoder");
		LOAD_OBS(output_set_service, "obs_output_set_service");
		LOAD_OBS(output_get_last_error, "obs_output_get_last_error");
		LOAD_OBS(get_encoder_by_name, "obs_get_encoder_by_name");
		LOAD_OBS(encoder_release, "obs_encoder_release");
		LOAD_OBS(encoder_set_video, "obs_encoder_set_video");
		LOAD_OBS(encoder_set_audio, "obs_encoder_set_audio");
		LOAD_OBS(encoder_video, "obs_encoder_video");
		LOAD_OBS(encoder_audio, "obs_encoder_audio");
		LOAD_OBS(encoder_get_codec, "obs_encoder_get_codec");
		LOAD_OBS(get_video, "obs_get_video");
		LOAD_OBS(get_audio, "obs_get_audio");
		LOAD_OBS(service_create, "obs_service_create");
		LOAD_OBS(service_release, "obs_service_release");
#undef LOAD_OBS
		frontend_get_streaming_output = reinterpret_cast<frontend_get_streaming_output_fn>(
			resolve_native_symbol(NativeLibrary::Frontend, "obs_frontend_get_streaming_output"));
	}

	[[nodiscard]] bool valid() const
	{
		return data_create && data_release && data_set_string && output_create && output_release &&
			output_start && output_force_stop && output_active && output_set_video_encoder &&
			output_set_audio_encoder && output_get_video_encoder && output_get_audio_encoder &&
			output_set_service && output_get_last_error && get_encoder_by_name && encoder_release &&
			encoder_set_video && encoder_set_audio && encoder_video && encoder_audio && encoder_get_codec && get_video &&
			get_audio && service_create && service_release &&
			frontend_get_streaming_output;
	}
};

ObsNativeOutputApi &obs_api()
{
	static ObsNativeOutputApi api;
	return api;
}

QString native_output_name(const QString &profile_id)
{
	QString safe = profile_id.trimmed();
	for (qsizetype index = 0; index < safe.size(); ++index)
		if (!safe.at(index).isLetterOrNumber())
			safe[index] = QLatin1Char('-');
	return QStringLiteral("tiktok-live-obs-%1").arg(safe);
}

} // namespace

struct NativeOutputManager::Session {
	QString output_name;
	obs_output_t *output = nullptr;
	obs_service_t *service = nullptr;
};

NativeOutputManager::~NativeOutputManager()
{
	remove_all();
}

NativeOutputManager::CreateResult NativeOutputManager::create(const QString &profile_id,
	const QString &server, const QString &key)
{
	remove(profile_id);
	ObsNativeOutputApi &api = obs_api();
	if (!api.valid())
		return {{}, QStringLiteral("This OBS version does not expose the native output APIs required for TikTok streaming.")};
	if (profile_id.trimmed().isEmpty() || server.trimmed().isEmpty() || key.isEmpty())
		return {{}, QStringLiteral("TikTok did not provide complete RTMP credentials.")};

	obs_output_t *main_output = api.frontend_get_streaming_output();
	obs_encoder_t *video_encoder = main_output ? api.output_get_video_encoder(main_output) : nullptr;
	obs_encoder_t *audio_encoder = main_output ? api.output_get_audio_encoder(main_output, 0) : nullptr;
	obs_encoder_t *video_encoder_ref = nullptr;
	obs_encoder_t *audio_encoder_ref = nullptr;
	if (!video_encoder) {
		for (const char *name : {"advanced_video_stream", "simple_video_stream"}) {
			video_encoder_ref = api.get_encoder_by_name(name);
			if (video_encoder_ref) {
				video_encoder = video_encoder_ref;
				break;
			}
		}
	}
	if (!audio_encoder) {
		for (const char *name : {"adv_stream_audio", "simple_aac", "simple_opus"}) {
			audio_encoder_ref = api.get_encoder_by_name(name);
			if (audio_encoder_ref) {
				audio_encoder = audio_encoder_ref;
				break;
			}
		}
	}
	auto release_encoder_sources = [&] {
		if (video_encoder_ref)
			api.encoder_release(video_encoder_ref);
		if (audio_encoder_ref)
			api.encoder_release(audio_encoder_ref);
		if (main_output)
			api.output_release(main_output);
	};
	if (!video_encoder || !audio_encoder) {
		release_encoder_sources();
		return {{}, QStringLiteral("OBS could not find its configured video and audio stream encoders.")};
	}
	if (!api.encoder_video(video_encoder))
		api.encoder_set_video(video_encoder, api.get_video());
	if (!api.encoder_audio(audio_encoder))
		api.encoder_set_audio(audio_encoder, api.get_audio());
	if (!api.encoder_video(video_encoder) || !api.encoder_audio(audio_encoder)) {
		release_encoder_sources();
		return {{}, QStringLiteral("OBS could not initialize its configured stream encoders.")};
	}

	const QString name = native_output_name(profile_id);
	const QByteArray output_name_utf8 = name.toUtf8();
	const QByteArray service_name_utf8 = (name + QStringLiteral("-service")).toUtf8();
	obs_data_t *settings = api.data_create();
	if (!settings) {
		release_encoder_sources();
		return {{}, QStringLiteral("OBS could not allocate TikTok output settings.")};
	}
	const QByteArray server_utf8 = server.toUtf8();
	const QByteArray key_utf8 = key.toUtf8();
	api.data_set_string(settings, "server", server_utf8.constData());
	api.data_set_string(settings, "key", key_utf8.constData());
	obs_service_t *service = api.service_create("rtmp_custom", service_name_utf8.constData(), settings, nullptr);
	api.data_release(settings);
	if (!service) {
		release_encoder_sources();
		return {{}, QStringLiteral("OBS could not create the TikTok RTMP service.")};
	}

	obs_output_t *output = api.output_create("rtmp_output", output_name_utf8.constData(), nullptr, nullptr);
	if (!output) {
		api.service_release(service);
		release_encoder_sources();
		return {{}, QStringLiteral("OBS could not create the native TikTok stream output.")};
	}
	api.output_set_video_encoder(output, video_encoder);
	api.output_set_audio_encoder(output, audio_encoder, 0);
	api.output_set_service(output, service);
	release_encoder_sources();

	auto session = std::make_shared<Session>();
	session->output_name = name;
	session->output = output;
	session->service = service;
	sessions_.insert(profile_id, session);
	return {name, {}};
}

bool NativeOutputManager::start(const QString &profile_id, QString *error)
{
	if (error)
		error->clear();
	const std::shared_ptr<Session> session = sessions_.value(profile_id);
	if (!session || !session->output) {
		if (error)
			*error = QStringLiteral("The native TikTok output is not prepared.");
		return false;
	}
	if (obs_api().output_start(session->output))
		return true;
	if (error) {
		*error = last_error(profile_id);
		if (error->isEmpty())
			*error = QStringLiteral("OBS rejected the native TikTok output start request.");
	}
	return false;
}

void NativeOutputManager::remove(const QString &profile_id)
{
	const std::shared_ptr<Session> session = sessions_.take(profile_id);
	if (!session)
		return;
	ObsNativeOutputApi &api = obs_api();
	if (session->output) {
		if (api.output_active && api.output_active(session->output) && api.output_force_stop)
			api.output_force_stop(session->output);
		if (api.output_release)
			api.output_release(session->output);
		session->output = nullptr;
	}
	if (session->service && api.service_release) {
		api.service_release(session->service);
		session->service = nullptr;
	}
}

void NativeOutputManager::remove_all()
{
	const QStringList profile_ids = sessions_.keys();
	for (const QString &profile_id : profile_ids)
		remove(profile_id);
}

bool NativeOutputManager::contains(const QString &profile_id) const
{
	return sessions_.contains(profile_id);
}

bool NativeOutputManager::active(const QString &profile_id) const
{
	const std::shared_ptr<Session> session = sessions_.value(profile_id);
	return session && session->output && obs_api().output_active && obs_api().output_active(session->output);
}

QString NativeOutputManager::last_error(const QString &profile_id) const
{
	const std::shared_ptr<Session> session = sessions_.value(profile_id);
	if (!session || !session->output || !obs_api().output_get_last_error)
		return {};
	const char *message = obs_api().output_get_last_error(session->output);
	return message ? QString::fromUtf8(message).trimmed() : QString{};
}

QString NativeOutputManager::output_name(const QString &profile_id) const
{
	const std::shared_ptr<Session> session = sessions_.value(profile_id);
	return session ? session->output_name : QString{};
}

QString NativeOutputManager::configured_video_codec() const
{
	ObsNativeOutputApi &api = obs_api();
	if (!api.valid())
		return {};
	obs_output_t *main_output = api.frontend_get_streaming_output();
	obs_encoder_t *video_encoder = main_output ? api.output_get_video_encoder(main_output) : nullptr;
	obs_encoder_t *video_encoder_ref = nullptr;
	if (!video_encoder) {
		for (const char *name : {"advanced_video_stream", "simple_video_stream"}) {
			video_encoder_ref = api.get_encoder_by_name(name);
			if (video_encoder_ref) {
				video_encoder = video_encoder_ref;
				break;
			}
		}
	}
	const char *codec_name = video_encoder ? api.encoder_get_codec(video_encoder) : nullptr;
	const QString codec = codec_name ? QString::fromUtf8(codec_name).trimmed().toLower() : QString{};
	if (video_encoder_ref)
		api.encoder_release(video_encoder_ref);
	if (main_output)
		api.output_release(main_output);
	return codec;
}
