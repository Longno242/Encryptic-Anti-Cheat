#!/usr/bin/env python3
"""Encryptic development telemetry + heartbeat receiver."""

from http.server import BaseHTTPRequestHandler, HTTPServer
import json

API_KEY = "dev-key-change-me"
PORT = 8787


class Handler(BaseHTTPRequestHandler):
    def _auth(self) -> bool:
        return self.headers.get("X-Encryptic-Key") == API_KEY

    def do_POST(self):
        if not self._auth():
            self.send_response(401)
            self.end_headers()
            return

        length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(length).decode("utf-8")

        if self.path == "/v1/violations":
            print("[violation]", json.dumps(json.loads(body), indent=2))
        elif self.path == "/v1/heartbeat":
            print("[heartbeat]", json.dumps(json.loads(body), indent=2))
        else:
            self.send_response(404)
            self.end_headers()
            return

        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.end_headers()
        self.wfile.write(b'{"ok":true}')

    def log_message(self, fmt, *args):
        return


if __name__ == "__main__":
    print(f"Encryptic dev server http://127.0.0.1:{PORT}")
    HTTPServer(("127.0.0.1", PORT), Handler).serve_forever()
