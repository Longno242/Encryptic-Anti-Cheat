const http = require("http");

const PORT = Number(process.env.ENCRYPTIC_PORT || 8787);
const API_KEY = "dev-key-change-me";

function readBody(req) {
  return new Promise((resolve) => {
    let data = "";
    req.on("data", (chunk) => (data += chunk));
    req.on("end", () => resolve(data));
  });
}

const server = http.createServer(async (req, res) => {
  if (req.headers["x-encryptic-key"] !== API_KEY) {
    res.writeHead(401);
    return res.end("unauthorized");
  }

  const body = await readBody(req);

  if (req.url === "/v1/violations" && req.method === "POST") {
    console.log("[violation]", body);
    res.writeHead(200, { "Content-Type": "application/json" });
    return res.end('{"ok":true}');
  }

  if (req.url === "/v1/heartbeat" && req.method === "POST") {
    console.log("[heartbeat]", body);
    res.writeHead(200, { "Content-Type": "application/json" });
    return res.end('{"ok":true,"session_valid":true}');
  }

  res.writeHead(404);
  res.end("not found");
});

server.on("error", (err) => {
  if (err.code === "EADDRINUSE") {
    console.log(`Encryptic telemetry server already running on port ${PORT}.`);
    console.log("Close the other window or stop the process using that port.");
    process.exit(0);
  }
  console.error(err);
  process.exit(1);
});

server.listen(PORT, () => {
  console.log(`Encryptic dev server http://127.0.0.1:${PORT}`);
  console.log("Set encryptic_config.json telemetry/heartbeat URLs to this host.");
});
