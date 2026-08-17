// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Bridge Contributors

#include "aitum_outputs.hpp"

#include <windows.h>

#include <cstring>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <obs.h>

namespace {

struct WebsocketResponse {
	unsigned int status_code;
	char *comment;
	char *response_data;
};

using obs_get_proc_handler_fn = proc_handler_t *(*)();
using proc_handler_call_fn = bool (*)(proc_handler_t *, const char *, calldata_t *);
using calldata_set_data_fn = void (*)(calldata_t *, const char *, const void *, size_t);
using calldata_get_data_fn = bool (*)(const calldata_t *, const char *, void *, size_t);
using bfree_fn = void (*)(void *);

template<typename Function>
Function obs_function(const char *name)
{
	const HMODULE obs = GetModuleHandleW(L"obs.dll");
	return obs ? reinterpret_cast<Function>(GetProcAddress(obs, name)) : nullptr;
}

QJsonArray find_outputs(const QJsonValue &value)
{
	if (value.isObject()) {
		const QJsonObject object = value.toObject();
		const auto outputs = object.value(QStringLiteral("outputs"));
		if (outputs.isArray())
			return outputs.toArray();
		for (auto it = object.begin(); it != object.end(); ++it) {
			const QJsonArray nested = find_outputs(it.value());
			if (!nested.isEmpty())
				return nested;
		}
	}
	return {};
}

bool find_success(const QJsonValue &value, bool *success)
{
	if (value.isObject()) {
		const QJsonObject object = value.toObject();
		if (object.contains(QStringLiteral("success")) && object.value(QStringLiteral("success")).isBool()) {
			*success = object.value(QStringLiteral("success")).toBool();
			return true;
		}
		for (auto it = object.begin(); it != object.end(); ++it)
			if (find_success(it.value(), success))
				return true;
	}
	if (value.isArray())
		for (const QJsonValue &item : value.toArray())
			if (find_success(item, success))
				return true;
	return false;
}

} // namespace

bool aitum_stream_suite_available()
{
	return GetModuleHandleW(L"aitum-stream-suite.dll") != nullptr;
}

QStringList aitum_output_names(QString *diagnostic, QHash<QString, QString> *types, QHash<QString, bool> *active_states)
{
	auto set_diagnostic = [diagnostic](const QString &message) {
		if (diagnostic)
			*diagnostic = message;
	};
	const auto get_global = obs_function<obs_get_proc_handler_fn>("obs_get_proc_handler");
	const auto call = obs_function<proc_handler_call_fn>("proc_handler_call");
	const auto set_data = obs_function<calldata_set_data_fn>("calldata_set_data");
	const auto get_data = obs_function<calldata_get_data_fn>("calldata_get_data");
	const auto free_memory = obs_function<bfree_fn>("bfree");
	if (!get_global || !call || !set_data || !get_data || !free_memory) {
		set_diagnostic(QStringLiteral("Required OBS APIs are unavailable."));
		return {};
	}
	auto set_string = [set_data](calldata_t *data, const char *name, const char *value) {
		set_data(data, name, value, value ? std::strlen(value) + 1 : 0);
	};
	auto get_ptr = [get_data](const calldata_t *data, const char *name) -> void * {
		void *value = nullptr;
		get_data(data, name, &value, sizeof(value));
		return value;
	};
	auto free_calldata = [free_memory](calldata_t *data) {
		if (!data->fixed && data->stack)
			free_memory(data->stack);
		data->stack = nullptr;
		data->size = 0;
		data->capacity = 0;
	};

	proc_handler_t *global = get_global();
	if (!global) {
		set_diagnostic(QStringLiteral("The OBS global procedure handler is unavailable."));
		return {};
	}
	calldata_t lookup = {};
	if (!call(global, "obs_websocket_api_get_ph", &lookup)) {
		free_calldata(&lookup);
		set_diagnostic(QStringLiteral("The OBS WebSocket API is unavailable."));
		return {};
	}
	proc_handler_t *websocket = static_cast<proc_handler_t *>(get_ptr(&lookup, "ph"));
	free_calldata(&lookup);
	if (!websocket) {
		set_diagnostic(QStringLiteral("OBS WebSocket did not provide its procedure handler."));
		return {};
	}

	const QJsonObject request_data{{QStringLiteral("vendorName"), QStringLiteral("aitum-stream-suite")},
					  {QStringLiteral("requestType"), QStringLiteral("get_outputs")},
					  {QStringLiteral("requestData"), QJsonObject{}}};
	const QByteArray request_json = QJsonDocument(request_data).toJson(QJsonDocument::Compact);
	calldata_t request = {};
	set_string(&request, "request_type", "CallVendorRequest");
	set_string(&request, "request_data", request_json.constData());
	if (!call(websocket, "call_request", &request)) {
		free_calldata(&request);
		set_diagnostic(QStringLiteral("OBS WebSocket could not call Aitum's vendor request."));
		return {};
	}
	auto *response = static_cast<WebsocketResponse *>(get_ptr(&request, "response"));
	QStringList names;
	QString response_diagnostic;
	if (response && response->status_code == 100 && response->response_data) {
		const auto document = QJsonDocument::fromJson(QByteArray(response->response_data));
		for (const QJsonValue &item : find_outputs(document.object())) {
			const QJsonObject output = item.toObject();
			const QString name = output.value(QStringLiteral("name")).toString();
			if (!name.isEmpty() && !names.contains(name))
				names.push_back(name);
			if (!name.isEmpty() && types)
				types->insert(name, output.value(QStringLiteral("type")).toString());
			if (!name.isEmpty() && active_states)
				active_states->insert(name, output.value(QStringLiteral("active")).toBool());
		}
		if (names.isEmpty())
			response_diagnostic = QStringLiteral("Aitum returned no stream outputs.");
	} else if (response) {
		response_diagnostic = response->comment && response->comment[0]
			? QString::fromUtf8(response->comment)
			: QStringLiteral("Aitum rejected the output request (status %1).").arg(response->status_code);
	} else {
		response_diagnostic = QStringLiteral("OBS WebSocket returned no response for Aitum's output request.");
	}
	if (response) {
		if (response->comment)
			free_memory(response->comment);
		if (response->response_data)
			free_memory(response->response_data);
		free_memory(response);
	}
	free_calldata(&request);
	names.sort(Qt::CaseInsensitive);
	if (names.isEmpty())
		set_diagnostic(response_diagnostic);
	else if (diagnostic)
		diagnostic->clear();
	return names;
}

bool aitum_output_is_active(const QString &output_name, bool *active, QString *diagnostic)
{
	if (active)
		*active = false;
	if (output_name.isEmpty()) {
		if (diagnostic)
			*diagnostic = QStringLiteral("No Aitum output was selected.");
		return false;
	}
	QHash<QString, bool> active_states;
	QString request_diagnostic;
	aitum_output_names(&request_diagnostic, nullptr, &active_states);
	const auto state = active_states.constFind(output_name);
	if (state == active_states.cend()) {
		if (diagnostic)
			*diagnostic = request_diagnostic.isEmpty()
				? QStringLiteral("Aitum did not return the selected output.")
				: request_diagnostic;
		return false;
	}
	if (active)
		*active = *state;
	if (diagnostic)
		diagnostic->clear();
	return true;
}

bool aitum_start_output(const QString &output_name, QString *diagnostic)
{
	auto set_diagnostic = [diagnostic](const QString &message) {
		if (diagnostic)
			*diagnostic = message;
	};
	if (output_name.isEmpty()) {
		set_diagnostic(QStringLiteral("No Aitum output was selected."));
		return false;
	}
	const auto get_global = obs_function<obs_get_proc_handler_fn>("obs_get_proc_handler");
	const auto call = obs_function<proc_handler_call_fn>("proc_handler_call");
	const auto set_data = obs_function<calldata_set_data_fn>("calldata_set_data");
	const auto get_data = obs_function<calldata_get_data_fn>("calldata_get_data");
	const auto free_memory = obs_function<bfree_fn>("bfree");
	if (!get_global || !call || !set_data || !get_data || !free_memory) {
		set_diagnostic(QStringLiteral("Required OBS APIs are unavailable."));
		return false;
	}
	auto set_string = [set_data](calldata_t *data, const char *name, const char *value) {
		set_data(data, name, value, value ? std::strlen(value) + 1 : 0);
	};
	auto get_ptr = [get_data](const calldata_t *data, const char *name) -> void * {
		void *value = nullptr;
		get_data(data, name, &value, sizeof(value));
		return value;
	};
	auto free_calldata = [free_memory](calldata_t *data) {
		if (!data->fixed && data->stack)
			free_memory(data->stack);
		data->stack = nullptr;
		data->size = 0;
		data->capacity = 0;
	};
	proc_handler_t *global = get_global();
	if (!global) {
		set_diagnostic(QStringLiteral("The OBS global procedure handler is unavailable."));
		return false;
	}
	calldata_t lookup = {};
	if (!call(global, "obs_websocket_api_get_ph", &lookup)) {
		free_calldata(&lookup);
		set_diagnostic(QStringLiteral("The OBS WebSocket API is unavailable."));
		return false;
	}
	proc_handler_t *websocket = static_cast<proc_handler_t *>(get_ptr(&lookup, "ph"));
	free_calldata(&lookup);
	if (!websocket) {
		set_diagnostic(QStringLiteral("OBS WebSocket did not provide its procedure handler."));
		return false;
	}
	const QJsonObject request_data{{QStringLiteral("vendorName"), QStringLiteral("aitum-stream-suite")},
		{QStringLiteral("requestType"), QStringLiteral("start_output")},
		{QStringLiteral("requestData"), QJsonObject{{QStringLiteral("output"), output_name}}}};
	const QByteArray request_json = QJsonDocument(request_data).toJson(QJsonDocument::Compact);
	calldata_t request = {};
	set_string(&request, "request_type", "CallVendorRequest");
	set_string(&request, "request_data", request_json.constData());
	if (!call(websocket, "call_request", &request)) {
		free_calldata(&request);
		set_diagnostic(QStringLiteral("OBS WebSocket could not call Aitum's start request."));
		return false;
	}
	auto *response = static_cast<WebsocketResponse *>(get_ptr(&request, "response"));
	bool started = false;
	QString response_diagnostic;
	if (response && response->status_code == 100 && response->response_data) {
		const QJsonDocument document = QJsonDocument::fromJson(QByteArray(response->response_data));
		bool vendor_success = true;
		if (find_success(document.object(), &vendor_success))
			started = vendor_success;
		else
			started = true;
		if (!started)
			response_diagnostic = QStringLiteral("Aitum could not start the selected output.");
	} else if (response) {
		response_diagnostic = response->comment && response->comment[0]
			? QString::fromUtf8(response->comment)
			: QStringLiteral("Aitum rejected the start request (status %1).").arg(response->status_code);
	} else {
		response_diagnostic = QStringLiteral("OBS WebSocket returned no response for Aitum's start request.");
	}
	if (response) {
		if (response->comment)
			free_memory(response->comment);
		if (response->response_data)
			free_memory(response->response_data);
		free_memory(response);
	}
	free_calldata(&request);
	if (!started)
		set_diagnostic(response_diagnostic);
	else if (diagnostic)
		diagnostic->clear();
	return started;
}
