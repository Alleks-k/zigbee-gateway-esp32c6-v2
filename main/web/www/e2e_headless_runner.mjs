import { createServer } from "node:http";
import { readFile } from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { chromium } from "playwright";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const rootDir = __dirname;
const host = "127.0.0.1";
const port = 4173;

const contentTypeByExt = {
    ".html": "text/html; charset=utf-8",
    ".js": "application/javascript; charset=utf-8",
    ".css": "text/css; charset=utf-8",
};

function resolveFile(urlPath) {
    const cleanPath = urlPath === "/" ? "/e2e_contract.html" : urlPath;
    const abs = path.normalize(path.join(rootDir, cleanPath));
    if (!abs.startsWith(rootDir)) {
        return null;
    }
    return abs;
}

async function startServer() {
    const server = createServer(async (req, res) => {
        try {
            const abs = resolveFile(req.url || "/");
            if (!abs) {
                res.writeHead(403);
                res.end("forbidden");
                return;
            }
            const data = await readFile(abs);
            const ext = path.extname(abs);
            res.writeHead(200, { "Content-Type": contentTypeByExt[ext] || "application/octet-stream" });
            res.end(data);
        } catch (_err) {
            res.writeHead(404);
            res.end("not found");
        }
    });

    await new Promise((resolve, reject) => {
        server.once("error", reject);
        server.listen(port, host, () => resolve());
    });
    return server;
}

function collectResults(results) {
    const fail = results.filter((line) => line.startsWith("FAIL:"));
    const done = results.some((line) => line.includes("All browser e2e contracts passed"));
    return { fail, done };
}

async function run() {
    const server = await startServer();
    const browser = await chromium.launch({ headless: true });

    try {
        const page = await browser.newPage();
        await page.goto(`http://${host}:${port}/e2e_contract.html`, { waitUntil: "load" });

        await page.waitForFunction(() => {
            const lines = Array.from(document.querySelectorAll("#results li"), (li) => li.textContent || "");
            return lines.some((line) => line.startsWith("FAIL:")) ||
                   lines.some((line) => line.includes("All browser e2e contracts passed"));
        }, { timeout: 30000 });

        const results = await page.evaluate(() => Array.from(
            document.querySelectorAll("#results li"),
            (li) => li.textContent || "",
        ));
        const { fail, done } = collectResults(results);
        if (!done || fail.length > 0) {
            throw new Error(`Browser contract failed. Results: ${results.join(" | ")}`);
        }

        console.log(`Browser contract passed (${results.length} checks).`);
    } finally {
        await browser.close();
        await new Promise((resolve) => server.close(() => resolve()));
    }
}

run().catch((err) => {
    console.error(err.message || String(err));
    process.exit(1);
});
