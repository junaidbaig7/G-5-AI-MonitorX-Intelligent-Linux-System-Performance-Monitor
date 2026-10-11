#!/usr/bin/env python3
"""
AI MonitorX -- Flask API server.

The C program (ai_monitorx) is the only thing that touches /proc. Every
2 seconds it writes a JSON snapshot to /tmp/ai_monitorx_stats.json and
appends readings to logs/cpu.log and logs/memory.log. This server just
reads those files and serves them over HTTP so a web frontend (running
on the Mac, through UTM port forwarding) can display them.

Endpoints:
  GET /              -> the web dashboard (static/index.html)
  GET /api/health    -> is the server up, and is the C monitor running?
  GET /api/stats     -> latest snapshot (CPU, memory, predictions, top processes)
  GET /api/history   -> recent CPU/memory readings, for drawing graphs
                        optional ?limit=N (default 30, max 500)

Run from the project folder:
  python3 app.py
"""

import csv
import json
import os
import time

from flask import Flask, jsonify, request

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
STATS_FILE = "/tmp/ai_monitorx_stats.json"
CPU_LOG = os.path.join(BASE_DIR, "logs", "cpu.log")
MEM_LOG = os.path.join(BASE_DIR, "logs", "memory.log")

# The C program writes every 2 seconds, but a cycle can take longer
# while Python predictions run. If the snapshot is older than this,
# assume the C monitor has stopped.
STALE_AFTER_SEC = 10

app = Flask(__name__)


@app.after_request
def allow_cross_origin(response):
    # The frontend is opened on the Mac, so it's a different origin
    # from this server. Without this header the browser blocks fetch().
    response.headers["Access-Control-Allow-Origin"] = "*"
    return response


def read_snapshot():
    """Returns (data, error_message). Exactly one of them is None."""
    if not os.path.exists(STATS_FILE):
        return None, "No data yet. Start the C monitor: ./ai_monitorx"
    try:
        with open(STATS_FILE) as f:
            return json.load(f), None
    except (OSError, json.JSONDecodeError) as e:
        return None, "Could not read snapshot: {}".format(e)


def read_log_tail(path, limit):
    """Returns the last `limit` rows of a reading,usage CSV log."""
    if not os.path.exists(path):
        return []
    rows = []
    try:
        with open(path, newline="") as f:
            for row in csv.DictReader(f):
                try:
                    rows.append({
                        "reading": int(row["reading"]),
                        "usage": float(row["usage"]),
                    })
                except (KeyError, ValueError):
                    continue  # skip a half-written or malformed line
    except OSError:
        return []
    return rows[-limit:]


def monitor_is_running(snapshot):
    if snapshot is None:
        return False
    return (time.time() - snapshot.get("timestamp", 0)) <= STALE_AFTER_SEC


@app.route("/")
def dashboard():
    # The web dashboard (static/index.html + style.css + script.js).
    # Flask serves the /static/ folder automatically.
    return app.send_static_file("index.html")


@app.route("/api/health")
def health():
    snapshot, _ = read_snapshot()
    return jsonify({
        "server": "ok",
        "monitor_running": monitor_is_running(snapshot),
    })


@app.route("/api/stats")
def stats():
    snapshot, error = read_snapshot()
    if error:
        return jsonify({"error": error}), 503
    snapshot["monitor_running"] = monitor_is_running(snapshot)
    return jsonify(snapshot)


@app.route("/api/history")
def history():
    try:
        limit = int(request.args.get("limit", 30))
    except ValueError:
        limit = 30
    limit = max(1, min(limit, 500))

    return jsonify({
        "cpu": read_log_tail(CPU_LOG, limit),
        "memory": read_log_tail(MEM_LOG, limit),
    })


if __name__ == "__main__":
    # host="0.0.0.0" is required: the default (127.0.0.1) only accepts
    # connections from inside the VM, so the Mac couldn't reach it.
    app.run(host="0.0.0.0", port=5000, debug=False)
