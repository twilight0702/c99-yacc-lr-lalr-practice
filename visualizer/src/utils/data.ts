import type { Manifest, StepData } from "../types";

const DEFAULT_CASE = "c99";
const DATA_ROOT = "/data/v1";
let cachedCaseId: string | null = null;

function resolveCaseIdFromQuery(): string | null {
  const query = new URLSearchParams(window.location.search);
  const caseId = query.get("case");
  return caseId && caseId.trim() ? caseId.trim() : null;
}

async function resolveLatestCaseId(): Promise<string | null> {
  try {
    const res = await fetch(`${DATA_ROOT}/latest.json`, { cache: "no-store" });
    if (!res.ok) {
      return null;
    }
    const payload = (await res.json()) as { case_id?: unknown };
    if (typeof payload.case_id === "string" && payload.case_id.trim()) {
      return payload.case_id.trim();
    }
  } catch {
    return null;
  }
  return null;
}

async function resolveCaseId(): Promise<string> {
  if (cachedCaseId) {
    return cachedCaseId;
  }
  const fromQuery = resolveCaseIdFromQuery();
  if (fromQuery) {
    cachedCaseId = fromQuery;
    return fromQuery;
  }
  const latestCase = await resolveLatestCaseId();
  cachedCaseId = latestCase ?? DEFAULT_CASE;
  return cachedCaseId;
}

export function getCaseId(): string {
  return cachedCaseId ?? resolveCaseIdFromQuery() ?? DEFAULT_CASE;
}

function previewBody(body: string): string {
  return body.replace(/\s+/g, " ").trim().slice(0, 160);
}

async function fetchJson<T>(url: string, label: string): Promise<T> {
  const res = await fetch(url, { cache: "no-store" });
  const text = await res.text();
  if (!res.ok) {
    throw new Error(`${label}失败: HTTP ${res.status} ${res.statusText} (${url})`);
  }
  const contentType = (res.headers.get("content-type") || "").toLowerCase();
  if (!contentType.includes("application/json")) {
    throw new Error(
      `${label}失败: 响应不是 JSON (content-type=${contentType || "unknown"})，可能路径不存在或被重定向到 HTML。响应片段: ${previewBody(text)}`
    );
  }
  try {
    return JSON.parse(text) as T;
  } catch (error) {
    const detail = error instanceof Error ? error.message : String(error);
    throw new Error(`${label}失败: JSON 解析错误 (${detail})，响应片段: ${previewBody(text)}`);
  }
}

export async function loadManifest(): Promise<Manifest> {
  const caseId = await resolveCaseId();
  return await fetchJson<Manifest>(`${DATA_ROOT}/${caseId}/manifest.json`, "加载 manifest ");
}

export async function loadStepData(manifest: Manifest, step: number): Promise<StepData> {
  const stepInfo = manifest.steps[String(step)];
  if (!stepInfo) {
    throw new Error(`manifest 未包含 step${step}`);
  }
  const caseId = manifest.case_id;
  const dataPath = stepInfo.files.data;
  return await fetchJson<StepData>(`${DATA_ROOT}/${caseId}/${dataPath}`, `加载 step${step} 数据`);
}

export async function loadCaseText(caseId: string, filename: string): Promise<string> {
  const url = `${DATA_ROOT}/${caseId}/${filename}`;
  const res = await fetch(url, { cache: "no-store" });
  const text = await res.text();
  if (!res.ok) {
    throw new Error(`加载 ${filename} 失败: HTTP ${res.status} ${res.statusText} (${url})`);
  }
  const contentType = (res.headers.get("content-type") || "").toLowerCase();
  if (contentType.includes("text/html")) {
    throw new Error(`加载 ${filename} 失败: 收到 HTML 响应，路径可能不存在 (${url})`);
  }
  return text;
}

export function downloadTextFile(filename: string, content: string): void {
  const blob = new Blob([content], { type: "text/plain;charset=utf-8" });
  const url = URL.createObjectURL(blob);
  const a = document.createElement("a");
  a.href = url;
  a.download = filename;
  a.click();
  URL.revokeObjectURL(url);
}

export function downloadJsonFile(filename: string, payload: unknown): void {
  downloadTextFile(filename, JSON.stringify(payload, null, 2));
}
