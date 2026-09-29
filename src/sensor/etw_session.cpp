#include "etwc/sensor/etw_session.hpp"

#include <windows.h>
// krabs kéo theo <tdh.h>, <evntrace.h>...
#include <krabs.hpp>
#include <string>
#include <vector>

#include "etwc/common/encoding.hpp"
#include "etwc/common/logging.hpp"
#include "etwc/sensor/providers.hpp"

namespace etwc {
namespace {

// Chuyển một property (bất kỳ kiểu TDH thông dụng) thành chuỗi UTF-8.
// Trả về chuỗi rỗng nếu không parse được hoặc kiểu không hỗ trợ.
std::string property_to_string(krabs::parser& parser, const krabs::property& prop) {
    const std::wstring& name = prop.name();
    switch (prop.type()) {
        case TDH_INTYPE_UNICODESTRING: {
            std::wstring v;
            if (parser.try_parse(name, v))
                return wide_to_utf8(v);
            break;
        }
        case TDH_INTYPE_ANSISTRING: {
            std::string v;
            if (parser.try_parse(name, v))
                return v;
            break;
        }
        case TDH_INTYPE_INT8: {
            std::int8_t v{};
            if (parser.try_parse(name, v))
                return std::to_string(static_cast<int>(v));
            break;
        }
        case TDH_INTYPE_UINT8: {
            std::uint8_t v{};
            if (parser.try_parse(name, v))
                return std::to_string(static_cast<unsigned>(v));
            break;
        }
        case TDH_INTYPE_INT16: {
            std::int16_t v{};
            if (parser.try_parse(name, v))
                return std::to_string(v);
            break;
        }
        case TDH_INTYPE_UINT16: {
            std::uint16_t v{};
            if (parser.try_parse(name, v))
                return std::to_string(v);
            break;
        }
        case TDH_INTYPE_INT32: {
            std::int32_t v{};
            if (parser.try_parse(name, v))
                return std::to_string(v);
            break;
        }
        case TDH_INTYPE_UINT32:
        case TDH_INTYPE_HEXINT32: {
            std::uint32_t v{};
            if (parser.try_parse(name, v))
                return std::to_string(v);
            break;
        }
        case TDH_INTYPE_INT64: {
            std::int64_t v{};
            if (parser.try_parse(name, v))
                return std::to_string(v);
            break;
        }
        case TDH_INTYPE_UINT64:
        case TDH_INTYPE_HEXINT64:
        case TDH_INTYPE_FILETIME: {
            std::uint64_t v{};
            if (parser.try_parse(name, v))
                return std::to_string(v);
            break;
        }
        case TDH_INTYPE_BOOLEAN: {
            bool v{};
            if (parser.try_parse(name, v))
                return v ? "1" : "0";
            break;
        }
        case TDH_INTYPE_POINTER: {
            krabs::pointer v{};
            if (parser.try_parse(name, v))
                return std::to_string(v.address);
            break;
        }
        default:
            break;
    }
    return {};
}

// Khởi tạo provider bằng GUID (nhanh, không dùng COM name lookup).
krabs::provider<> make_provider(const wchar_t* guid_str, std::uint64_t keywords) {
    const GUID id = krabs::guid(guid_str);
    krabs::provider<> p(id);
    p.any(keywords);
    return p;
}

}  // namespace

struct EtwSession::Impl {
    krabs::user_trace trace{L"EtwTelemetryCollector"};
    std::vector<krabs::provider<>> providers;
};

EtwSession::EtwSession(const Config& cfg) : cfg_(cfg), impl_(std::make_unique<Impl>()) {}

EtwSession::~EtwSession() {
    stop();
}

void EtwSession::on_raw_record(ProviderId provider, const void* record_ptr, const void* ctx_ptr) {
    const auto& record = *static_cast<const EVENT_RECORD*>(record_ptr);
    const auto& ctx = *static_cast<const krabs::trace_context*>(ctx_ptr);

    RawEvent ev;
    ev.provider = provider;
    ev.pid = record.EventHeader.ProcessId;
    ev.tid = record.EventHeader.ThreadId;
    ev.timestamp = static_cast<Timestamp>(record.EventHeader.TimeStamp.QuadPart);

    try {
        krabs::schema schema(record, ctx.schema_locator);
        ev.event_id = static_cast<std::uint16_t>(schema.event_id());
        ev.opcode = static_cast<std::uint8_t>(schema.event_opcode());

        krabs::parser parser(schema);
        for (const krabs::property& prop : parser.properties()) {
            std::string value = property_to_string(parser, prop);
            if (!value.empty())
                ev.properties.emplace(wide_to_utf8(prop.name()), std::move(value));
        }
    } catch (const std::exception&) {
        // Schema chưa sẵn / event lạ: vẫn đẩy metadata cơ bản đi.
    }

    events_received_.fetch_add(1, std::memory_order_relaxed);
    if (sink_)
        sink_(std::move(ev));
}

void EtwSession::configure_providers() {
    impl_->providers.clear();

    auto add = [&](ProviderId id, const wchar_t* guid_str, std::uint64_t keywords) {
        ETWC_LOG_DEBUG(std::string("EtwSession: enabling provider ") + wide_to_utf8(guid_str));
        krabs::provider<> p = make_provider(guid_str, keywords);
        p.add_on_event_callback(
            [this, id](const EVENT_RECORD& record, const krabs::trace_context& ctx) {
                on_raw_record(id, &record, &ctx);
            });
        impl_->providers.push_back(std::move(p));
        impl_->trace.enable(impl_->providers.back());
    };

    if (cfg_.trace_process)
        add(ProviderId::KernelProcess, providers::kKernelProcess, providers::kProcessKeywords);
    if (cfg_.trace_file)
        add(ProviderId::KernelFile, providers::kKernelFile, providers::kFileKeywords);
    if (cfg_.trace_registry)
        add(ProviderId::KernelRegistry, providers::kKernelRegistry, providers::kRegistryKeywords);
    if (cfg_.trace_network)
        add(ProviderId::KernelNetwork, providers::kKernelNetwork, providers::kNetworkKeywords);

    ETWC_LOG_INFO("EtwSession: enabled " + std::to_string(impl_->providers.size()) + " providers");
}

void EtwSession::start(RawEventSink sink) {
    sink_ = std::move(sink);
    configure_providers();
    running_ = true;

    trace_thread_ = std::thread([this] {
        try {
            ETWC_LOG_INFO("ETW trace loop starting");
            impl_->trace.start();  // blocking cho tới khi stop()
        } catch (const std::exception& e) {
            ETWC_LOG_ERROR(std::string("ETW trace failed (cần quyền Administrator?): ") + e.what());
        }
        running_ = false;
    });
}

void EtwSession::stop() {
    if (!running_.exchange(false)) {
        if (trace_thread_.joinable())
            trace_thread_.join();
        return;
    }
    try {
        impl_->trace.stop();
    } catch (const std::exception& e) {
        ETWC_LOG_ERROR(std::string("ETW trace stop error: ") + e.what());
    }
    if (trace_thread_.joinable())
        trace_thread_.join();
}

}  // namespace etwc
