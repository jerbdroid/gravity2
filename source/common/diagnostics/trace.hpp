#pragma once

#include "perfetto.h"

#define GRAVITY_RENDERING_TRACE_INSTANT(...) \
  TRACE_EVENT_INSTANT("rendering", __VA_ARGS__, "file", __FILE__, "line", __LINE__)
#define GRAVITY_RENDERING_TRACE(...) \
  TRACE_EVENT("rendering", __VA_ARGS__, "file", __FILE__, "line", __LINE__)
#define GRAVITY_RENDERING_TRACE_COUNTER(...) TRACE_COUNTER("rendering", __VA_ARGS__)

#define GRAVITY_SYSTEM_TRACE_INSTANT(...) \
  TRACE_EVENT_INSTANT("system", __VA_ARGS__, "file", __FILE__, "line", __LINE__)
#define GRAVITY_SYSTEM_TRACE(...) \
  TRACE_EVENT("system", __VA_ARGS__, "file", __FILE__, "line", __LINE__)

#define GRAVITY_NETWORK_TRACE_INSTANT(...) \
  TRACE_EVENT_INSTANT("network", __VA_ARGS__, "file", __FILE__, "line", __LINE__)
#define GRAVITY_NETWORK_TRACE(...) \
  TRACE_EVENT("network", __VA_ARGS__, "file", __FILE__, "line", __LINE__)

#define GRAVITY_TRACE_INSTANT(CATEGORY, ...) \
  TRACE_EVENT_INSTANT(CATEGORY, __VA_ARGS__, "file", __FILE__, "line", __LINE__)
#define GRAVITY_TRACE(CATEGORY, ...) \
  TRACE_EVENT(CATEGORY, __VA_ARGS__, "file", __FILE__, "line", __LINE__)

#define GRAVITY_TRACE_BEGIN(CATEGORY, ...) \
  TRACE_EVENT_BEGIN(CATEGORY, __VA_ARGS__, "file", __FILE__, "line", __LINE__)

#define GRAVITY_TRACE_END(CATEGORY, ...) \
  TRACE_EVENT_END(CATEGORY, __VA_ARGS__, "file", __FILE__, "line", __LINE__)

PERFETTO_DEFINE_CATEGORIES(
    perfetto::Category("rendering").SetDescription("Events from the graphics subsystem"),
    perfetto::Category("network").SetDescription("Network upload and download statistics"),
    perfetto::Category("system").SetDescription("System events"),
    perfetto::Category("system.debug").SetDescription("System debug events").SetTags("debug"),
    perfetto::Category("resource").SetDescription("Resource events"),
    perfetto::Category("resource.debug").SetDescription("Resource debug events").SetTags("debug"));

using TracingSessionPtr = std::unique_ptr<perfetto::TracingSession>;

class TracingContext {
 public:
  static void initialize();
  static auto startTracing() -> TracingSessionPtr;
  static void stopTracing(TracingSessionPtr tracing_session);
};