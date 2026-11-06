"""Summarizes a run's event log: timings, usage, milestones, and an isolation audit.

    python summarize.py runs/<run>/events.jsonl
"""
import json
import re
import sys

SUSPECT = re.compile(
    r"egret|pyomo|pglib|noAML|mamba|anaconda|curl|wget|https?:|\bgit\b|\bpip\b|python|\.claude|/software/(?!gurobi)",
    re.IGNORECASE,
)


def main():
    records = [json.loads(line) for line in open(sys.argv[1])]
    if not records:
        print("No events recorded.")
        return
    start = records[0]["arrived"]
    calls = {}
    tool_counts = {}
    suspects = []
    milestones = {}
    errors = 0
    final = None
    last_source_edit = None

    for record in records:
        event = record["event"]
        minutes = (record["arrived"] - start) / 60
        if event.get("type") == "result":
            final = event
        message = event.get("message")
        content = message.get("content") if isinstance(message, dict) else None
        if not isinstance(content, list):
            continue
        for block in content:
            if block.get("type") == "tool_use":
                name, tool_input = block["name"], block.get("input", {})
                tool_counts[name] = tool_counts.get(name, 0) + 1
                text = tool_input.get("command") or tool_input.get("file_path") or json.dumps(tool_input)
                calls[block["id"]] = (name, text, minutes)
                if name in ("WebFetch", "WebSearch") or SUSPECT.search(text):
                    suspects.append(f"{minutes:7.1f} min  {name}: {text[:150]}")
            elif block.get("type") == "tool_result":
                failed = bool(block.get("is_error"))
                errors += failed
                name, text, _ = calls.get(block.get("tool_use_id"), ("?", "", 0))
                if name == "Bash" and not failed:
                    if "cmake --build" in text or re.search(r"\bmake\b", text):
                        milestones.setdefault("first successful build", minutes)
                    if "uc_solve" in text and ("data/" in text or ".json" in text):
                        milestones.setdefault("first completed solve", minutes)
                if name in ("Write", "Edit") and not failed and text.endswith((".c", ".h", "CMakeLists.txt")):
                    last_source_edit = minutes
                if name == "Write" and not failed and text.endswith(("NOTES.md", "RESULTS.md")):
                    milestones.setdefault(f"first wrote {text.rsplit('/', 1)[-1]}", minutes)

    print(f"events: {len(records)}")
    print(f"event_stream_minutes: {(records[-1]['arrived'] - start) / 60:.2f}")
    if final:
        for key in ("subtype", "is_error", "num_turns", "duration_ms", "duration_api_ms", "total_cost_usd"):
            print(f"{key}: {final.get(key)}")
        print(f"model_usage: {json.dumps(final.get('modelUsage', final.get('model_usage')))}")
        denials = final.get("permission_denials") or []
        print(f"permission_denials: {len(denials)}")
        for denial in denials:
            print("  ", json.dumps(denial)[:200])
    else:
        print("No final result event: the run ended abnormally or hit the time limit.")
    print(f"tool_calls: {json.dumps(tool_counts, sort_keys=True)}")
    print(f"tool_errors: {errors}")
    for label, minutes in milestones.items():
        print(f"milestone: {label} at {minutes:.1f} min")
    if last_source_edit is not None:
        print(f"translation_minutes (start to last edit of the source code): {last_source_edit:.1f}")
    print(f"isolation_audit: {len(suspects)} tool calls to review")
    for line in suspects:
        print("  ", line)


if __name__ == "__main__":
    main()
