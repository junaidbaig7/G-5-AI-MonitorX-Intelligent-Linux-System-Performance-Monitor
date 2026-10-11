#!/usr/bin/env python3
"""
AI MonitorX -- resource usage predictor.

Reads a CSV log of past readings (columns: reading, usage) and fits a
linear regression to predict the next reading. This is the ML
enhancement described in the project brief -- the systems-programming
core (reading /proc, threads, signals) lives entirely in the C
program; this script only ever sees numbers the C program already
measured and logged.

Two output modes:
  (default)   A human-readable report, meant to be run by hand:
                  python3 ai_prediction.py cpu
                  python3 ai_prediction.py memory
  --for-c     A single compact line the C program parses with sscanf:
                  current,predicted,threshold,trend,over_threshold
              e.g. "76.00,84.00,80.00,increasing,1"
"""

import sys
import os
import argparse
import pandas as pd
from sklearn.linear_model import LinearRegression

CPU_THRESHOLD = 80.0
MEMORY_THRESHOLD = 85.0

# A predicted change smaller than this (in percentage points) counts
# as "stable" rather than up/down, so tiny noise doesn't flip the label.
TREND_EPSILON = 1.0


def predict_usage(filename, threshold):
    """
    Returns a dict with current/predicted/threshold/trend/over_threshold,
    or None if the log doesn't exist yet or doesn't have enough rows.
    """
    if not os.path.exists(filename):
        return None

    data = pd.read_csv(filename)

    if len(data) < 5:
        return None

    X = data[["reading"]]
    y = data["usage"]

    model = LinearRegression()
    model.fit(X, y)

    next_reading = pd.DataFrame({"reading": [len(data) + 1]})
    predicted_value = float(model.predict(next_reading)[0])

    # Usage is a percentage. A steep recent ramp can make the raw
    # extrapolation shoot past 100% (or below 0%), which is
    # meaningless here, so clamp it.
    predicted_value = max(0.0, min(100.0, predicted_value))

    current_value = float(y.iloc[-1])
    diff = predicted_value - current_value

    if diff > TREND_EPSILON:
        trend = "increasing"
    elif diff < -TREND_EPSILON:
        trend = "decreasing"
    else:
        trend = "stable"

    return {
        "current": current_value,
        "predicted": predicted_value,
        "threshold": threshold,
        "trend": trend,
        "over_threshold": predicted_value >= threshold,
    }


def print_human_readable(result):
    print()
    print("--------------------------------------")
    print("AI RESOURCE PREDICTION")
    print("--------------------------------------")
    print("Current Usage   : {:.2f}%".format(result["current"]))
    print("Predicted Usage : {:.2f}%".format(result["predicted"]))
    print("Threshold       : {:.2f}%".format(result["threshold"]))
    print("Trend           : {}".format(result["trend"].capitalize()))

    if result["over_threshold"]:
        print("Status          : WARNING")
        print("⚠ High resource usage expected!")
    else:
        print("Status          : NORMAL")
        print("✓ Resource usage expected to remain normal.")
    print("--------------------------------------")


def print_for_c(result):
    """Compact, fixed-order line the C program parses with sscanf."""
    print("{:.2f},{:.2f},{:.2f},{},{}".format(
        result["current"],
        result["predicted"],
        result["threshold"],
        result["trend"],
        1 if result["over_threshold"] else 0,
    ))


def main():
    parser = argparse.ArgumentParser(description="AI MonitorX resource predictor")
    parser.add_argument("resource", choices=["cpu", "memory"])
    parser.add_argument(
        "--for-c",
        action="store_true",
        help="print a compact machine-readable line instead of the human-readable report",
    )
    args = parser.parse_args()

    if args.resource == "cpu":
        filename, threshold = "logs/cpu.log", CPU_THRESHOLD
    else:
        filename, threshold = "logs/memory.log", MEMORY_THRESHOLD

    result = predict_usage(filename, threshold)

    if result is None:
        if args.for_c:
            print("NA")
        else:
            print("Not enough data for prediction yet (need at least 5 readings).")
        sys.exit(0)

    if args.for_c:
        print_for_c(result)
    else:
        print_human_readable(result)


if __name__ == "__main__":
    main()
