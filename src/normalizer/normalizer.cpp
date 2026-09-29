#include "etwc/normalizer/normalizer.hpp"

#include "etwc/common/path_normalizer.hpp"
#include "etwc/common/uuid.hpp"
#include "etwc/sensor/peb_reader.hpp"

namespace etwc {

Normalizer::Normalizer() = default;

std::optional<NormalizedEvent> Normalizer::normalize(const RawEvent& raw) {
    EventKind kind = map_opcode(raw);
    if (kind == EventKind::Unknown)
        return std::nullopt;

    NormalizedEvent ev;
    ev.uuid = generate_uuid_v4();
    ev.kind = kind;
    ev.timestamp = raw.timestamp;
    ev.pid = raw.pid;

    // TODO: đọc các property cụ thể theo kind (ImageName, FileName, KeyName...).
    ev.command_line = raw.get("CommandLine");
    ev.process_name = raw.get("ImageName");

    enrich_lineage(ev);
    enrich_privilege(ev);
    enrich_command_line(ev);
    return ev;
}

EventKind Normalizer::map_opcode(const RawEvent& /*raw*/) const {
    // TODO: ánh xạ (provider_id, event_id/opcode) -> EventKind.
    return EventKind::Unknown;
}

void Normalizer::enrich_lineage(NormalizedEvent& /*ev*/) const {
    // TODO: tra cứu ngược PPID trong cache tiến trình để điền parent_name.
}

void Normalizer::enrich_privilege(NormalizedEvent& /*ev*/) const {
    // TODO: mở token tiến trình -> is_system / token_elevated.
}

void Normalizer::enrich_command_line(NormalizedEvent& ev) const {
    if (ev.command_line.empty() && ev.kind == EventKind::ProcessCreate) {
        if (auto cmd = read_command_line_from_peb(ev.pid)) {
            ev.command_line = *cmd;
        }
    }
}

}  // namespace etwc
