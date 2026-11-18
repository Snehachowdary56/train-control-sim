# Design review checklist

Used before merging each change. Findings from reviewing this project are recorded below.

| Area | Question | Result |
|---|---|---|
| Requirements | Does every requirement in `REQUIREMENTS.md` have a test? | Yes, see the traceability table |
| Safety | Can any path move a train past its MA? | No: checked in `step()` (sets `ma_violation`), by the controller's minimum-gap check, and by `log_report.pl` |
| Safety | What happens if the radio drops? | Emergency brake after 2 s (FR-6), tested |
| Concurrency | Is all shared state behind the mutex? Any lock held during I/O? | Yes; no lock held during `send`/`recv` |
| Concurrency | Can shutdown hang? | No: `accept()` woken by a loopback connection, workers woken by `shutdown()` |
| Portability | Any POSIX-only calls outside `net.cpp`? | No |
| Input handling | Malformed or negative values in messages? | Rejected and counted (FR-8) |
| Testing | Unit, integration, stress, race detection, log checks? | All run in CI |

## Findings

Found while reviewing the code for the Windows port, before the first Windows build:

- **F-1 (fixed):** the test macro used `do { } while (0)`, which MSVC can flag as a constant condition at
  `/W4`; with `/WX` that would stop the build. Changed to an `if/else` form.
- **F-2 (fixed):** `windows.h` defines `min`/`max` macros that clash with `std::min`/`std::max`. Added
  `NOMINMAX` for Windows builds.
