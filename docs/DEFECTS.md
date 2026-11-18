# Defect reports

## DR-001: emergency brake test failed although the train behaved correctly

| Field | Value |
|---|---|
| Found by | `test_control` (`stale_authority_triggers_emergency_brake_then_recovers`) |
| Severity | Low (test defect, not a product defect) |
| Status | Fixed |

**Observed:** the check `speed < 20` failed after 2.5 s without a fresh movement authority.

**Investigation:** the train started at 20 m/s, below its 22 m/s line speed, so for the first 2 s (while its
authority was still valid) it correctly *accelerated*. The emergency brake only started after the 2 s timeout,
and 0.5 s of braking at 1.3 m/s² did not bring it back under 20 m/s. The control logic was right; the test's
expectation was wrong.

**Fix:** the test now checks that once the train is in emergency mode, its speed drops by exactly the
emergency braking rate on the next step.

**Prevention:** tests of timed behaviour compare against the state at the moment the behaviour starts, not
against the initial state.
