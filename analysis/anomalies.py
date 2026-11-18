"""Data mining on simulator logs: builds features for every train on every 1 km segment between stations,
then uses an Isolation Forest (unsupervised machine learning) to flag segments that ran unusually,
e.g. an emergency brake after a radio loss, or a train repeatedly held back by the train ahead.
Also draws the time-distance graph (train graph) that rail planners use.

Usage: python analysis/anomalies.py run.csv --out analysis_out
"""
from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np
import pandas as pd
from sklearn.ensemble import IsolationForest
from sklearn.preprocessing import StandardScaler

FEATURES = ["run_time_s", "stops", "brake_share", "min_moving_kmh", "eb_samples"]


def load(path: str | Path) -> pd.DataFrame:
    df = pd.read_csv(path)
    expected = {"t_s", "train", "position_m", "speed_mps", "authority_m", "mode"}
    missing = expected - set(df.columns)
    if missing:
        raise ValueError(f"not a cbtc_sim log, missing columns: {sorted(missing)}")
    return df.sort_values(["train", "t_s"]).reset_index(drop=True)


def segment_features(df: pd.DataFrame, spacing_m: float = 1000.0) -> pd.DataFrame:
    df = df.copy()
    df["segment"] = (df["position_m"] // spacing_m).astype(int)
    moving = df[df["mode"] != "DWELL"]
    rows = []
    for (train, seg), g in moving.groupby(["train", "segment"]):
        if len(g) < 5:
            continue
        stopped = (g["speed_mps"] == 0) & (g["mode"].isin(["BRAKE", "EB"]))
        stops = int((stopped & ~stopped.shift(fill_value=False)).sum())
        moving_speeds = g.loc[g["speed_mps"] > 0.5, "speed_mps"]
        rows.append({
            "train": train, "segment": seg,
            "run_time_s": g["t_s"].max() - g["t_s"].min(),
            "stops": stops,
            "brake_share": float(g["mode"].isin(["BRAKE", "EB"]).mean()),
            "min_moving_kmh": float(moving_speeds.min() * 3.6) if len(moving_speeds) else 0.0,
            "eb_samples": int((g["mode"] == "EB").sum()),
        })
    return pd.DataFrame(rows)


def flag_anomalies(feats: pd.DataFrame, contamination: float = 0.08, seed: int = 7) -> pd.DataFrame:
    x = StandardScaler().fit_transform(feats[FEATURES].to_numpy(dtype=float))
    model = IsolationForest(n_estimators=300, contamination=contamination, random_state=seed).fit(x)
    out = feats.copy()
    out["anomaly_score"] = -model.score_samples(x)       # higher = more unusual
    out["anomaly"] = model.predict(x) == -1
    return out.sort_values("anomaly_score", ascending=False).reset_index(drop=True)


def train_graph(df: pd.DataFrame, path: Path) -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, ax = plt.subplots(figsize=(9, 5))
    for train, g in df.groupby("train"):
        ax.plot(g["t_s"] / 60, g["position_m"] / 1000, lw=1.4, label=f"Train {train}")
        eb = g[g["mode"] == "EB"]
        if len(eb):
            ax.scatter(eb["t_s"] / 60, eb["position_m"] / 1000, color="#b91c1c", s=10, zorder=3)
    ax.set_xlabel("Time (minutes)")
    ax.set_ylabel("Position (km)")
    ax.set_title("Time-distance graph (red = emergency brake)")
    ax.grid(alpha=0.3)
    ax.legend(frameon=False)
    fig.tight_layout()
    fig.savefig(path, dpi=130)
    plt.close(fig)


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("log")
    p.add_argument("--out", default="analysis_out")
    a = p.parse_args()
    out = Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    df = load(a.log)
    scored = flag_anomalies(segment_features(df))
    scored.to_csv(out / "segment_anomalies.csv", index=False)
    train_graph(df, out / "train_graph.png")
    pd.set_option("display.width", 140)
    print(f"{len(scored)} train-segments, {int(scored['anomaly'].sum())} flagged as unusual")
    print(scored.head(8).round(2).to_string(index=False))


if __name__ == "__main__":
    np.set_printoptions(suppress=True)
    main()
