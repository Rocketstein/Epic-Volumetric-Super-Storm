import argparse
import base64
import http.client
import json
import sys


HOST = "127.0.0.1"
PORT = 8000
PATH = "/mcp"


def request(body, session_id=None, method="POST"):
    connection = http.client.HTTPConnection(HOST, PORT, timeout=60)
    headers = {
        "Content-Type": "application/json",
        "Accept": "application/json, text/event-stream",
    }
    if session_id:
        headers["Mcp-Session-Id"] = session_id
        headers["MCP-Protocol-Version"] = "2025-11-25"
    payload = json.dumps(body, separators=(",", ":")) if body is not None else None
    connection.request(method, PATH, body=payload, headers=headers)
    response = connection.getresponse()
    raw = response.read().decode("utf-8", errors="replace")
    response_headers = dict(response.getheaders())
    connection.close()
    if response.status >= 400:
        raise RuntimeError(f"HTTP {response.status}: {raw}")
    if not raw:
        return response_headers, None
    if "text/event-stream" in response_headers.get("content-type", ""):
        events = [
            json.loads(line[5:].strip())
            for line in raw.splitlines()
            if line.startswith("data:")
        ]
        return response_headers, events[-1] if events else None
    return response_headers, json.loads(raw)


def open_session():
    headers, result = request({
        "jsonrpc": "2.0",
        "id": 1,
        "method": "initialize",
        "params": {
            "protocolVersion": "2025-11-25",
            "clientInfo": {"name": "Codex", "version": "1.0"},
            "capabilities": {},
        },
    })
    session_id = headers.get("mcp-session-id") or headers.get("Mcp-Session-Id")
    if not session_id:
        raise RuntimeError(f"initialize returned no session id: {result}")
    request({"jsonrpc": "2.0", "method": "notifications/initialized"}, session_id)
    return session_id


def close_session(session_id):
    try:
        request(None, session_id, method="DELETE")
    except Exception:
        pass


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("name")
    parser.add_argument("arguments_base64")
    args = parser.parse_args()
    arguments = json.loads(base64.b64decode(args.arguments_base64).decode("utf-8"))
    session_id = open_session()
    try:
        _, result = request({
            "jsonrpc": "2.0",
            "id": 3,
            "method": "tools/call",
            "params": {"name": args.name, "arguments": arguments},
        }, session_id)
        json.dump(result, sys.stdout, ensure_ascii=False, indent=2)
        sys.stdout.write("\n")
    finally:
        close_session(session_id)


if __name__ == "__main__":
    main()
