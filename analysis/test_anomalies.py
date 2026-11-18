import subprocess
from pathlib import Path

import pandas as pd
import pytest

from analysis.anomalies import flag_anomalies, load, segment_features

ROOT = Path(__file__).resolve().parents[1]


def find_sim() -> Path:
    for p in [ROOT / "build" / "cbtc_sim", ROOT / "build" / "Release" / "cbtc_sim.exe", ROOT / "build" / "cbtc_sim.exe"]:
        if p.exists():
            return p
    pytest.skip("build cbtc_sim first (cmake --build build)")


@pytest.fixture(scope="module")
def log(tmp_path_factory) -> pd.DataFrame:
    path = tmp_path_factory.mktemp("sim") / "run.csv"
    subprocess.run([str(find_sim()), "--trains", "4", "--radio-loss", "3:300:304", "--log", str(path)],
                   check=True, capture_output=True)
    return load(path)


def test_features_cover_every_train(log):
    feats = segment_features(log)
    assert set(feats["train"]) == {1, 2, 3, 4}
    assert (feats["run_time_s"] > 0).all()


def test_radio_loss_segment_is_flagged(log):
    scored = flag_anomalies(segment_features(log))
    eb = scored[scored["eb_samples"] > 0]
    assert len(eb) == 1 and eb.iloc[0]["train"] == 3
    assert bool(eb.iloc[0]["anomaly"])


def test_rejects_wrong_file(tmp_path):
    bad = tmp_path / "x.csv"
    bad.write_text("a,b\n1,2\n")
    with pytest.raises(ValueError):
        load(bad)
