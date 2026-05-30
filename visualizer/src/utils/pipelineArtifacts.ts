import type { Manifest } from "../types";
import { loadCaseText } from "./data";

export interface ResolvedArtifact {
  code: string;
  label: string;
  note?: string;
}

export function countSourceLines(code: string): number {
  if (!code) {
    return 0;
  }
  return code.split("\n").length;
}

export function countNonEmptyLines(code: string): number {
  if (!code) {
    return 0;
  }
  return code.split("\n").filter((line) => line.trim().length > 0).length;
}

export async function loadLexArtifacts(manifest: Manifest): Promise<{
  lexSpec: ResolvedArtifact;
  lexInput: ResolvedArtifact;
}> {
  const caseId = manifest.case_id;
  const lexFilename = manifest.assets?.lex ?? "lexer.l";
  const inputFilename = manifest.assets?.input ?? "input.c";

  const [lexCode, inputCode] = await Promise.all([
    loadCaseText(caseId, lexFilename),
    loadCaseText(caseId, inputFilename)
  ]);

  return {
    lexSpec: {
      code: lexCode,
      label: lexFilename,
      note: "从当前 case 数据目录直接读取"
    },
    lexInput: {
      code: inputCode,
      label: inputFilename,
      note: "从当前 case 数据目录直接读取"
    }
  };
}

export async function loadJimpleArtifact(manifest: Manifest): Promise<ResolvedArtifact> {
  const caseId = manifest.case_id;
  const jimpleFilename = manifest.assets?.jimple ?? "output.jimple";
  const jimpleCode = await loadCaseText(caseId, jimpleFilename);
  return {
    code: jimpleCode,
    label: jimpleFilename,
    note: "从当前 case 数据目录直接读取"
  };
}
