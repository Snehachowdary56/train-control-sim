# Requirements

Each requirement has an ID and the test that verifies it (traceability).

## Functional requirements

| ID | Requirement | Verified by |
|---|---|---|
| FR-1 | Each train reports its position and speed to the zone controller every 0.2 s. | `test_simulation` (reports counted by the controller) |
| FR-2 | The zone controller answers every report with a movement authority (MA): the rear of the train ahead minus a 50 m safety margin, or the end of the line for the lead train. | `test_control::authority_stops_behind_train_ahead` |
| FR-3 | An MA never moves backwards behind the train's current position. | `test_control::authority_never_goes_backwards` |
| FR-4 | A train brakes in time to stop at or before its MA and never passes it. | `test_control::train_never_passes_its_authority`, `test_simulation`, `log_report.pl` |
| FR-5 | A train stops at every station and dwells for 20 s. | `test_control::stops_and_dwells_at_station` |
| FR-6 | If no fresh MA arrives for 2 s (radio loss), the train applies the emergency brake and stays braked until it has stopped and a fresh MA arrives. | `test_control::stale_authority_triggers_emergency_brake_then_recovers`, `test_simulation::radio_loss...` |
| FR-7 | A train that reaches the terminus leaves the zone so it no longer limits the trains behind. | `test_simulation::four_trains...` (all trains finish) |
| FR-8 | Malformed messages are rejected and counted, never acted on. | `test_protocol` |
| FR-9 | The run log can be checked automatically and turned into a JUnit test report. | `tools/t/log_report.t` |
| FR-10 | Unusual train runs are flagged from the logs without labelled data. | `analysis/test_anomalies.py` |

## Non-functional requirements

| ID | Requirement | Verified by |
|---|---|---|
| NFR-1 Safety | No train is ever closer than the safety margin to the rear of the train ahead (as last reported). | controller `min_gap_m >= 0` in `test_simulation` and every CI run |
| NFR-2 Concurrency | No data races between the controller threads and train threads. | ThreadSanitizer CI job |
| NFR-3 Scalability | 12 trains on one line with 300 m starting headway run without violations. | `test_simulation::many_trains_stress` |
| NFR-4 Portability | Builds and passes all tests on Windows (MSVC, Winsock) and Linux (GCC, POSIX sockets). | CI `windows` and `linux` jobs |
| NFR-5 Code quality | Builds with all warnings treated as errors (`/W4 /WX`, `-Wall -Wextra -Werror`). | every CI build |
| NFR-6 Latency | Control messages are sent immediately (TCP_NODELAY), one small text line each. | design, `net.cpp` |
